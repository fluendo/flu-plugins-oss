MAINTAINERCLEANFILES = Makefile.in

noinst_LTLIBRARIES = libfluc_threads.la
nobase_libfluc_threadsinclude_HEADERS = \
	fluc_mutex.h fluc_monitor.h fluc_barrier.h

libfluc_threadsincludedir = $(includedir)/fluc/threads
libfluc_threads_la_SOURCES = fluc_mutex.c fluc_monitor.c fluc_barrier.c

EXTRA_CFLAGS =
EXTRA_LIBS =
EXTRA_LDFLAGS =

libfluc_threads_la_CFLAGS = \
	$(ERROR_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(EXTRA_CFLAGS)

libfluc_threads_la_LIBADD = \
	$(GST_LIBS) \
	$(GST_BASE_LIBS) \
	$(EXTRA_LIBS)

libfluc_threads_la_LDFLAGS = \
	$(EXTRA_LDFLAGS)
