MAINTAINERCLEANFILES = Makefile.in config.guess config.sub ltmain.sh config.h.in missing aclocal.m4 install-sh configure depcomp compile

SUBDIRS =

noinst_HEADERS =

noinst_LTLIBRARIES =
lib_LTLIBRARIES = libfludownloader.la
include_HEADERS = $(top_srcdir)/lib/fludownloader.h

noinst_PROGRAMS = multidl

libfludownloader_la_SOURCES = $(top_srcdir)/lib/fludownloader.c $(top_srcdir)/lib/fludownloader.h

libfludownloader_la_CFLAGS = $(FLUDOWNLOADER_CFLAGS)

libfludownloader_la_LIBADD = $(FLUDOWNLOADER_LIBS)

libfludownloader_la_LDFLAGS =

multidl_SOURCES = $(top_srcdir)/examples/multidl.c

multidl_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir) $(FLUDOWNLOADER_CFLAGS)

multidl_LDADD = $(top_builddir)/libfludownloader.la

pcfiles = $(top_builddir)/fludownloader.pc
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)

EXTRA_DIST =

