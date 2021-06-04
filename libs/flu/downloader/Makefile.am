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

EXTRA_DIST = libfludownloader.spec common/glib-compat.h

include common/release.mak

if !USE_SYSTEM_CURL
clean-local: clean-local-check
.PHONY: clean-local-check
clean-local-check:
	$(MAKE) -C $(top_builddir)/ext/curl distclean
endif
