// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3RequestHandler implementation.
//
// DAP responses are produced by H5S3Reader (which reads the HDF5 file from S3
// through the ROS3 VFD and emits DMR/DDS/DAS text), then parsed into the libdap
// response objects. The DMR is the primary response; the DDS is derived from
// the DMR and the DAS is parsed from the generated DAS text.

#include "config.h"

#include <cstdio>
#include <fstream>
#include <unistd.h>

#include <libdap/DMR.h>
#include <libdap/DDS.h>
#include <libdap/DAS.h>
#include <libdap/D4ParserSax2.h>
#include <libdap/D4BaseTypeFactory.h>
#include <libdap/BaseTypeFactory.h>

#include <BESDMRResponse.h>
#include <BESDDSResponse.h>
#include <BESDASResponse.h>
#include <BESDapNames.h>
#include <BESDataNames.h>
#include <BESResponseNames.h>
#include <BESResponseHandler.h>
#include <BESVersionInfo.h>
#include <BESInfo.h>
#include <BESContainer.h>
#include <BESInternalError.h>
#include <BESDebug.h>
#include <BESIndent.h>
#include <TheBESKeys.h>

#include "H5S3RequestHandler.h"
#include "H5S3DapBuilder.h"
#include "H5S3Names.h"

using namespace libdap;
using namespace std;

