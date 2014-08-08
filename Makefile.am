MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config.h.in config.h.in~ stamp-h.in

# TODO: include default location on Linux if the path exists
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ccommon-$(CCOMMON_RELEASE_VERSION).pc

SUBDIRS = src . benchmark . test . demo
