// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3ContainerStorage: a volatile container store whose containers name S3
// objects served by the h5s3 handler (via ROS3). Modeled on s3_reader's
// S3ContainerStorage. Registered under the space name "h5s3".

#ifndef H5S3ContainerStorage_h_
#define H5S3ContainerStorage_h_ 1

#include <string>
#include <ostream>

#include "BESContainerStorageVolatile.h"

namespace h5s3 {

class H5S3ContainerStorage : public BESContainerStorageVolatile {
public:
    explicit H5S3ContainerStorage(const std::string &n);
    ~H5S3ContainerStorage() override = default;

    void add_container(const std::string &s_name, const std::string &r_name, const std::string &type) override;

    void dump(std::ostream &strm) const override;
};

} // namespace h5s3

#endif // H5S3ContainerStorage_h_
