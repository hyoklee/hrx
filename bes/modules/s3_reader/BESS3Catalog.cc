// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of S3_module, A C++ module that can be loaded in to
// the OPeNDAP Back-End Server (BES) and is able to handle remote requests.

// Copyright (c) 2026 OPeNDAP, Inc.
// Author: OPeNDAP
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

#include "config.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <BESCatalogUtils.h>
#include <BESDebug.h>
#include <BESInternalError.h>
#include <BESLog.h>
#include <BESUtil.h>
#include <TheBESKeys.h>

#include "CatalogItem.h"
#include "CatalogNode.h"

#include "BESS3Catalog.h"
#include "S3Names.h"

using namespace std;
using namespace bes;

static const string S3_CATALOG_MODULE = "s3";

// ============================================================
// Static member definitions
// ============================================================

bool                       BESS3Catalog::d_use_cache         = true;
ngap::MemoryCache<string>  BESS3Catalog::d_mem_cache;
FileCache                  BESS3Catalog::d_file_cache;
int                        BESS3Catalog::d_mem_cache_size     = 100;
int                        BESS3Catalog::d_mem_cache_purge    = 20;
long long                  BESS3Catalog::d_file_cache_size_bytes  = 100LL * ::MEGABYTE;
long long                  BESS3Catalog::d_file_cache_purge_bytes =  20LL * ::MEGABYTE;
string                     BESS3Catalog::d_file_cache_dir     = "/tmp/hyrax_s3_catalog_cache";

// ============================================================
// Constructor — read config, initialise caches once
// ============================================================

BESS3Catalog::BESS3Catalog(const string &catalog_name)
    : BESCatalog(catalog_name) {

    // Required: bucket name
    d_bucket = TheBESKeys::read_string_key(S3_CATALOG_BUCKET_KEY, "");
    if (d_bucket.empty()) {
        throw BESInternalError("S3 Catalog: " + string(S3_CATALOG_BUCKET_KEY)
                               + " is not set in configuration.", __FILE__, __LINE__);
    }

    // Optional connection config
    d_endpoint = TheBESKeys::read_string_key(S3_CATALOG_ENDPOINT_KEY, "");
    d_region   = TheBESKeys::read_string_key(S3_CATALOG_REGION_KEY,   "us-east-1");
    d_aws_key  = TheBESKeys::read_string_key(S3_CATALOG_AWS_KEY_ID,   "");
    d_aws_secret = TheBESKeys::read_string_key(S3_CATALOG_AWS_SECRET_KEY, "");

    // Cache config (only read once; static members shared across instances)
    d_use_cache = TheBESKeys::read_bool_key(S3_CATALOG_USE_CACHE_KEY, true);
    if (d_use_cache) {
        d_mem_cache_size  = TheBESKeys::read_int_key(S3_CATALOG_MEM_CACHE_SIZE_KEY,  d_mem_cache_size);
        d_mem_cache_purge = TheBESKeys::read_int_key(S3_CATALOG_MEM_CACHE_PURGE_KEY, d_mem_cache_purge);

        if (!d_mem_cache.initialize(d_mem_cache_size, d_mem_cache_purge)) {
            ERROR_LOG("BESS3Catalog: failed to initialise memory cache\n");
        }

        long long size_mb  = TheBESKeys::read_ulong_key(S3_CATALOG_FILE_CACHE_SIZE_KEY,
                                                         d_file_cache_size_bytes / ::MEGABYTE);
        long long purge_mb = TheBESKeys::read_ulong_key(S3_CATALOG_FILE_CACHE_PURGE_KEY,
                                                         d_file_cache_purge_bytes / ::MEGABYTE);
        d_file_cache_dir   = TheBESKeys::read_string_key(S3_CATALOG_FILE_CACHE_DIR_KEY, d_file_cache_dir);
        d_file_cache_size_bytes  = size_mb  * (long long)::MEGABYTE;
        d_file_cache_purge_bytes = purge_mb * (long long)::MEGABYTE;

        BESUtil::mkdir_p(d_file_cache_dir, 0775);
        if (!d_file_cache.initialize(d_file_cache_dir, d_file_cache_size_bytes, d_file_cache_purge_bytes)) {
            ERROR_LOG("BESS3Catalog: failed to initialise file cache at " + d_file_cache_dir + "\n");
        }
    }
}

