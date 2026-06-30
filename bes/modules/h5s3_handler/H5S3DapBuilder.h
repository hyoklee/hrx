// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3DapBuilder: build a DAP4 DMR whose variables read their data live from
// S3 through the HDF5 ROS3 VFD. Unlike H5S3Reader (which only emits DMR/DDS/DAS
// *text* for the metadata responses), the variables created here carry enough
// information to fetch their own data on demand, so the DMR can back a DAP4
// data response (get.dap), including the fileout-netcdf (.dap.nc4) path.
//
// This code depends on libdap and is linked only into the in-process module
// (libh5s3_handler), never into the libdap-free h5s3_dap CLI helper.

#ifndef H5S3_DAP_BUILDER_H
#define H5S3_DAP_BUILDER_H

#include <string>

#include "H5S3Reader.h"   // for h5s3::S3Auth

namespace libdap { class DMR; }

namespace h5s3 {

/// Populate @p dmr with the structure of the HDF5 object s3://<bucket>/<key>,
/// creating read-capable variables (atomic scalars and arrays) that fetch their
/// data from S3 via the ROS3 VFD when the framework serializes the response.
/// @param name dataset name set on the DMR.
void build_dmr_object(libdap::DMR *dmr, const S3Auth &auth,
                      const std::string &bucket, const std::string &key,
                      const std::string &name);

} // namespace h5s3

#endif // H5S3_DAP_BUILDER_H
