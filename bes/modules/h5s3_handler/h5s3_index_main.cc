// -*- mode: c++; c-basic-offset:4 -*-
//
// h5s3_index: turn a TSV listing (key\tsize\tlast_modified, as produced by
// h5s3_list) on stdin into index.parquet, or read an existing index back.
// Links only the Arrow/Parquet stack -- never the AWS C++ SDK -- so the two
// incompatible AWS SDK builds never coexist in one process.
//
//   h5s3_index write <index.parquet>     read TSV from stdin, write parquet
//   h5s3_index read  <index.parquet>     print parquet rows as TSV

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "H5S3Index.h"

using namespace std;

int main(int argc, char **argv)
{
    if (argc < 3) {
        cerr << "usage: h5s3_index write|read <index.parquet>\n";
        return 1;
    }
    string cmd = argv[1];
    string path = argv[2];

    try {
        if (cmd == "write") {
            vector<h5s3::IndexEntry> entries;
            string line;
            while (getline(cin, line)) {
                if (line.empty()) continue;
                istringstream ls(line);
                h5s3::IndexEntry e;
                string size_s;
                getline(ls, e.key, '\t');
                getline(ls, size_s, '\t');
                getline(ls, e.last_modified, '\t');
                e.size = size_s.empty() ? 0 : stoull(size_s);
                entries.push_back(std::move(e));
            }
            h5s3::H5S3Index::write(path, entries);
            cerr << "wrote " << entries.size() << " entries to " << path << "\n";
        }
        else if (cmd == "read") {
            for (const auto &e : h5s3::H5S3Index::read(path))
                cout << e.key << "\t" << e.size << "\t" << e.last_modified << "\n";
        }
        else {
            cerr << "unknown command: " << cmd << "\n";
            return 1;
        }
    } catch (const exception &e) {
        cerr << "error: " << e.what() << "\n";
        return 2;
    }
    return 0;
}
