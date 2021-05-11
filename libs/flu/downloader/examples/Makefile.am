MAINTAINERCLEANFILES = Makefile.in

if BUILD_EXAMPLES
noinst_PROGRAMS = multidl dl_helper
multidl_SOURCES = multidl.c
multidl_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir) $(FLUDOWNLOADER_CFLAGS)
multidl_LDADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/lib/libfludownloader.la
dl_helper_SOURCES = dl_helper.c
dl_helper_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir) $(FLUDOWNLOADER_CFLAGS)
dl_helper_LDADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/lib/libfludownloader.la

endif
