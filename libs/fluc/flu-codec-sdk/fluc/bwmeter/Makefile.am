MAINTAINERCLEANFILES = Makefile.in

noinst_LTLIBRARIES = libfluc_bwmeter.la

nobase_libfluc_bwmeterinclude_HEADERS = fluc_bwmeter.h
libfluc_bwmeterincludedir = $(includedir)/fluc/bwmeter

noinst_HEADERS = \
  fluc_bwmeter_base.h \
  fluc_bwmeter_sock.h \
  fluc_bwmeter_private.h

libfluc_bwmeter_la_SOURCES = \
  fluc_bwmeter.c \
  fluc_bwmeter_base.c \
  fluc_bwmeter_sock.c

libfluc_bwmeter_la_CFLAGS = \
  $(ERROR_CFLAGS) \
  $(GST_CFLAGS)

libfluc_bwmeter_la_LDFLAGS = -no-undefined
libfluc_bwmeter_la_LIBADD = $(GST_LIBS)
