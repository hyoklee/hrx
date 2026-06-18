// -*- mode: c++; c-basic-offset:4 -*-
//
// h5s3_list: list *.h5 objects in an S3 bucket using the AWS C++ SDK and print
// them as TSV ("key\tsize\tlast_modified"). Kept in its own binary so the AWS
// C++ SDK and the Arrow/Parquet stack (which bundles a different AWS SDK) never
// share a process. Connection comes from the environment.

#include <cstdlib>
#include <iostream>
#include <string>

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/ListObjectsV2Request.h>

using namespace std;

static string env(const char *k, const string &dflt = "")
{
    const char *v = getenv(k);
    return (v && *v) ? string(v) : dflt;
}

int main()
{
    string bucket = env("H5S3_BUCKET", "h5s3");
    string region = env("AWS_DEFAULT_REGION", "us-east-1");
    string key_id = env("AWS_ACCESS_KEY_ID", "test");
    string secret = env("AWS_SECRET_ACCESS_KEY", "test");
    string endpoint = env("H5S3_ENDPOINT", "http://localhost:4566");

    Aws::SDKOptions options;
    Aws::InitAPI(options);
    int rc = 0;
    {
        Aws::Client::ClientConfiguration cfg;
        cfg.region = region;
        if (!endpoint.empty()) {
            cfg.endpointOverride = endpoint;
            cfg.scheme = Aws::Http::Scheme::HTTP;
        }
        Aws::Auth::AWSCredentials creds(key_id.c_str(), secret.c_str());
        Aws::S3::S3Client client(creds, cfg,
            Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
            /*useVirtualAddressing*/ false);

        Aws::String token;
        do {
            Aws::S3::Model::ListObjectsV2Request req;
            req.SetBucket(bucket.c_str());
            if (!token.empty()) req.SetContinuationToken(token);
            auto outcome = client.ListObjectsV2(req);
            if (!outcome.IsSuccess()) {
                cerr << "ListObjectsV2 failed: " << outcome.GetError().GetMessage() << "\n";
                rc = 2;
                break;
            }
            for (const auto &o : outcome.GetResult().GetContents()) {
                string key = o.GetKey().c_str();
                if (key.size() >= 3 && key.substr(key.size() - 3) == ".h5")
                    cout << key << "\t" << o.GetSize() << "\t"
                         << o.GetLastModified().ToGmtString(Aws::Utils::DateFormat::ISO_8601).c_str() << "\n";
            }
            token = outcome.GetResult().GetIsTruncated() ? outcome.GetResult().GetNextContinuationToken() : "";
        } while (!token.empty());
    }
    Aws::ShutdownAPI(options);
    return rc;
}
