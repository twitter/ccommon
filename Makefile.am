MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure config.h.in config.h.in~ stamp-h.in

## Place generated object files (.o) into the same directory as their source
## files, in order to avoid collisions when non-recursive make is used.
AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4 -I /usr/local/share/aclocal

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ccommon-$(CCOMMON_RELEASE_VERSION).pc

SUBDIRS = src . test
