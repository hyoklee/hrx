// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3RequestHandler: serves DAP responses for HDF5 files in S3 via the ROS3 VFD.

#ifndef I_H5S3RequestHandler_H
#define I_H5S3RequestHandler_H

#include <string>
#include <ostream>

#include "BESRequestHandler.h"
#include "H5S3Reader.h"

namespace h5s3 {

class H5S3RequestHandler : public BESRequestHandler {
public:
    explicit H5S3RequestHandler(const std::string &name);
    ~H5S3RequestHandler() override = default;

    void dump(std::ostream &strm) const override;

    static bool h5s3_build_dmr(BESDataHandlerInterface &dhi);
    static bool h5s3_build_dds(BESDataHandlerInterface &dhi);
    static bool h5s3_build_das(BESDataHandlerInterface &dhi);
    static bool h5s3_build_help(BESDataHandlerInterface &dhi);
    static bool h5s3_build_version(BESDataHandlerInterface &dhi);

private:
    // Read S3 connection parameters from the BES keys (h5s3.conf).
    static S3Auth auth_from_keys();
    // Map a BES container to an S3 object key.
    static std::string key_from_dhi(BESDataHandlerInterface &dhi);
};

} // namespace h5s3

#endif // I_H5S3RequestHandler_H
