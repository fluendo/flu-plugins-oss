MAINTAINERCLEANFILES = Makefile.in

if BUILD_EXAMPLES
noinst_PROGRAMS = multidl
multidl_SOURCES = multidl.c
multidl_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir) $(FLUDOWNLOADER_CFLAGS)
multidl_LDADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/lib/libfludownloader.la
endif
