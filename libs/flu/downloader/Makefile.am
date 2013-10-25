MAINTAINERCLEANFILES = Makefile.in config.guess config.sub ltmain.sh config.h.in missing aclocal.m4 install-sh configure depcomp compile

SUBDIRS =

noinst_HEADERS =

noinst_LTLIBRARIES =
lib_LTLIBRARIES = libfludownloader.la libfludownloader_static.la
include_HEADERS = $(top_srcdir)/lib/fludownloader.h

libfludownloader_la_SOURCES = $(top_srcdir)/lib/fludownloader.c $(top_srcdir)/lib/fludownloader.h
libfludownloader_la_CFLAGS = $(FLUDOWNLOADER_CFLAGS)
libfludownloader_la_LIBADD = $(FLUDOWNLOADER_LIBS)
libfludownloader_la_LDFLAGS = $(WIN32_LDFLAGS) -shared

libfludownloader_static_la_SOURCES = $(libfludownloader_la_SOURCES)
libfludownloader_static_la_CFLAGS = $(libfludownloader_la_CFLAGS) -fPIC -DPIC
libfludownloader_static_la_LDFLAGS = $(WIN32_LDFLAGS) -static
libfludownloader_static_la_LIBADD = $(libfludownloader_la_LIBADD)

if BUILD_EXAMPLES
noinst_PROGRAMS = multidl

multidl_SOURCES = $(top_srcdir)/examples/multidl.c

multidl_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir) $(FLUDOWNLOADER_CFLAGS)

multidl_LDADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/libfludownloader.la
endif

pcfiles = \
	$(top_builddir)/fludownloader.pc \
	$(top_builddir)/fludownloader_static.pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)

EXTRA_DIST = libfludownloader.spec

include common/release.mak
