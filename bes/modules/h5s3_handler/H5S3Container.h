// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3Container: a BES container whose "real name" is an S3 object key. Unlike a
// normal catalog container, it is never staged to local disk -- access() simply
// returns the key, and the h5s3 request handler opens it from S3 via the ROS3
// VFD. Modeled on s3_reader's S3Container.

#ifndef H5S3Container_h_
#define H5S3Container_h_ 1

#include <string>
#include <ostream>

#include "BESContainer.h"

namespace h5s3 {

class H5S3Container : public BESContainer {
    void initialize();
    void _duplicate(H5S3Container &copy_to);

public:
    H5S3Container() = default;
    H5S3Container(const std::string &sym_name, const std::string &real_name, const std::string &type);

    H5S3Container(const H5S3Container &copy_from) = delete;
    H5S3Container &operator=(const H5S3Container &other) = delete;

    ~H5S3Container() override = default;

    BESContainer *ptr_duplicate() override;

    /// The S3 object key; the h5s3 handler opens it via the ROS3 VFD.
    std::string access() override;

    bool release() override;

    void dump(std::ostream &strm) const override;
};

} // namespace h5s3

#endif // H5S3Container_h_
