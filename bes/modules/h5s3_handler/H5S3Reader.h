// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3Reader: open an HDF5 file directly from S3 using the HDF5 ROS3 virtual
// file driver and build DAP responses (DMR, DDS, DAS) from it.
//
// This is a deliberately "thin" reader: it covers the common atomic and array
// datatypes (integer, float, fixed/variable string) plus compound datasets and
// HDF5 attributes. It is not a full CF-aware translation like hdf5_handler.

#ifndef H5S3_READER_H
#define H5S3_READER_H

#include <string>

#include <hdf5.h>

namespace h5s3 {

/// S3 connection parameters for the ROS3 VFD.
struct S3Auth {
    std::string region = "us-east-1";
    std::string access_key;            ///< AWS access key id (e.g. "test" for localstack)
    std::string secret_key;            ///< AWS secret key
    std::string endpoint;              ///< Alternate endpoint URL (e.g. http://localhost:4566); empty = real AWS
};

/// Build DAP responses for an HDF5 object read from S3 via the ROS3 VFD.
class H5S3Reader {
public:
    explicit H5S3Reader(S3Auth auth) : d_auth(std::move(auth)) {}

    /// Open s3://<bucket>/<key> via the ROS3 VFD. Returns a file id (>=0) or throws.
    hid_t open(const std::string &bucket, const std::string &key) const;

    /// Build a DAP4 DMR (XML) for the object. @p name is the dataset name attribute.
    std::string build_dmr(const std::string &bucket, const std::string &key, const std::string &name) const;

    /// Build a DAP2 DDS (text) for the object.
    std::string build_dds(const std::string &bucket, const std::string &key, const std::string &name) const;

    /// Build a DAP2 DAS (text) for the object.
    std::string build_das(const std::string &bucket, const std::string &key, const std::string &name) const;

private:
    S3Auth d_auth;
};

} // namespace h5s3

#endif // H5S3_READER_H