// ============================================================
// Private helpers
// ============================================================

/**
 * Lazily initialise the AWS S3 client on first use.
 */
void BESS3Catalog::init_aws_client() const {
    if (d_aws_initialized) return;
    d_aws.initialize_s3_client(d_region, d_aws_key, d_aws_secret, d_endpoint);
    d_aws_initialized = true;
}

/**
 * Convert a catalog path (e.g. "/subdir/") to an S3 key prefix ("subdir/").
 * The root path "/" maps to "" (list the whole bucket root).
 */
string BESS3Catalog::path_to_prefix(const string &path) {
    if (path.empty() || path == "/") return "";
    // Strip leading '/'
    string prefix = (path[0] == '/') ? path.substr(1) : path;
    // Ensure trailing '/' so ListObjectsV2 treats it as a directory
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';
    return prefix;
}

string BESS3Catalog::make_cache_key(const string &bucket, const string &prefix) {
    return bucket + ":" + prefix;
}

/**
 * Serialise a listing as newline-delimited records: key|size|lmt|is_prefix
 */
string BESS3Catalog::serialize(const vector<S3ObjectInfo> &items) {
    ostringstream oss;
    for (const auto &item : items) {
        oss << item.key << '|' << item.size << '|' << item.last_modified
            << '|' << (item.is_prefix ? '1' : '0') << '\n';
    }
    return oss.str();
}

/**
 * Deserialise the newline-delimited format produced by serialize().
 */
vector<S3ObjectInfo> BESS3Catalog::deserialize(const string &data) {
    vector<S3ObjectInfo> items;
    istringstream iss(data);
    string line;
    while (getline(iss, line)) {
        if (line.empty()) continue;
        // Parse: key|size|lmt|is_prefix
        size_t p1 = line.find('|');
        if (p1 == string::npos) continue;
        size_t p2 = line.find('|', p1 + 1);
        if (p2 == string::npos) continue;
        size_t p3 = line.find('|', p2 + 1);
        if (p3 == string::npos) continue;

        S3ObjectInfo info;
        info.key          = line.substr(0, p1);
        info.size         = static_cast<size_t>(stoull(line.substr(p1 + 1, p2 - p1 - 1)));
        info.last_modified = line.substr(p2 + 1, p3 - p2 - 1);
        info.is_prefix    = (line[p3 + 1] == '1');
        items.push_back(move(info));
    }
    return items;
}

/**
 * Fetch the object listing for an S3 prefix, consulting the two-tier cache first.
 */
vector<S3ObjectInfo> BESS3Catalog::get_listing(const string &s3_prefix) const {
    const string cache_key = make_cache_key(d_bucket, s3_prefix);

    if (d_use_cache) {
        // Tier 1: memory cache
        string cached_data;
        if (d_mem_cache.get(cache_key, cached_data)) {
            BESDEBUG(S3_CATALOG_MODULE, "BESS3Catalog: memory cache hit for " << cache_key << endl);
            return deserialize(cached_data);
        }

        // Tier 2: file cache
        FileCache::Item item;
        if (d_file_cache.get(FileCache::hash_key(cache_key), item)) {
            BESDEBUG(S3_CATALOG_MODULE, "BESS3Catalog: file cache hit for " << cache_key << endl);
            // Read file content into a string
            string file_data;
            char buf[4096];
            ssize_t n;
            while ((n = read(item.get_fd(), buf, sizeof(buf))) > 0)
                file_data.append(buf, n);

            d_mem_cache.put(cache_key, file_data);
            return deserialize(file_data);
        }
    }

    // Cache miss — call S3
    BESDEBUG(S3_CATALOG_MODULE, "BESS3Catalog: S3 ListObjectsV2 for bucket=" << d_bucket
             << " prefix=" << s3_prefix << endl);
    init_aws_client();
    auto items = d_aws.s3_list_objects(d_bucket, s3_prefix, "/");

    if (!d_aws.get_aws_exception_name().empty()) {
        ERROR_LOG("BESS3Catalog: S3 error listing " + d_bucket + "/" + s3_prefix
                  + ": " + d_aws.get_aws_exception_message() + "\n");
    }

    if (d_use_cache && !items.empty()) {
        const string serialized = serialize(items);
        d_file_cache.put_data(FileCache::hash_key(cache_key), serialized);
        d_file_cache.purge();
        d_mem_cache.put(cache_key, serialized);
    }

    return items;
}

