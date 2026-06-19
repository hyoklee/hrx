// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3Module implementation.

#include "config.h"

#include <BESRequestHandlerList.h>
#include <BESContainerStorageList.h>
#include <BESCatalogList.h>
#include <BESDebug.h>
#include <BESIndent.h>

#include "H5S3Module.h"
#include "H5S3RequestHandler.h"
#include "H5S3ContainerStorage.h"
#include "H5S3Catalog.h"
#include "H5S3Names.h"

using namespace std;

namespace h5s3 {

void H5S3Module::initialize(const string &modname)
{
    BESDEBUG(modname, "Initializing h5s3 module " << modname << endl);

    BESRequestHandlerList::TheList()->add_handler(modname, new H5S3RequestHandler(modname));

    // Container store so S3 objects can be referenced with space="h5s3" and
    // served via ROS3 without a local-file existence check.
    BESContainerStorageList::TheList()->add_persistence(new H5S3ContainerStorage(modname));

    // Catalog that lists the bucket's HDF5 files from the cached index.parquet.
    if (!BESCatalogList::TheCatalogList()->ref_catalog(modname))
        BESCatalogList::TheCatalogList()->add_catalog(new H5S3Catalog(modname));

    BESDebug::Register(modname);

    BESDEBUG(modname, "Done initializing h5s3 module " << modname << endl);
}

void H5S3Module::terminate(const string &modname)
{
    BESDEBUG(modname, "Cleaning h5s3 module " << modname << endl);

    BESRequestHandler *rh = BESRequestHandlerList::TheList()->remove_handler(modname);
    delete rh;

    BESContainerStorageList::TheList()->deref_persistence(modname);

    BESCatalogList::TheCatalogList()->deref_catalog(modname);

    BESDEBUG(modname, "Done cleaning h5s3 module " << modname << endl);
}

void H5S3Module::dump(ostream &strm) const
{
    strm << BESIndent::LMarg << "H5S3Module::dump - (" << (void *) this << ")" << endl;
}

} // namespace h5s3

extern "C" BESAbstractModule *maker()
{
    return new h5s3::H5S3Module;
}
