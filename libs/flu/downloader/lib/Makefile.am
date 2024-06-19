MAINTAINERCLEANFILES = Makefile.in

noinst_LTLIBRARIES = libfludownloader.la
noinst_HEADERS = fludownloader.h fludownloaderhelper.h

libfludownloader_la_SOURCES = fludownloader.c fludownloader.h fludownloaderhelper.c fludownloaderhelper.h
libfludownloader_la_CFLAGS = $(FLUDOWNLOADER_CFLAGS) -I${top_srcdir}/../../libs/flu-codec-sdk -DPIC
libfludownloader_la_LIBADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/../../libs/flu-codec-sdk/fluc/libfluc_common.la
libfludownloader_la_LDFLAGS = $(WIN32_LDFLAGS)
