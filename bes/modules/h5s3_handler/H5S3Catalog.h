// -*- mode: c++; c-basic-offset:4 -*-
//
// H5S3Catalog: a BES catalog that lists the HDF5 files recorded in the cached
// index.parquet (read via the out-of-process h5s3_index helper, since the
// Arrow/Parquet stack cannot share the BES process). Browsing this catalog
// shows the S3 bucket's HDF5 files; each is served by the h5s3 handler via ROS3.

#ifndef H5S3Catalog_h_
#define H5S3Catalog_h_ 1

#include <ostream>
#include <string>

#include "BESCatalog.h"

class BESCatalogEntry;
namespace bes { class CatalogNode; }

namespace h5s3 {

class H5S3Catalog : public BESCatalog {
    std::string d_index_path;   ///< full path to index.parquet
    std::string d_index_bin;    ///< path to the h5s3_index helper executable

public:
    explicit H5S3Catalog(const std::string &catalog_name);
    ~H5S3Catalog() override = default;

    std::string get_root() const override;

    BESCatalogEntry *show_catalog(const std::string &container, BESCatalogEntry *entry) override;

    bes::CatalogNode *get_node(const std::string &path) const override;

    void get_site_map(const std::string &prefix, const std::string &node_suffix,
                      const std::string &leaf_suffix, std::ostream &out,
                      const std::string &path = "/") const override;

    void dump(std::ostream &strm) const override;
};

} // namespace h5s3

#endif // H5S3Catalog_h_
