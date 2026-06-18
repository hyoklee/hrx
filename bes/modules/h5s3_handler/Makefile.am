# Automake file for h5s3_handler
#
AUTOMAKE_OPTIONS = foreign

M_NAME = h5s3_handler
M_VER = 0.1.0

# Prefixes for the module's special dependencies. The HDF5 used here is the
# ROS3-enabled build (separate from the BES's bundled HDF5); Parquet/Arrow come
# from the conda environment. Override on the make command line if needed.
H5S3_HDF5_PREFIX = $(HOME)/hdf5-ros3
PARQUET_PREFIX   = $(HOME)/miniconda3

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/dispatch -I$(top_srcdir)/dap \
              -I$(top_srcdir)/xmlcommand -I$(top_srcdir)/http \
              -I$(H5S3_HDF5_PREFIX)/include $(DAP_CFLAGS)
AM_CPPFLAGS += -DMODULE_NAME=\"$(M_NAME)\" -DMODULE_VERSION=\"$(M_VER)\"

LIBADD = $(BES_DISPATCH_LIB) $(BES_HTTP_LIB) $(DAP_SERVER_LIBS) $(DAP_CLIENT_LIBS)

if USE_AWS_SDK
AM_CPPFLAGS += -I$(aws_prefix)/include
endif

lib_besdir = $(libdir)/bes
lib_bes_LTLIBRARIES = libh5s3_handler.la

# The in-process module: ROS3 reader + BES wiring. Links HDF5(ROS3) only --
# never Arrow or the AWS C++ SDK (kept out of process; see helpers below).
libh5s3_handler_la_SOURCES = H5S3Module.cc H5S3RequestHandler.cc H5S3Reader.cc \
                             H5S3Module.h H5S3RequestHandler.h H5S3Reader.h H5S3Names.h
libh5s3_handler_la_CPPFLAGS = $(AM_CPPFLAGS)
libh5s3_handler_la_LDFLAGS = -avoid-version -module -L$(H5S3_HDF5_PREFIX)/lib
libh5s3_handler_la_LIBADD = $(LIBADD) -lhdf5

# Out-of-process helpers installed alongside the module.
lib_bes_PROGRAMS = h5s3_list h5s3_index h5s3_dap

# Listing helper: AWS C++ SDK only.
h5s3_list_SOURCES = h5s3_list.cc
h5s3_list_CPPFLAGS = -I$(aws_prefix)/include
h5s3_list_LDFLAGS = -L$(aws_libdir)
h5s3_list_LDADD = -laws-cpp-sdk-s3 -laws-cpp-sdk-core

# Index helper: Arrow/Parquet only.
h5s3_index_SOURCES = h5s3_index_main.cc H5S3Index.cc H5S3Index.h
h5s3_index_CPPFLAGS = -I$(PARQUET_PREFIX)/include
h5s3_index_LDFLAGS = -L$(PARQUET_PREFIX)/lib
h5s3_index_LDADD = -lparquet -larrow

# DAP helper (CLI counterpart of the in-process reader): HDF5(ROS3) only.
h5s3_dap_SOURCES = h5s3_dap_main.cc H5S3Reader.cc H5S3Reader.h
h5s3_dap_CPPFLAGS = -I$(H5S3_HDF5_PREFIX)/include
h5s3_dap_LDFLAGS = -L$(H5S3_HDF5_PREFIX)/lib
h5s3_dap_LDADD = -lhdf5

EXTRA_DIST = h5s3.conf.in data tests/benchmark.sh
CLEANFILES = *~ h5s3.conf

moduledir = $(sysconfdir)/bes/modules
module_DATA = h5s3.conf

h5s3.conf: h5s3.conf.in $(top_builddir)/config.status
	sed -e "s%[@]bes_modules_dir[@]%${lib_besdir}%" \
	    -e "s%[@]datadir[@]%${datadir}%" $< > h5s3.conf
