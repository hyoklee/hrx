// -*- mode: c++; c-basic-offset:4 -*-
//
// h5s3_dap: build a DAP response (DMR/DDS/DAS) for one HDF5 object read from S3
// through the HDF5 ROS3 VFD. Links only HDF5 (+ its AWS C runtime) -- no Arrow,
// no AWS C++ SDK. Connection comes from the environment.
//
//   h5s3_dap dmr|dds|das <key>

#include <cstdlib>
#include <iostream>
#include <string>

#include "H5S3Reader.h"

using namespace std;

static string env(const char *k, const string &dflt = "")
{
    const char *v = getenv(k);
    return (v && *v) ? string(v) : dflt;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        cerr << "usage: h5s3_dap dmr|dds|das <key>\n";
        return 1;
    }
    string cmd = argv[1];
    string key = argv[2];
    string bucket = env("H5S3_BUCKET", "h5s3");

    h5s3::S3Auth auth;
    auth.region = env("AWS_DEFAULT_REGION", "us-east-1");
    auth.access_key = env("AWS_ACCESS_KEY_ID", "test");
    auth.secret_key = env("AWS_SECRET_ACCESS_KEY", "test");
    auth.endpoint = env("H5S3_ENDPOINT", "http://localhost:4566");

    try {
        h5s3::H5S3Reader reader(auth);
        if (cmd == "dmr") cout << reader.build_dmr(bucket, key, key);
        else if (cmd == "dds") cout << reader.build_dds(bucket, key, key);
        else if (cmd == "das") cout << reader.build_das(bucket, key, key);
        else { cerr << "unknown command: " << cmd << "\n"; return 1; }
    } catch (const exception &e) {
        cerr << "error: " << e.what() << "\n";
        return 2;
    }
    return 0;
}
