// S3Container.cc

// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of S3_module, A C++ module that can be loaded in to
// the OPeNDAP Back-End Server (BES) and is able to handle remote requests.

// Copyright (c) 2020 OPeNDAP, Inc.
// Author: Nathan Potter <ndp@opendap.org>
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
// Authors:
//      ndp       Nathan Potter <ndp@opendap.org>

#ifndef E_S3Names_H
#define E_S3Names_H 1

#define S3_NAME "s3"    // Used when the handler is added to the services registry
#define MODULE S3_NAME  // Used for BESDEBUG calls

// These are text items that can appear in a DMR++. This text is replaced with an actual
// URL to some data.
#define DATA_ACCESS_URL_KEY "OPeNDAP_DMRpp_DATA_ACCESS_URL"
#define MISSING_DATA_ACCESS_URL_KEY "OPeNDAP_DMRpp_MISSING_DATA_ACCESS_URL"

// This is the name of a BES Key used by the module to control whether the software
// tries to substitute values for the above two 'keys.'
#define S3_INJECT_DATA_URL_KEY "S3.inject_data_urls"

// === S3 Catalog configuration keys ===
#define S3_CATALOG_BUCKET_KEY           "S3.Catalog.Bucket"
#define S3_CATALOG_ENDPOINT_KEY         "S3.Catalog.Endpoint"
#define S3_CATALOG_REGION_KEY           "S3.Catalog.Region"
#define S3_CATALOG_AWS_KEY_ID           "S3.Catalog.AWSKeyId"
#define S3_CATALOG_AWS_SECRET_KEY       "S3.Catalog.AWSSecretKey"
#define S3_CATALOG_USE_CACHE_KEY        "S3.Catalog.UseCache"
#define S3_CATALOG_MEM_CACHE_SIZE_KEY   "S3.Catalog.MemCacheSize.Items"
#define S3_CATALOG_MEM_CACHE_PURGE_KEY  "S3.Catalog.MemCachePurge.Items"
#define S3_CATALOG_FILE_CACHE_DIR_KEY   "S3.Catalog.FileCacheDir"
#define S3_CATALOG_FILE_CACHE_SIZE_KEY  "S3.Catalog.FileCacheSize.MB"
#define S3_CATALOG_FILE_CACHE_PURGE_KEY "S3.Catalog.FileCachePurge.MB"

#endif // E_S3Names_H
