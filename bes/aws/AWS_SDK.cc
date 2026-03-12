// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of the OPeNDAP Back-End Server (BES)
// and creates an allowed hosts list of which systems that may be
// accessed by the server as part of its routine operation.

// Copyright (c) 2025 OPeNDAP, Inc.
// Author: James Gallagher <jgallagher@opendap.org>
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
//

// Created by James Gallagher on 3/4/25.

#include "config.h"

#include <string>
#include <exception>

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <aws/core/utils/DateTime.h>
#include <fstream>
#include <iostream>

#include "AWS_SDK.h"

#include "BESInternalFatalError.h"

using namespace std;

namespace bes {

Aws::SDKOptions AWS_SDK::options;


/**
 * @brief Get an S3 Client.
 * @param region AWS region string.
 * @param aws_key The AWS Key ID
 * @param aws_secret_key The AWS Secret Key
 * @return The AWS S3 Client object
 */
Aws::S3::S3Client AWS_SDK::get_s3_client(const string &region, const string &aws_key,
                                          const string &aws_secret_key,
                                          const string &endpoint_override) {
    Aws::S3::S3ClientConfiguration clientConfig;
    clientConfig.region = region;

    if (!endpoint_override.empty()) {
        clientConfig.endpointOverride = endpoint_override;
        // Use path-style URLs required for localstack and custom S3-compatible endpoints
        clientConfig.useVirtualAddressing = false;
    }

    auto credentialsProvider = Aws::Auth::AWSCredentials(aws_key, aws_secret_key);

    return {credentialsProvider, nullptr, clientConfig};
}

void AWS_SDK::throw_if_s3_client_uninitialized() const {
    if (!d_is_s3_client_initialized) {
        throw BESInternalFatalError("AWS s3 client called before initialization.", __FILE__, __LINE__);
    }
}

/**
 *
 * @param bucket Name of the S3 bucket
 * @param key Object key in the bucket
 * @return True if the object exists and can be accessed, false otherwise
 */
bool AWS_SDK::s3_head_exists(const string &bucket, const string &key) {
    throw_if_s3_client_uninitialized();

    Aws::S3::Model::HeadObjectRequest head_request;
    head_request.SetBucket(bucket);
    head_request.SetKey(key);

    const auto head_outcome = d_s3_client.HeadObject(head_request);
    if (head_outcome.IsSuccess()) {
        return true;
    }
    const auto &error = head_outcome.GetError();
    const auto http_code = error.GetResponseCode(); // Aws::Http::HttpResponseCode is an enum. See cast below.
    d_aws_exception_message = error.GetMessage();
    d_aws_exception_name = error.GetExceptionName();
    d_http_status_code = static_cast<int>(http_code);

    return false;
}

/**
 *
 * @param bucket Name of the S3 bucket
 * @param key Object key in the bucket
 * @return Received data as a string or the empty string
 */
string AWS_SDK::s3_get_as_string(const string &bucket, const string &key) {
    throw_if_s3_client_uninitialized();

    Aws::S3::Model::GetObjectRequest object_request;
    object_request.SetBucket(bucket);
    object_request.SetKey(key);

    auto get_object_outcome = d_s3_client.GetObject(object_request);
    if (get_object_outcome.IsSuccess()) {
        const auto &retrieved_file = get_object_outcome.GetResultWithOwnership().GetBody();
        stringstream file_contents;
        file_contents << retrieved_file.rdbuf();
        return {file_contents.str()};
    }
    const auto error = get_object_outcome.GetError();
    const auto httpCode = error.GetResponseCode(); // Aws::Http::HttpResponseCode
    d_aws_exception_message = error.GetMessage();
    d_aws_exception_name = error.GetExceptionName();
    d_http_status_code = static_cast<int>(httpCode);

    return {""};
}

/**
 *
 * @param bucket Name of the S3 bucket
 * @param key Object key in the bucket
 * @param filename Local file/path name for the received data
 * @return True if successful, false otherwise
 */
bool AWS_SDK::s3_get_as_file(const string &bucket, const string &key, const string &filename) {
    throw_if_s3_client_uninitialized();

    Aws::S3::Model::GetObjectRequest object_request;
    object_request.SetBucket(bucket);
    object_request.SetKey(key);

    auto get_object_outcome = d_s3_client.GetObject(object_request);
    if (get_object_outcome.IsSuccess()) {
        const auto &retrieved_file = get_object_outcome.GetResultWithOwnership().GetBody();
        std::ofstream output_file(filename, std::ios::binary);
        output_file << retrieved_file.rdbuf();
        return true;
    }
    const auto error = get_object_outcome.GetError();
    const auto httpCode = error.GetResponseCode(); // Aws::Http::HttpResponseCode
    d_aws_exception_message = error.GetMessage();
    d_aws_exception_name = error.GetExceptionName();
    d_http_status_code = static_cast<int>(httpCode);

    return false;
}

/**
 *
 * @param bucketName Name of the bucket.
 * @param key Name of an object key.
 * @param expirationSeconds Expiration in seconds for pre-signed URL.
 * @param clientConfig Aws client configuration.
 * @return Aws::String A pre-signed URL. Will look something like `https://<bucket>.s3.us-east-1.amazonaws.com/<key>?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=<hash>%2F20251010%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20251010T152738Z&X-Amz-Expires=60&X-Amz-SignedHeaders=host&X-Amz-Signature=<hash>`
 */
Aws::String AWS_SDK::s3_generate_presigned_object_url(const Aws::String &bucket_name,
                                                      const Aws::String &key,
                                                      uint64_t expiration_seconds) {
    throw_if_s3_client_uninitialized();

    return d_s3_client.GeneratePresignedUrl(bucket_name, key, Aws::Http::HttpMethod::HTTP_GET, expiration_seconds);
}

/**
 * @brief List objects in an S3 bucket under the given prefix.
 *
 * Uses ListObjectsV2 with pagination. When a delimiter is provided (default "/"),
 * common prefixes are returned as S3ObjectInfo entries with is_prefix=true so
 * callers can treat them as "directories."
 *
 * @param bucket     S3 bucket name
 * @param prefix     Key prefix to list (empty string for bucket root)
 * @param delimiter  Grouping delimiter (default "/")
 * @return Vector of S3ObjectInfo entries (prefixes + objects), empty on error
 */
std::vector<S3ObjectInfo> AWS_SDK::s3_list_objects(const string &bucket,
                                                    const string &prefix,
                                                    const string &delimiter) {
    throw_if_s3_client_uninitialized();

    std::vector<S3ObjectInfo> results;
    Aws::String continuation_token;
    bool is_truncated = true;

    while (is_truncated) {
        Aws::S3::Model::ListObjectsV2Request req;
        req.SetBucket(bucket);
        req.SetPrefix(prefix);
        if (!delimiter.empty()) req.SetDelimiter(delimiter);
        if (!continuation_token.empty()) req.SetContinuationToken(continuation_token);

        auto outcome = d_s3_client.ListObjectsV2(req);
        if (!outcome.IsSuccess()) {
            const auto &error = outcome.GetError();
            d_aws_exception_message = error.GetMessage();
            d_aws_exception_name = error.GetExceptionName();
            d_http_status_code = static_cast<int>(error.GetResponseCode());
            break;
        }

        const auto &result = outcome.GetResult();

        // Real objects (leaves)
        for (const auto &obj : result.GetContents()) {
            S3ObjectInfo info;
            info.key = obj.GetKey();
            info.size = static_cast<size_t>(obj.GetSize());
            info.last_modified = obj.GetLastModified().ToGmtString(Aws::Utils::DateFormat::ISO_8601);
            info.is_prefix = false;
            results.push_back(std::move(info));
        }

        // Common prefixes (sub-directories)
        for (const auto &cp : result.GetCommonPrefixes()) {
            S3ObjectInfo info;
            info.key = cp.GetPrefix();
            info.size = 0;
            info.is_prefix = true;
            results.push_back(std::move(info));
        }

        is_truncated = result.GetIsTruncated();
        continuation_token = result.GetNextContinuationToken();
    }

    return results;
}

} // namespace bes