namespace h5s3 {

H5S3RequestHandler::H5S3RequestHandler(const string &name) : BESRequestHandler(name)
{
    add_method(DMR_RESPONSE, H5S3RequestHandler::h5s3_build_dmr);
    add_method(DAP4DATA_RESPONSE, H5S3RequestHandler::h5s3_build_dap4data);
    add_method(DDS_RESPONSE, H5S3RequestHandler::h5s3_build_dds);
    add_method(DAS_RESPONSE, H5S3RequestHandler::h5s3_build_das);
    add_method(HELP_RESPONSE, H5S3RequestHandler::h5s3_build_help);
    add_method(VERS_RESPONSE, H5S3RequestHandler::h5s3_build_version);
}

static string key(const string &name)
{
    string v;
    bool found = false;
    TheBESKeys::TheKeys()->get_value(name, v, found);
    return found ? v : string();
}

S3Auth H5S3RequestHandler::auth_from_keys()
{
    S3Auth a;
    string r = key(H5S3_REGION_KEY);   if (!r.empty()) a.region = r;
    a.access_key = key(H5S3_ACCESS_KEY);
    a.secret_key = key(H5S3_SECRET_KEY);
    a.endpoint = key(H5S3_ENDPOINT_KEY);
    string ps = key(H5S3_PATHSTYLE_KEY);
    a.force_path_style = (ps == "true" || ps == "TRUE" || ps == "yes" || ps == "1");
    return a;
}

string H5S3RequestHandler::key_from_dhi(BESDataHandlerInterface &dhi)
{
    // The S3 object key is the container's relative name (path under the
    // h5s3 data directory), without a leading slash.
    string rel = dhi.container->get_relative_name();
    if (rel.empty())
        rel = dhi.container->get_real_name();
    while (!rel.empty() && rel.front() == '/')
        rel.erase(rel.begin());
    return rel;
}

bool H5S3RequestHandler::h5s3_build_dmr(BESDataHandlerInterface &dhi)
{
    auto *response = dynamic_cast<BESDMRResponse *>(dhi.response_handler->get_response_object());
    if (!response)
        throw BESInternalError("Expected a BESDMRResponse", __FILE__, __LINE__);

    DMR *dmr = response->get_dmr();
    static D4BaseTypeFactory factory;
    dmr->set_factory(&factory);

    string bucket = key(H5S3_BUCKET_KEY);
    string obj = key_from_dhi(dhi);

    try {
        H5S3Reader reader(auth_from_keys());
        string xml = reader.build_dmr(bucket, obj, obj);

        D4ParserSax2 parser;
        parser.intern(xml, dmr);
        dmr->set_name(obj);
        dmr->set_filename(obj);
    }
    catch (const std::exception &e) {
        dmr->set_factory(nullptr);
        throw BESInternalError(string("h5s3: ") + e.what(), __FILE__, __LINE__);
    }

    dmr->set_factory(nullptr);
    return true;
}

bool H5S3RequestHandler::h5s3_build_dap4data(BESDataHandlerInterface &dhi)
{
    auto *response = dynamic_cast<BESDMRResponse *>(dhi.response_handler->get_response_object());
    if (!response)
        throw BESInternalError("Expected a BESDMRResponse", __FILE__, __LINE__);

    DMR *dmr = response->get_dmr();
    static D4BaseTypeFactory factory;
    dmr->set_factory(&factory);

    string bucket = key(H5S3_BUCKET_KEY);
    string obj = key_from_dhi(dhi);

    try {
        // Build a DMR whose variables read their data live from S3 (ROS3), then
        // let the framework apply the DAP4 constraint/function and serialize.
        build_dmr_object(dmr, auth_from_keys(), bucket, obj, obj);
        dmr->set_filename(obj);

        response->set_dap4_constraint(dhi);
        response->set_dap4_function(dhi);
    }
    catch (const std::exception &e) {
        dmr->set_factory(nullptr);
        throw BESInternalError(string("h5s3: ") + e.what(), __FILE__, __LINE__);
    }

    dmr->set_factory(nullptr);
    return true;
}

bool H5S3RequestHandler::h5s3_build_dds(BESDataHandlerInterface &dhi)
{
    auto *response = dynamic_cast<BESDDSResponse *>(dhi.response_handler->get_response_object());
    if (!response)
        throw BESInternalError("Expected a BESDDSResponse", __FILE__, __LINE__);

    string bucket = key(H5S3_BUCKET_KEY);
    string obj = key_from_dhi(dhi);

    try {
        H5S3Reader reader(auth_from_keys());
        string dds_text = reader.build_dds(bucket, obj, obj);

        char tmpl[] = "/tmp/h5s3_dds_XXXXXX";
        int fd = mkstemp(tmpl);
        if (fd < 0)
            throw BESInternalError("h5s3: cannot create temp file for DDS", __FILE__, __LINE__);
        { ofstream os(tmpl); os << dds_text; }
        DDS *dds = response->get_dds();
        // Heap-allocated factory whose lifetime matches the DDS (avoids a
        // dangling factory pointer during the DDS's own teardown).
        dds->set_factory(new BaseTypeFactory);
        dds->parse(string(tmpl));
        dds->set_dataset_name(obj);
        close(fd);
        unlink(tmpl);
    }
    catch (const std::exception &e) {
        throw BESInternalError(string("h5s3: ") + e.what(), __FILE__, __LINE__);
    }
    return true;
}

bool H5S3RequestHandler::h5s3_build_das(BESDataHandlerInterface &dhi)
{
    auto *response = dynamic_cast<BESDASResponse *>(dhi.response_handler->get_response_object());
    if (!response)
        throw BESInternalError("Expected a BESDASResponse", __FILE__, __LINE__);

    string bucket = key(H5S3_BUCKET_KEY);
    string obj = key_from_dhi(dhi);

    try {
        H5S3Reader reader(auth_from_keys());
        string das_text = reader.build_das(bucket, obj, obj);

        // Parse the generated DAS text through the libdap DAS grammar.
        char tmpl[] = "/tmp/h5s3_das_XXXXXX";
        int fd = mkstemp(tmpl);
        if (fd < 0)
            throw BESInternalError("h5s3: cannot create temp file for DAS", __FILE__, __LINE__);
        { ofstream os(tmpl); os << das_text; }
        DAS *das = response->get_das();
        das->parse(string(tmpl));
        close(fd);
        unlink(tmpl);
    }
    catch (const std::exception &e) {
        throw BESInternalError(string("h5s3: ") + e.what(), __FILE__, __LINE__);
    }
    return true;
}

bool H5S3RequestHandler::h5s3_build_help(BESDataHandlerInterface &dhi)
{
    auto info = dynamic_cast<BESInfo *>(dhi.response_handler->get_response_object());
    if (!info)
        throw BESInternalError("Expected a BESInfo response", __FILE__, __LINE__);

    info->begin_tag("module");
    info->add_tag("name", MODULE_NAME);
    info->add_tag("version", MODULE_VERSION);
    info->add_data_from_file("h5s3_handler.Reference", "h5s3_handler");
    info->end_tag("module");
    return true;
}

bool H5S3RequestHandler::h5s3_build_version(BESDataHandlerInterface &dhi)
{
    auto info = dynamic_cast<BESVersionInfo *>(dhi.response_handler->get_response_object());
    if (!info)
        throw BESInternalError("Expected a BESVersionInfo response", __FILE__, __LINE__);

    info->add_module(MODULE_NAME, MODULE_VERSION);
    return true;
}

void H5S3RequestHandler::dump(ostream &strm) const
{
    strm << BESIndent::LMarg << "H5S3RequestHandler::dump - (" << (void *) this << ")" << endl;
    BESIndent::Indent();
    BESRequestHandler::dump(strm);
    BESIndent::UnIndent();
}

} // namespace h5s3
