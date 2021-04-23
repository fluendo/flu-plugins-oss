MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libfludownloader.la libfludownloader_static.la
include_HEADERS = fludownloader.h fludownloaderhelper.h

libfludownloader_la_SOURCES = fludownloader.c fludownloader.h fludownloaderhelper.c fludownloaderhelper.h
libfludownloader_la_CFLAGS = $(FLUDOWNLOADER_CFLAGS) -I${top_srcdir}/../../libs/flu-codec-sdk
libfludownloader_la_LIBADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/../../libs/flu-codec-sdk/fluc/libfluc.la
libfludownloader_la_LDFLAGS = $(WIN32_LDFLAGS) -shared

libfludownloader_static_la_SOURCES = $(libfludownloader_la_SOURCES)
libfludownloader_static_la_CFLAGS = $(libfludownloader_la_CFLAGS) -DPIC
if !HAVE_WINDOWS
libfludownloader_static_la_CFLAGS += -fPIC
endif
libfludownloader_static_la_LDFLAGS = $(WIN32_LDFLAGS) -static
libfludownloader_static_la_LIBADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/../../libs/flu-codec-sdk/fluc/libfluc-static.la
