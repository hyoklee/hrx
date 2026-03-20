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

#ifndef BESS3Catalog_h_
#define BESS3Catalog_h_ 1

#include <ostream>
#include <string>
#include <vector>

#include "BESCatalog.h"
#include "MemoryCache.h"
#include "FileCache.h"
#include "AWS_SDK.h"

class BESCatalogEntry;

namespace bes {
class CatalogNode;
}

/**
 * @brief A BES Catalog that lists objects in an S3 bucket.
 *
 * BESS3Catalog implements BESCatalog for S3-backed storage. It calls
 * S3 ListObjectsV2 with a "/" delimiter so that key prefixes appear as
 * "directory" nodes and real objects appear as leaves.
 *
 * Results are cached in a two-tier cache (MemoryCache + FileCache) using
 * the same pattern as NgapOwnedContainer.
 *
 * Configuration keys (set in s3.conf):
 *   S3.Catalog.Bucket          - S3 bucket name (required)
 *   S3.Catalog.Endpoint        - Optional endpoint override (e.g. localstack URL)
 *   S3.Catalog.Region          - AWS region (default: us-east-1)
 *   S3.Catalog.AWSKeyId        - AWS access key ID
 *   S3.Catalog.AWSSecretKey    - AWS secret access key
 *   S3.Catalog.UseCache        - Enable two-tier caching (default: true)
 *   S3.Catalog.MemCacheSize.Items  - Max memory cache entries (default: 100)
 *   S3.Catalog.MemCachePurge.Items - Items purged per eviction cycle (default: 20)
 *   S3.Catalog.FileCacheDir    - Directory for file cache (default: /tmp/hyrax_s3_catalog_cache)
 *   S3.Catalog.FileCacheSize.MB    - File cache max size in MB (default: 100)
 *   S3.Catalog.FileCachePurge.MB   - File cache purge target in MB (default: 20)
 *
 * Also reads BES standard catalog keys:
 *   BES.Catalog.s3.TypeMatch   - Handler:regex pairs for identifying data files
 *   BES.Catalog.s3.RootDirectory - Required by BESCatalogUtils (set to "/" or any valid dir)
 */
class BESS3Catalog : public BESCatalog {
    std::string d_bucket;
    std::string d_endpoint;
    std::string d_region;
    std::string d_aws_key;
    std::string d_aws_secret;

    // Two-tier cache — static so all instances share the same cache
    static bool d_use_cache;
    static ngap::MemoryCache<std::string> d_mem_cache;
    static FileCache d_file_cache;
    static int d_mem_cache_size;
    static int d_mem_cache_purge;
    static long long d_file_cache_size_bytes;
    static long long d_file_cache_purge_bytes;
    static std::string d_file_cache_dir;
    static int d_cache_ttl_seconds;   ///< listing cache TTL (0 = never expire)

    // AWS client — one per instance, initialized lazily
    mutable bes::AWS_SDK d_aws;
    mutable bool d_aws_initialized = false;

    void init_aws_client() const;

    std::vector<bes::S3ObjectInfo> get_listing(const std::string &s3_prefix) const;
    bes::CatalogNode *build_node(const std::string &path,
                                  const std::vector<bes::S3ObjectInfo> &items) const;

    static std::string serialize(const std::vector<bes::S3ObjectInfo> &items);
    static std::vector<bes::S3ObjectInfo> deserialize(const std::string &data);

    static std::string make_cache_key(const std::string &bucket, const std::string &prefix);
    static std::string path_to_prefix(const std::string &path);
    bool is_expired(const std::string &cached_data) const;

public:
    explicit BESS3Catalog(const std::string &catalog_name);
    ~BESS3Catalog() override = default;

    BESCatalogEntry *show_catalog(const std::string &container, BESCatalogEntry *entry) override;

    std::string get_root() const override;

    bes::CatalogNode *get_node(const std::string &path) const override;

    void get_site_map(const std::string &prefix, const std::string &node_suffix,
                      const std::string &leaf_suffix, std::ostream &out,
                      const std::string &path = "/") const override;

    void dump(std::ostream &strm) const override;
};

#endif // BESS3Catalog_h_
