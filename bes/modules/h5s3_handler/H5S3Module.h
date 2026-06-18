// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3Module: BES module registration for h5s3_handler.

#ifndef I_H5S3Module_H
#define I_H5S3Module_H

#include <string>
#include <ostream>

#include "BESAbstractModule.h"

namespace h5s3 {

class H5S3Module : public BESAbstractModule {
public:
    H5S3Module() = default;
    ~H5S3Module() override = default;

    void initialize(const std::string &modname) override;
    void terminate(const std::string &modname) override;

    void dump(std::ostream &strm) const override;
};

} // namespace h5s3

#endif // I_H5S3Module_H
