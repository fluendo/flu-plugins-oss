MAINTAINERCLEANFILES = Makefile.in

if BUILD_EXAMPLES
noinst_PROGRAMS = multidl dl_helper
multidl_SOURCES = multidl.c
multidl_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir) $(FLUDOWNLOADER_CFLAGS)
if WITH_STATIC_FLUC
multidl_LDADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/lib/libfludownloader_static.la
else
multidl_LDADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/lib/libfludownloader.la
endif
dl_helper_SOURCES = dl_helper.c
dl_helper_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir) $(FLUDOWNLOADER_CFLAGS)
if WITH_STATIC_FLUC
dl_helper_LDADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/lib/libfludownloader_static.la
else
dl_helper_LDADD = $(FLUDOWNLOADER_LIBS) $(top_builddir)/lib/libfludownloader.la
endif

endif
