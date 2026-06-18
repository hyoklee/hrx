// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3Reader implementation. Opens an HDF5 file from S3 via the ROS3 VFD and
// walks groups/datasets/attributes to emit DMR (DAP4), DDS and DAS (DAP2).

#include "H5S3Reader.h"

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <H5FDros3.h>

using namespace std;

namespace h5s3 {

// ---- datatype name mapping -------------------------------------------------

// DAP4 atomic type name for an HDF5 datatype (returns "" if unsupported here).
static string dap4_type_name(hid_t t)
{
    switch (H5Tget_class(t)) {
        case H5T_INTEGER: {
            size_t sz = H5Tget_size(t);
            bool sign = (H5Tget_sign(t) == H5T_SGN_2);
            switch (sz) {
                case 1: return sign ? "Int8" : "UInt8";
                case 2: return sign ? "Int16" : "UInt16";
                case 4: return sign ? "Int32" : "UInt32";
                case 8: return sign ? "Int64" : "UInt64";
                default: return "Int32";
            }
        }
        case H5T_FLOAT:
            return (H5Tget_size(t) <= 4) ? "Float32" : "Float64";
        case H5T_STRING:
            return "String";
        case H5T_ENUM:
            return "Int32";
        default:
            return "";
    }
}

// DAP2 (DDS/DAS) type name for an HDF5 datatype.
static string dap2_type_name(hid_t t)
{
    switch (H5Tget_class(t)) {
        case H5T_INTEGER: {
            size_t sz = H5Tget_size(t);
            bool sign = (H5Tget_sign(t) == H5T_SGN_2);
            switch (sz) {
                case 1: return "Byte";
                case 2: return sign ? "Int16" : "UInt16";
                case 4: return sign ? "Int32" : "UInt32";
                case 8: return sign ? "Int32" : "UInt32";   // DAP2 has no 64-bit ints
                default: return "Int32";
            }
        }
        case H5T_FLOAT:
            return (H5Tget_size(t) <= 4) ? "Float32" : "Float64";
        case H5T_STRING:
            return "String";
        case H5T_ENUM:
            return "Int32";
        default:
            return "";
    }
}

static string xml_escape(const string &s)
{
    string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

// ---- file open via ROS3 ----------------------------------------------------

hid_t H5S3Reader::open(const string &bucket, const string &key) const
{
    H5FD_ros3_fapl_t fa;
    memset(&fa, 0, sizeof(fa));
    fa.version = H5FD_CURR_ROS3_FAPL_T_VERSION;
    fa.authenticate = (!d_auth.access_key.empty()) ? 1 : 0;
    strncpy(fa.aws_region, d_auth.region.c_str(), H5FD_ROS3_MAX_REGION_LEN);
    strncpy(fa.secret_id, d_auth.access_key.c_str(), H5FD_ROS3_MAX_SECRET_ID_LEN);
    strncpy(fa.secret_key, d_auth.secret_key.c_str(), H5FD_ROS3_MAX_SECRET_KEY_LEN);

    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    if (fapl < 0)
        throw runtime_error("H5S3Reader: H5Pcreate failed");
    if (H5Pset_fapl_ros3(fapl, &fa) < 0) {
        H5Pclose(fapl);
        throw runtime_error("H5S3Reader: H5Pset_fapl_ros3 failed");
    }
    if (!d_auth.endpoint.empty() && H5Pset_fapl_ros3_endpoint(fapl, d_auth.endpoint.c_str()) < 0) {
        H5Pclose(fapl);
        throw runtime_error("H5S3Reader: H5Pset_fapl_ros3_endpoint failed");
    }

    string url = "s3://" + bucket + "/" + key;
    hid_t fid = H5Fopen(url.c_str(), H5F_ACC_RDONLY, fapl);
    H5Pclose(fapl);
    if (fid < 0)
        throw runtime_error("H5S3Reader: H5Fopen failed for " + url);
    return fid;
}

// ---- DMR (DAP4) ------------------------------------------------------------

// Build the dimension suffix for a dataset's dataspace as DAP4 <Dim> elements.
static string dap4_dims(hid_t space)
{
    int ndims = H5Sget_simple_extent_ndims(space);
    if (ndims <= 0)
        return "";
    vector<hsize_t> dims(ndims);
    H5Sget_simple_extent_dims(space, dims.data(), nullptr);
    string out;
    for (int i = 0; i < ndims; ++i)
        out += "      <Dim size=\"" + to_string((unsigned long long)dims[i]) + "\"/>\n";
    return out;
}

// Emit one DMR variable for an HDF5 dataset.
static string dmr_dataset(hid_t dset, const string &name, const string &indent)
{
    hid_t dt = H5Dget_type(dset);
    hid_t space = H5Dget_space(dset);
    string dims = dap4_dims(space);
    string out;

    if (H5Tget_class(dt) == H5T_COMPOUND) {
        out += indent + "<Structure name=\"" + xml_escape(name) + "\">\n";
        int nmembers = H5Tget_nmembers(dt);
        for (int m = 0; m < nmembers; ++m) {
            char *mname = H5Tget_member_name(dt, m);
            hid_t mt = H5Tget_member_type(dt, m);
            string tn = dap4_type_name(mt);
            if (!tn.empty())
                out += indent + "  <" + tn + " name=\"" + xml_escape(mname) + "\"/>\n";
            H5Tclose(mt);
            H5free_memory(mname);
        }
        if (!dims.empty())
            out += dims;
        out += indent + "</Structure>\n";
    }
    else {
        string tn = dap4_type_name(dt);
        if (tn.empty()) tn = "Int32";
        if (dims.empty()) {
            out += indent + "<" + tn + " name=\"" + xml_escape(name) + "\"/>\n";
        }
        else {
            out += indent + "<" + tn + " name=\"" + xml_escape(name) + "\">\n";
            out += dims;
            out += indent + "</" + tn + ">\n";
        }
    }

    H5Sclose(space);
    H5Tclose(dt);
    return out;
}

// Recursively walk a group, emitting DMR for datasets and nested groups.
static string dmr_group(hid_t loc, const string &indent)
{
    string out;
    H5G_info_t gi;
    H5Gget_info(loc, &gi);
    for (hsize_t i = 0; i < gi.nlinks; ++i) {
        char name[1024];
        H5Lget_name_by_idx(loc, ".", H5_INDEX_NAME, H5_ITER_INC, i, name, sizeof(name), H5P_DEFAULT);

        H5O_info2_t oi;
        if (H5Oget_info_by_idx3(loc, ".", H5_INDEX_NAME, H5_ITER_INC, i, &oi, H5O_INFO_BASIC, H5P_DEFAULT) < 0)
            continue;

        if (oi.type == H5O_TYPE_DATASET) {
            hid_t dset = H5Dopen2(loc, name, H5P_DEFAULT);
            if (dset >= 0) {
                out += dmr_dataset(dset, name, indent);
                H5Dclose(dset);
            }
        }
        else if (oi.type == H5O_TYPE_GROUP) {
            hid_t grp = H5Gopen2(loc, name, H5P_DEFAULT);
            if (grp >= 0) {
                out += indent + "<Group name=\"" + xml_escape(name) + "\">\n";
                out += dmr_group(grp, indent + "  ");
                out += indent + "</Group>\n";
                H5Gclose(grp);
            }
        }
    }
    return out;
}

string H5S3Reader::build_dmr(const string &bucket, const string &key, const string &name) const
{
    hid_t fid = open(bucket, key);
    string body;
    try {
        hid_t root = H5Gopen2(fid, "/", H5P_DEFAULT);
        body = dmr_group(root, "    ");
        H5Gclose(root);
    } catch (...) {
        H5Fclose(fid);
        throw;
    }
    H5Fclose(fid);

    ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
       << "<Dataset xmlns=\"http://xml.opendap.org/ns/DAP/4.0#\" "
       << "dapVersion=\"4.0\" dmrVersion=\"1.0\" name=\"" << xml_escape(name) << "\">\n"
       << body
       << "</Dataset>\n";
    return os.str();
}

// ---- DDS (DAP2) ------------------------------------------------------------

static string dds_dims(hid_t space)
{
    int ndims = H5Sget_simple_extent_ndims(space);
    if (ndims <= 0)
        return "";
    vector<hsize_t> dims(ndims);
    H5Sget_simple_extent_dims(space, dims.data(), nullptr);
    string out;
    for (int i = 0; i < ndims; ++i)
        out += "[" + to_string((unsigned long long)dims[i]) + "]";
    return out;
}

static string dds_dataset(hid_t dset, const string &name, const string &indent)
{
    hid_t dt = H5Dget_type(dset);
    hid_t space = H5Dget_space(dset);
    string dimstr = dds_dims(space);
    string out;

    if (H5Tget_class(dt) == H5T_COMPOUND) {
        out += indent + "Structure {\n";
        int nmembers = H5Tget_nmembers(dt);
        for (int m = 0; m < nmembers; ++m) {
            char *mname = H5Tget_member_name(dt, m);
            hid_t mt = H5Tget_member_type(dt, m);
            string tn = dap2_type_name(mt);
            if (!tn.empty())
                out += indent + "    " + tn + " " + mname + ";\n";
            H5Tclose(mt);
            H5free_memory(mname);
        }
        out += indent + "} " + name + dimstr + ";\n";
    }
    else {
        string tn = dap2_type_name(dt);
        if (tn.empty()) tn = "Int32";
        out += indent + tn + " " + name + dimstr + ";\n";
    }

    H5Sclose(space);
    H5Tclose(dt);
    return out;
}

static string dds_group(hid_t loc, const string &indent)
{
    string out;
    H5G_info_t gi;
    H5Gget_info(loc, &gi);
    for (hsize_t i = 0; i < gi.nlinks; ++i) {
        char name[1024];
        H5Lget_name_by_idx(loc, ".", H5_INDEX_NAME, H5_ITER_INC, i, name, sizeof(name), H5P_DEFAULT);
        H5O_info2_t oi;
        if (H5Oget_info_by_idx3(loc, ".", H5_INDEX_NAME, H5_ITER_INC, i, &oi, H5O_INFO_BASIC, H5P_DEFAULT) < 0)
            continue;
        if (oi.type == H5O_TYPE_DATASET) {
            hid_t dset = H5Dopen2(loc, name, H5P_DEFAULT);
            if (dset >= 0) { out += dds_dataset(dset, name, indent); H5Dclose(dset); }
        }
        else if (oi.type == H5O_TYPE_GROUP) {
            hid_t grp = H5Gopen2(loc, name, H5P_DEFAULT);
            if (grp >= 0) {
                out += indent + "Structure {\n";
                out += dds_group(grp, indent + "    ");
                out += indent + "} " + name + ";\n";
                H5Gclose(grp);
            }
        }
    }
    return out;
}

string H5S3Reader::build_dds(const string &bucket, const string &key, const string &name) const
{
    hid_t fid = open(bucket, key);
    string body;
    try {
        hid_t root = H5Gopen2(fid, "/", H5P_DEFAULT);
        body = dds_group(root, "    ");
        H5Gclose(root);
    } catch (...) { H5Fclose(fid); throw; }
    H5Fclose(fid);

    ostringstream os;
    os << "Dataset {\n" << body << "} " << name << ";\n";
    return os.str();
}

// ---- DAS (DAP2) ------------------------------------------------------------

// Emit attributes attached to an HDF5 object as a DAS attribute container.
static string das_attributes(hid_t obj, const string &container, const string &indent)
{
    int nattr = (int)H5Aget_num_attrs(obj);
    if (nattr <= 0)
        return "";
    string out = indent + container + " {\n";
    for (int i = 0; i < nattr; ++i) {
        hid_t attr = H5Aopen_idx(obj, (unsigned)i);
        if (attr < 0) continue;
        char aname[1024];
        H5Aget_name(attr, sizeof(aname), aname);
        hid_t at = H5Aget_type(attr);
        string tn = dap2_type_name(at);
        if (tn.empty()) tn = "String";

        if (H5Tget_class(at) == H5T_STRING) {
            // Read the (first) string value.
            hid_t aspace = H5Aget_space(attr);
            if (H5Tis_variable_str(at)) {
                char *rdata = nullptr;
                if (H5Aread(attr, at, &rdata) >= 0 && rdata) {
                    out += indent + "    String " + aname + " \"" + rdata + "\";\n";
                    H5free_memory(rdata);
                }
                H5Sclose(aspace);
            }
            else {
                size_t sz = H5Tget_size(at);
                vector<char> buf(sz + 1, 0);
                if (H5Aread(attr, at, buf.data()) >= 0)
                    out += indent + "    String " + aname + " \"" + string(buf.data()) + "\";\n";
                H5Sclose(aspace);
            }
        }
        else {
            out += indent + "    " + tn + " " + aname + " {...};\n";
        }
        H5Tclose(at);
        H5Aclose(attr);
    }
    out += indent + "}\n";
    return out;
}

static string das_group(hid_t loc, const string &indent)
{
    string out;
    H5G_info_t gi;
    H5Gget_info(loc, &gi);
    for (hsize_t i = 0; i < gi.nlinks; ++i) {
        char name[1024];
        H5Lget_name_by_idx(loc, ".", H5_INDEX_NAME, H5_ITER_INC, i, name, sizeof(name), H5P_DEFAULT);
        H5O_info2_t oi;
        if (H5Oget_info_by_idx3(loc, ".", H5_INDEX_NAME, H5_ITER_INC, i, &oi, H5O_INFO_BASIC, H5P_DEFAULT) < 0)
            continue;
        if (oi.type == H5O_TYPE_DATASET) {
            hid_t dset = H5Dopen2(loc, name, H5P_DEFAULT);
            if (dset >= 0) { out += das_attributes(dset, name, indent); H5Dclose(dset); }
        }
    }
    return out;
}

string H5S3Reader::build_das(const string &bucket, const string &key, const string & /*name*/) const
{
    hid_t fid = open(bucket, key);
    string body;
    try {
        hid_t root = H5Gopen2(fid, "/", H5P_DEFAULT);
        body = das_attributes(root, "HDF5_GLOBAL", "    ");
        body += das_group(root, "    ");
        H5Gclose(root);
    } catch (...) { H5Fclose(fid); throw; }
    H5Fclose(fid);

    ostringstream os;
    os << "Attributes {\n" << body << "}\n";
    return os.str();
}

} // namespace h5s3
