// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3DapBuilder implementation. Walks an HDF5 file opened from S3 via the ROS3
// VFD and builds a libdap DMR of read-capable variables. Each variable opens
// the file from S3 and reads its dataset on demand (lazy read()), so the same
// DMR can serve metadata and back the DAP4 data response.

#include "H5S3DapBuilder.h"

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <hdf5.h>

#include <libdap/DMR.h>
#include <libdap/D4Group.h>
#include <libdap/Structure.h>
#include <libdap/Array.h>
#include <libdap/Byte.h>
#include <libdap/Int8.h>
#include <libdap/Int16.h>
#include <libdap/UInt16.h>
#include <libdap/Int32.h>
#include <libdap/UInt32.h>
#include <libdap/Int64.h>
#include <libdap/UInt64.h>
#include <libdap/Float32.h>
#include <libdap/Float64.h>
#include <libdap/Str.h>

using namespace std;
using namespace libdap;

namespace h5s3 {

// ---- HDF5 native memory type for a libdap atomic type ----------------------

// Returns an HDF5 predefined native type (never to be closed) for reading the
// values of a libdap atomic type, or -1 for non-numeric / unsupported types.
static hid_t native_for(Type t)
{
    switch (t) {
        case dods_byte_c:
        case dods_uint8_c:   return H5T_NATIVE_UINT8;
        case dods_char_c:
        case dods_int8_c:    return H5T_NATIVE_INT8;
        case dods_int16_c:   return H5T_NATIVE_INT16;
        case dods_uint16_c:  return H5T_NATIVE_UINT16;
        case dods_int32_c:   return H5T_NATIVE_INT32;
        case dods_uint32_c:  return H5T_NATIVE_UINT32;
        case dods_int64_c:   return H5T_NATIVE_INT64;
        case dods_uint64_c:  return H5T_NATIVE_UINT64;
        case dods_float32_c: return H5T_NATIVE_FLOAT;
        case dods_float64_c: return H5T_NATIVE_DOUBLE;
        default:             return -1;
    }
}

// ---- shared data read (called lazily by a variable's read()) ---------------

// Read every string element of a dataset (fixed- or variable-length) into @p out.
static void read_strings(hid_t dset, hid_t fspace, hid_t ftype,
                         hssize_t npoints, vector<string> &out)
{
    if (npoints <= 0)
        return;
    hid_t mtype = H5Tcopy(H5T_C_S1);
    if (H5Tis_variable_str(ftype) > 0) {
        H5Tset_size(mtype, H5T_VARIABLE);
        vector<char *> ptrs((size_t) npoints, nullptr);
        if (H5Dread(dset, mtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, ptrs.data()) >= 0) {
            for (hssize_t i = 0; i < npoints; ++i)
                out[i] = ptrs[i] ? ptrs[i] : "";
            H5Dvlen_reclaim(mtype, fspace, H5P_DEFAULT, ptrs.data());
        }
    }
    else {
        size_t sz = H5Tget_size(ftype);
        H5Tset_size(mtype, sz);
        vector<char> buf((size_t) npoints * sz);
        if (H5Dread(dset, mtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf.data()) >= 0) {
            for (hssize_t i = 0; i < npoints; ++i) {
                const char *p = buf.data() + (size_t) i * sz;
                out[i] = string(p, strnlen(p, sz));
            }
        }
    }
    H5Tclose(mtype);
}

// Open s3://<bucket>/<key>, read the dataset at @p path in full, and store the
// values into the libdap variable @p bt (an Array or an atomic scalar).
static void read_into(const S3Auth &auth, const string &bucket, const string &key,
                      const string &path, BaseType *bt)
{
    H5S3Reader reader(auth);
    hid_t fid = reader.open(bucket, key);
    hid_t dset = -1, fspace = -1, ftype = -1;
    try {
        dset = H5Dopen2(fid, path.c_str(), H5P_DEFAULT);
        if (dset < 0)
            throw runtime_error("cannot open dataset " + path);
        fspace = H5Dget_space(dset);
        ftype = H5Dget_type(dset);
        hssize_t npoints = H5Sget_simple_extent_npoints(fspace);
        if (npoints < 0) npoints = 0;

        auto *arr = dynamic_cast<Array *>(bt);
        Type et = arr ? arr->var()->type() : bt->type();

        if (et == dods_str_c || et == dods_url_c) {
            vector<string> strs((size_t) npoints);
            read_strings(dset, fspace, ftype, npoints, strs);
            if (arr)
                arr->set_value(strs, (int) strs.size());
            else if (!strs.empty())
                static_cast<Str *>(bt)->set_value(strs[0]);
        }
        else {
            hid_t nt = native_for(et);
            if (nt < 0)
                throw runtime_error("unsupported element type for " + path);
            size_t esz = H5Tget_size(nt);
            vector<unsigned char> buf((size_t) npoints * esz);
            if (npoints > 0 &&
                H5Dread(dset, nt, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf.data()) < 0)
                throw runtime_error("H5Dread failed for " + path);
            // val2buf copies the row-major buffer into the variable, for both
            // arrays (npoints elements) and scalars (1 element).
            bt->val2buf(buf.data());
        }
    }
    catch (...) {
        if (ftype >= 0) H5Tclose(ftype);
        if (fspace >= 0) H5Sclose(fspace);
        if (dset >= 0) H5Dclose(dset);
        H5Fclose(fid);
        throw;
    }
    H5Tclose(ftype);
    H5Sclose(fspace);
    H5Dclose(dset);
    H5Fclose(fid);
}

// ---- read-capable variable types -------------------------------------------

// An Array whose read() pulls its dataset from S3 via ROS3.
class H5S3Array : public Array {
    S3Auth d_auth;
    string d_bucket, d_key, d_path;
public:
    H5S3Array(const string &n, BaseType *proto, S3Auth auth,
              string bucket, string key, string path)
        : Array(n, proto, true), d_auth(std::move(auth)),
          d_bucket(std::move(bucket)), d_key(std::move(key)), d_path(std::move(path)) {}

