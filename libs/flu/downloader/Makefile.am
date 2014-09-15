MAINTAINERCLEANFILES = \
Makefile.in \
config.guess \
config.sub \
ltmain.sh \
config.h.in \
missing \
aclocal.m4 \
install-sh \
configure \
depcomp \
compile

SUBDIRS = ext lib examples
pcfiles = \
	$(top_builddir)/fludownloader.pc \
	$(top_builddir)/fludownloader_static.pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)

EXTRA_DIST = libfludownloader.spec

include common/release.mak
