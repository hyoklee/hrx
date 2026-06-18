// -*- mode: c++; c-basic-offset:4 -*-
//
// This file is part of h5s3_handler, a BES module that serves HDF5 files
// stored in S3 through the HDF5 ROS3 virtual file driver.
//
// H5S3Index: read/write the cached file listing as an Apache Parquet file
// (index.parquet). The index columns are: key, size, last_modified.

#ifndef H5S3_INDEX_H
#define H5S3_INDEX_H

#include <cstdint>
#include <string>
#include <vector>

namespace h5s3 {

/// One row of the index: an HDF5 object in the S3 bucket.
struct IndexEntry {
    std::string key;            ///< S3 object key (the HDF5 file name)
    uint64_t    size = 0;       ///< Object size in bytes
    std::string last_modified;  ///< ISO-8601 timestamp from S3 (may be empty)
};

/// Read/write index.parquet using the Apache Parquet C++ library.
class H5S3Index {
public:
    /// True if a regular file exists at @p path.
    static bool exists(const std::string &path);

    /// Write @p entries to the Parquet file at @p path (overwriting any existing file).
    /// Throws std::runtime_error on failure.
    static void write(const std::string &path, const std::vector<IndexEntry> &entries);

    /// Read all rows from the Parquet file at @p path.
    /// Throws std::runtime_error on failure.
    static std::vector<IndexEntry> read(const std::string &path);
};

} // namespace h5s3

#endif // H5S3_INDEX_H