    BaseType *ptr_duplicate() override { return new H5S3Array(*this); }

    bool read() override
    {
        if (read_p())
            return true;
        read_into(d_auth, d_bucket, d_key, d_path, this);
        set_read_p(true);
        return true;
    }
};

// An atomic scalar whose read() pulls its dataset's single value from S3.
template <class Base>
class H5S3Scalar : public Base {
    S3Auth d_auth;
    string d_bucket, d_key, d_path;
public:
    H5S3Scalar(const string &n, S3Auth auth, string bucket, string key, string path)
        : Base(n), d_auth(std::move(auth)), d_bucket(std::move(bucket)),
          d_key(std::move(key)), d_path(std::move(path)) {}

    BaseType *ptr_duplicate() override { return new H5S3Scalar<Base>(*this); }

    bool read() override
    {
        if (this->read_p())
            return true;
        read_into(d_auth, d_bucket, d_key, d_path, this);
        this->set_read_p(true);
        return true;
    }
};

// ---- type construction from HDF5 datatypes ---------------------------------

// A plain (non-reading) libdap atomic prototype for an HDF5 datatype, used as
// the template variable of an Array and as compound-member placeholders.
// Returns nullptr for unsupported classes.
static BaseType *make_atomic_proto(hid_t t, const string &name)
{
    switch (H5Tget_class(t)) {
        case H5T_INTEGER: {
            size_t sz = H5Tget_size(t);
            bool sign = (H5Tget_sign(t) == H5T_SGN_2);
            switch (sz) {
                case 1: return sign ? (BaseType *) new Int8(name)  : (BaseType *) new Byte(name);
                case 2: return sign ? (BaseType *) new Int16(name) : (BaseType *) new UInt16(name);
                case 4: return sign ? (BaseType *) new Int32(name) : (BaseType *) new UInt32(name);
                case 8: return sign ? (BaseType *) new Int64(name) : (BaseType *) new UInt64(name);
                default: return new Int32(name);
            }
        }
        case H5T_FLOAT:
            return (H5Tget_size(t) <= 4) ? (BaseType *) new Float32(name) : (BaseType *) new Float64(name);
        case H5T_STRING:
            return new Str(name);
        case H5T_ENUM:
            return new Int32(name);
        default:
            return nullptr;
    }
}

// A read-capable atomic scalar for an HDF5 datatype. Returns nullptr if unsupported.
static BaseType *make_scalar(hid_t t, const string &name, const S3Auth &auth,
                             const string &bucket, const string &key, const string &path)
{
    switch (H5Tget_class(t)) {
        case H5T_INTEGER: {
            size_t sz = H5Tget_size(t);
            bool s = (H5Tget_sign(t) == H5T_SGN_2);
            switch (sz) {
                case 1: return s ? (BaseType *) new H5S3Scalar<Int8>(name, auth, bucket, key, path)
                                 : (BaseType *) new H5S3Scalar<Byte>(name, auth, bucket, key, path);
                case 2: return s ? (BaseType *) new H5S3Scalar<Int16>(name, auth, bucket, key, path)
                                 : (BaseType *) new H5S3Scalar<UInt16>(name, auth, bucket, key, path);
                case 4: return s ? (BaseType *) new H5S3Scalar<Int32>(name, auth, bucket, key, path)
                                 : (BaseType *) new H5S3Scalar<UInt32>(name, auth, bucket, key, path);
                case 8: return s ? (BaseType *) new H5S3Scalar<Int64>(name, auth, bucket, key, path)
                                 : (BaseType *) new H5S3Scalar<UInt64>(name, auth, bucket, key, path);
                default: return new H5S3Scalar<Int32>(name, auth, bucket, key, path);
            }
        }
        case H5T_FLOAT:
            return (H5Tget_size(t) <= 4)
                       ? (BaseType *) new H5S3Scalar<Float32>(name, auth, bucket, key, path)
                       : (BaseType *) new H5S3Scalar<Float64>(name, auth, bucket, key, path);
        case H5T_STRING:
            return new H5S3Scalar<Str>(name, auth, bucket, key, path);
        case H5T_ENUM:
            return new H5S3Scalar<Int32>(name, auth, bucket, key, path);
        default:
            return nullptr;
    }
}

// Build the libdap variable for one HDF5 dataset (scalar, array, or - best
// effort, without data - a compound as a Structure). @p h5path is the dataset's
// absolute path in the file (used by read() to reopen it from S3).
static BaseType *build_dataset_var(hid_t dset, const string &name, const string &h5path,
                                   const S3Auth &auth, const string &bucket, const string &key)
{
    hid_t dt = H5Dget_type(dset);
    hid_t space = H5Dget_space(dset);
    int ndims = H5Sget_simple_extent_ndims(space);
    vector<hsize_t> dims;
    if (ndims > 0) {
        dims.resize(ndims);
        H5Sget_simple_extent_dims(space, dims.data(), nullptr);
    }

    BaseType *result = nullptr;

    if (H5Tget_class(dt) == H5T_COMPOUND) {
        // Compound data read is not supported by this thin reader; emit the
        // structure shape so the variable still appears in the response.
        auto *st = new Structure(name);
        st->set_is_dap4(true);
        int nmembers = H5Tget_nmembers(dt);
        for (int m = 0; m < nmembers; ++m) {
            char *mname = H5Tget_member_name(dt, m);
            hid_t mt = H5Tget_member_type(dt, m);
            BaseType *proto = make_atomic_proto(mt, mname ? mname : "");
            if (proto) {
                proto->set_is_dap4(true);
                st->add_var_nocopy(proto);
            }
            H5Tclose(mt);
            H5free_memory(mname);
        }
        result = st;
    }
    else if (ndims <= 0) {
        result = make_scalar(dt, name, auth, bucket, key, h5path);
        if (!result)
            result = new H5S3Scalar<Int32>(name, auth, bucket, key, h5path);
    }
    else {
        BaseType *proto = make_atomic_proto(dt, name);
        if (!proto)
            proto = new Int32(name);
        proto->set_is_dap4(true);
        auto *arr = new H5S3Array(name, proto, auth, bucket, key, h5path);
        delete proto; // Array copies the prototype (Vector::add_var)
        for (int i = 0; i < ndims; ++i)
            arr->append_dim_ll((int64_t) dims[i]);
        result = arr;
    }

    if (result)
        result->set_is_dap4(true);

    H5Sclose(space);
    H5Tclose(dt);
    return result;
}

// Recursively walk an HDF5 group, adding datasets and nested groups to @p parent.
// @p h5path is the group's absolute path, ending in '/'.
static void walk_group(hid_t loc, const string &h5path, D4Group *parent,
                       const S3Auth &auth, const string &bucket, const string &key)
{
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
                BaseType *v = build_dataset_var(dset, name, h5path + name, auth, bucket, key);
                if (v)
                    parent->add_var_nocopy(v);
                H5Dclose(dset);
            }
        }
        else if (oi.type == H5O_TYPE_GROUP) {
            hid_t grp = H5Gopen2(loc, name, H5P_DEFAULT);
            if (grp >= 0) {
                auto *g = new D4Group(name);
                g->set_is_dap4(true);
                walk_group(grp, h5path + name + "/", g, auth, bucket, key);
                parent->add_group_nocopy(g);
                H5Gclose(grp);
            }
        }
    }
}

// ---- entry point -----------------------------------------------------------

void build_dmr_object(DMR *dmr, const S3Auth &auth, const string &bucket,
                      const string &key, const string &name)
{
    dmr->set_name(name);

    H5S3Reader reader(auth);
    hid_t fid = reader.open(bucket, key);
    try {
        hid_t root = H5Gopen2(fid, "/", H5P_DEFAULT);
        if (root < 0)
            throw runtime_error("cannot open root group of " + key);
        walk_group(root, "/", dmr->root(), auth, bucket, key);
        H5Gclose(root);
    }
    catch (...) {
        H5Fclose(fid);
        throw;
    }
    H5Fclose(fid);
}

} // namespace h5s3
