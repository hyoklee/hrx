// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3Container implementation.

#include "config.h"

#include <string>

#include <BESInternalError.h>
#include <BESIndent.h>

#include "H5S3Container.h"
#include "H5S3Names.h"

using namespace std;

namespace h5s3 {

H5S3Container::H5S3Container(const string &sym_name, const string &real_name, const string &type)
    : BESContainer(sym_name, real_name, type)
{
    initialize();
}

void H5S3Container::initialize()
{
    // Requests for this container are served by the h5s3 request handler.
    if (get_container_type().empty())
        set_container_type(H5S3_NAME);

    // The relative name is the S3 object key (what the handler opens via ROS3).
    set_relative_name(get_real_name());
}

void H5S3Container::_duplicate(H5S3Container &copy_to)
{
    BESContainer::_duplicate(copy_to);
}

BESContainer *H5S3Container::ptr_duplicate()
{
    auto container = new H5S3Container;
    _duplicate(*container);
    return container;
}

string H5S3Container::access()
{
    // No staging: the h5s3 handler reads the object straight from S3 via the
    // ROS3 VFD using this key. Return the key as the access string.
    return get_real_name();
}

bool H5S3Container::release()
{
    return true;
}

void H5S3Container::dump(ostream &strm) const
{
    strm << BESIndent::LMarg << "H5S3Container::dump - (" << (void *) this << ")" << endl;
    BESIndent::Indent();
    BESContainer::dump(strm);
    BESIndent::UnIndent();
}

} // namespace h5s3