/**
 * Build a CatalogNode from a listing, using BESCatalogUtils to decide what is "data".
 * S3 key prefixes become nodes; real objects become leaves.
 * The item name is the last path component (not the full key).
 */
bes::CatalogNode *BESS3Catalog::build_node(const string &path,
                                            const vector<S3ObjectInfo> &items) const {
    auto *node = new bes::CatalogNode(path);
    node->set_catalog_name(get_catalog_name());
    node->set_lmt(BESUtil::get_time());

    const BESCatalogUtils *utils = get_catalog_utils();

    for (const auto &item : items) {
        if (item.is_prefix) {
            // Strip the trailing '/' and leading prefix to get the display name
            string name = item.key;
            if (!name.empty() && name.back() == '/') name.pop_back();
            size_t slash = name.rfind('/');
            if (slash != string::npos) name = name.substr(slash + 1);

            auto *ci = new bes::CatalogItem(name, 0, BESUtil::get_time(), bes::CatalogItem::node);
            node->add_node(ci);
        } else {
            // Object — display name is last path component
            size_t slash = item.key.rfind('/');
            string name = (slash != string::npos) ? item.key.substr(slash + 1) : item.key;
            if (name.empty()) continue;  // skip prefix-itself entry

            bool is_data = utils ? utils->is_data(name) : false;
            auto *ci = new bes::CatalogItem(name, item.size, item.last_modified,
                                             is_data, bes::CatalogItem::leaf);
            node->add_leaf(ci);
        }
    }

    return node;
}

// ============================================================
// BESCatalog interface
// ============================================================

string BESS3Catalog::get_root() const {
    return "s3://" + d_bucket + "/";
}

/**
 * @deprecated — required by BESCatalog interface; returns nullptr.
 */
BESCatalogEntry *BESS3Catalog::show_catalog(const string & /*container*/, BESCatalogEntry *entry) {
    return entry;
}

/**
 * Return catalog node for the given path, fetching from S3 (or cache).
 *
 * @param path  Catalog path, e.g. "/" for root, "/subdir/" for a sub-prefix.
 */
bes::CatalogNode *BESS3Catalog::get_node(const string &path) const {
    const string s3_prefix = path_to_prefix(path);
    auto items = get_listing(s3_prefix);
    return build_node(path, items);
}

/**
 * Write a flat site map of all reachable objects under 'path' to 'out'.
 * Recursively descends into common prefixes.
 */
void BESS3Catalog::get_site_map(const string &url_prefix, const string &node_suffix,
                                 const string &leaf_suffix, ostream &out,
                                 const string &path) const {
    const string s3_prefix = path_to_prefix(path);
    auto items = get_listing(s3_prefix);

    for (const auto &item : items) {
        if (item.is_prefix) {
            string name = item.key;
            if (!name.empty() && name.back() == '/') name.pop_back();
            size_t slash = name.rfind('/');
            string rel = (slash != string::npos) ? name.substr(slash + 1) : name;
            const string child_path = path + rel + "/";
            out << url_prefix << child_path << node_suffix << '\n';
            get_site_map(url_prefix, node_suffix, leaf_suffix, out, child_path);
        } else {
            size_t slash = item.key.rfind('/');
            string name = (slash != string::npos) ? item.key.substr(slash + 1) : item.key;
            if (name.empty()) continue;
            out << url_prefix << path << name << leaf_suffix << '\n';
        }
    }
}

void BESS3Catalog::dump(ostream &strm) const {
    strm << BESIndent::LMarg << "BESS3Catalog::dump - (" << (void *)this << ")\n";
    BESIndent::Indent();
    strm << BESIndent::LMarg << "catalog name: " << get_catalog_name() << "\n";
    strm << BESIndent::LMarg << "bucket:       " << d_bucket << "\n";
    strm << BESIndent::LMarg << "endpoint:     " << (d_endpoint.empty() ? "(AWS default)" : d_endpoint) << "\n";
    strm << BESIndent::LMarg << "region:       " << d_region << "\n";
    strm << BESIndent::LMarg << "use cache:    " << (d_use_cache ? "yes" : "no") << "\n";
    BESIndent::UnIndent();
}
