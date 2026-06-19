// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3Catalog implementation.

#include "config.h"

#include <cstdio>
#include <sstream>
#include <vector>

#include <TheBESKeys.h>
#include <BESUtil.h>
#include <BESCatalogUtils.h>
#include <BESNotFoundError.h>
#include <BESInternalError.h>
#include <BESIndent.h>
#include "CatalogNode.h"
#include "CatalogItem.h"

#include "H5S3Catalog.h"
#include "H5S3Names.h"

using namespace std;

namespace h5s3 {

// One row read back from index.parquet.
struct Row { string key; size_t size = 0; string mtime; };

H5S3Catalog::H5S3Catalog(const string &catalog_name) : BESCatalog(catalog_name)
{
    bool found = false;
    string dir;
    TheBESKeys::TheKeys()->get_value(H5S3_INDEX_DIR_KEY, dir, found);
    if (dir.empty())
        throw BESInternalError(string("h5s3 catalog: ") + H5S3_INDEX_DIR_KEY + " is not set", __FILE__, __LINE__);
    while (!dir.empty() && dir.back() == '/') dir.pop_back();
    d_index_path = dir + "/" + H5S3_INDEX_NAME;

    found = false;
    TheBESKeys::TheKeys()->get_value(H5S3_INDEX_BIN_KEY, d_index_bin, found);
    if (d_index_bin.empty())
        throw BESInternalError(string("h5s3 catalog: ") + H5S3_INDEX_BIN_KEY + " is not set", __FILE__, __LINE__);
}

// Read index.parquet by running:  <index_bin> read <index_path>
// which prints "key\tsize\tmtime" lines. Done out-of-process because the
// Arrow/Parquet stack cannot coexist with the AWS SDK in the BES process.
static vector<Row> read_index(const string &index_bin, const string &index_path)
{
    vector<Row> rows;
    string cmd = "'" + index_bin + "' read '" + index_path + "' 2>/dev/null";
    FILE *p = popen(cmd.c_str(), "r");
    if (!p)
        return rows;
    char line[4096];
    while (fgets(line, sizeof(line), p)) {
        string s(line);
        if (!s.empty() && s.back() == '\n') s.pop_back();
        if (s.empty()) continue;
        istringstream ls(s);
        Row r; string size_s;
        getline(ls, r.key, '\t');
        getline(ls, size_s, '\t');
        getline(ls, r.mtime, '\t');
        try { r.size = size_s.empty() ? 0 : (size_t)stoull(size_s); } catch (...) { r.size = 0; }
        if (!r.key.empty()) rows.push_back(std::move(r));
    }
    pclose(p);
    return rows;
}

string H5S3Catalog::get_root() const
{
    return d_index_path;
}

BESCatalogEntry *H5S3Catalog::show_catalog(const string & /*container*/, BESCatalogEntry *entry)
{
    return entry; // deprecated interface
}

bes::CatalogNode *H5S3Catalog::get_node(const string &path) const
{
    vector<Row> rows = read_index(d_index_bin, d_index_path);

    // Normalize the requested path to a bare key (no leading '/').
    string want = path;
    while (!want.empty() && want.front() == '/') want.erase(want.begin());

    const BESCatalogUtils *utils = get_catalog_utils();

    // Root: list every HDF5 file in the index as a leaf.
    if (want.empty() || want == "/") {
        auto *node = new bes::CatalogNode("/");
        node->set_catalog_name(get_catalog_name());
        node->set_lmt(BESUtil::get_time());
        for (const auto &r : rows) {
            bool is_data = utils ? utils->is_data(r.key) : true;
            auto *ci = new bes::CatalogItem(r.key, r.size, r.mtime, is_data, bes::CatalogItem::leaf);
            node->add_leaf(ci);
        }
        return node;
    }

    // A specific file: return a single-leaf node if it's in the index.
    for (const auto &r : rows) {
        if (r.key == want) {
            bool is_data = utils ? utils->is_data(r.key) : true;
            auto *leaf = new bes::CatalogItem(r.key, r.size, r.mtime, is_data, bes::CatalogItem::leaf);
            auto *node = new bes::CatalogNode(path);
            node->set_catalog_name(get_catalog_name());
            node->set_lmt(BESUtil::get_time());
            node->set_leaf(leaf);
            return node;
        }
    }

    throw BESNotFoundError("h5s3: '" + path + "' is not in the index", __FILE__, __LINE__);
}

void H5S3Catalog::get_site_map(const string & /*prefix*/, const string & /*node_suffix*/,
                               const string &leaf_suffix, ostream &out, const string &path) const
{
    for (const auto &r : read_index(d_index_bin, d_index_path))
        out << path << r.key << leaf_suffix << "\n";
}

void H5S3Catalog::dump(ostream &strm) const
{
    strm << BESIndent::LMarg << "H5S3Catalog::dump - (" << (void *) this << ") index=" << d_index_path << endl;
}

} // namespace h5s3
