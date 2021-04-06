MAINTAINERCLEANFILES = Makefile.in

noinst_LTLIBRARIES = libfluc_threads.la

nobase_libfluc_threadsinclude_HEADERS = \
  fluc_threads.h \
  fluc_mutex.h \
  fluc_monitor.h \
  fluc_barrier.h
libfluc_threadsincludedir = $(includedir)/fluc/threads

libfluc_threads_la_SOURCES = \
  fluc_mutex.c \
  fluc_monitor.c \
  fluc_barrier.c

libfluc_threads_la_CFLAGS = \
  $(ERROR_CFLAGS) \
  $(GST_CFLAGS)

libfluc_threads_la_LDFLAGS = -no-undefined
libfluc_threads_la_LIBADD = $(GST_LIBS)
