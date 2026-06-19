// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3ContainerStorage implementation.

#include "config.h"

#include <string>

#include <BESIndent.h>

#include "H5S3ContainerStorage.h"
#include "H5S3Container.h"
#include "H5S3Names.h"

using namespace std;

namespace h5s3 {

H5S3ContainerStorage::H5S3ContainerStorage(const string &n) : BESContainerStorageVolatile(n)
{
}

/// Create an H5S3Container for the S3 object key @p r_name. The container type
/// is forced to "h5s3" so requests route to the h5s3 handler.
void H5S3ContainerStorage::add_container(const string &s_name, const string &r_name, const string & /*type*/)
{
    // Strip a leading '/' so the value is a bare S3 key.
    string key = r_name;
    if (!key.empty() && key.front() == '/')
        key.erase(key.begin());

    auto c = new H5S3Container(s_name, key, H5S3_NAME);
    BESContainerStorageVolatile::add_container(c);
}

void H5S3ContainerStorage::dump(ostream &strm) const
{
    strm << BESIndent::LMarg << "H5S3ContainerStorage::dump - (" << (void *) this << ")" << endl;
    BESIndent::Indent();
    BESContainerStorageVolatile::dump(strm);
    BESIndent::UnIndent();
}

} // namespace h5s3
