MAINTAINERCLEANFILES = Makefile.in

plugindir = $(libdir)/gstreamer-@GST_MAJORMINOR@
plugin_LTLIBRARIES = libgstfluttml.la

libgstfluttml_la_SOURCES = \
fluttml.c \
gstttmlparse.c \
gstttmltype.c

libgstfluttml_la_CXXFLAGS = \
-I$(top_srcdir)/common \
-I$(top_builddir) \
 $(GST_CFLAGS) \
 $(GST_BASE_CFLAGS)

libgstfluttml_la_CFLAGS = \
-I$(top_srcdir)/common \
-I$(top_builddir) \
-I$(top_srcdir)/gst/ttml \
 $(GST_CFLAGS) \
 $(GST_BASE_CFLAGS)

libgstfluttml_la_LIBADD = \
$(GST_LIBS) \
$(GST_BASE_LIBS)

libgstfluttml_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(CPU_TUNE_LDFLAGS) \
  -no-undefined

libgstfluttml_la_DEPENDENCIES =

noinst_HEADERS = \
gstttmlparse.h \
gstttmltype.h \
gstttmlprivate.h

