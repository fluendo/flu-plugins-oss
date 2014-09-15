MAINTAINERCLEANFILES = Makefile.in

if BUILD_EXAMPLES
noinst_PROGRAMS = multidl curl_pipelining_test
multidl_SOURCES = multidl.c
multidl_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir) $(FLUDOWNLOADER_CFLAGS)
multidl_LDADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/lib/libfludownloader.la

curl_pipelining_test_SOURCES = curl_pipelining_test.c
curl_pipelining_test_CFLAGS = $(FLUDOWNLOADER_CFLAGS)
curl_pipelining_test_LDADD = $(FLUDOWNLOADER_LIBS)
endif
