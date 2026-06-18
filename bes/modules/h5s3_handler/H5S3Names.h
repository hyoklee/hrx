// -*- mode: c++; c-basic-offset:4 -*-
//
// Configuration key names for the h5s3_handler module. These are read from the
// BES keys (h5s3.conf).

#ifndef H5S3_NAMES_H
#define H5S3_NAMES_H

#define H5S3_NAME "h5s3"

// S3 connection
#define H5S3_BUCKET_KEY    "H5S3.Bucket"        // S3 bucket holding the HDF5 files
#define H5S3_ENDPOINT_KEY  "H5S3.Endpoint"      // Alternate endpoint (e.g. http://localhost:4566); empty => AWS
#define H5S3_REGION_KEY    "H5S3.Region"        // AWS region (default us-east-1)
#define H5S3_ACCESS_KEY    "H5S3.AccessKeyId"   // Access key id
#define H5S3_SECRET_KEY    "H5S3.SecretKey"     // Secret key

// Index (Parquet) cache
#define H5S3_INDEX_DIR_KEY "H5S3.IndexDir"      // Dir for index.parquet (default {prefix}/share/hyrax/data/h5s3)
#define H5S3_INDEX_NAME    "index.parquet"

// Paths to the out-of-process helper executables (Arrow/Parquet and AWS C++ SDK
// must not share the BES process, so listing + indexing run as helpers).
#define H5S3_LIST_BIN_KEY  "H5S3.ListBin"       // path to h5s3_list
#define H5S3_INDEX_BIN_KEY "H5S3.IndexBin"      // path to h5s3_index

#endif // H5S3_NAMES_H
