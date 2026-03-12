# Automake file for s3_reader
#
# 07/25/18
#
AUTOMAKE_OPTIONS = foreign

ACLOCAL_AMFLAGS = -I conf

# Set the module version here, in the spec file and in configure.ac
M_NAME = s3_reader
M_VER = 0.9.2

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/dispatch -I$(top_srcdir)/dap -I$(top_srcdir)/xmlcommand -I$(top_srcdir)/http -I$(top_srcdir)/aws -I$(top_srcdir)/modules/dmrpp_module/ngap_container $(DAP_CFLAGS)
LIBADD = $(BES_DISPATCH_LIB) $(BES_HTTP_LIB) $(DAP_SERVER_LIBS) $(DAP_CLIENT_LIBS)

AM_CPPFLAGS += -DMODULE_NAME=\"$(M_NAME)\" -DMODULE_VERSION=\"$(M_VER)\"

if USE_AWS_SDK
AM_CPPFLAGS += -I$(aws_prefix)/include
LIBADD += $(top_builddir)/aws/libbes_aws.la $(aws_libs)
AM_LDFLAGS += -L$(aws_libdir)
S3_EXTRA_LDFLAGS = -L$(aws_libdir)
endif

AM_CXXFLAGS=
include $(top_srcdir)/coverage.mk

SUBDIRS = . tests

lib_besdir=$(libdir)/bes

lib_bes_LTLIBRARIES = libs3_reader.la
libs3_reader_la_SOURCES = $(S3_SRC) $(S3_HDR)
libs3_reader_la_LDFLAGS = -avoid-version -module $(S3_EXTRA_LDFLAGS)
libs3_reader_la_LIBADD = $(LIBADD)

S3_SRC =  S3RequestHandler.cc S3Module.cc S3Container.cc S3ContainerStorage.cc
S3_HDR =  S3Names.h S3RequestHandler.h S3Module.h S3Container.h S3ContainerStorage.h

if USE_AWS_SDK
S3_SRC += BESS3Catalog.cc
S3_HDR += BESS3Catalog.h
endif

EXTRA_DIST = s3.conf.in data

CLEANFILES = *~ s3.conf

moduledir = $(sysconfdir)/bes/modules
module_DATA = s3.conf

s3.conf: s3.conf.in $(top_builddir)/config.status
	sed -e "s%[@]bes_modules_dir[@]%${lib_besdir}%" $< > s3.conf

C4_DIR=./cccc
.PHONY: cccc
cccc:	
	cccc --outdir=$(C4_DIR) $(S3_SRC) $(S3_HDR)


