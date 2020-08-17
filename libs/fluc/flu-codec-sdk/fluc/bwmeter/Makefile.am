MAINTAINERCLEANFILES = Makefile.in

noinst_LTLIBRARIES = libfluc_bwmeter.la
nobase_libfluc_bwmeterinclude_HEADERS = \
	fluc_bwmeter.h

libfluc_bwmeterincludedir = $(includedir)/fluc/bwmeter
libfluc_bwmeter_la_SOURCES = fluc_bwmeter.c fluc_bwmeter_base.c \
	fluc_bwmeter_sock.c

EXTRA_CFLAGS =
EXTRA_LIBS =
EXTRA_LDFLAGS =

libfluc_bwmeter_la_CFLAGS = \
	$(ERROR_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(EXTRA_CFLAGS)

libfluc_bwmeter_la_LIBADD = \
	$(GST_LIBS) \
	$(GST_BASE_LIBS) \
	$(EXTRA_LIBS)

libfluc_bwmeter_la_LDFLAGS = \
	$(EXTRA_LDFLAGS)
