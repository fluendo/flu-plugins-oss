MAINTAINERCLEANFILES = Makefile.in

SUBDIRS =

if !USE_SYSTEM_CURL
all-local:
	cd curl && $(MAKE)

clean-local:
	cd curl && $(MAKE) clean

dist-clean:
	cd curl && $(MAKE) distclean
	rm -f Makefile

mantainer-clean:
	cd curl && $(MAKE) maintainer-clean
endif

distdir:
	tar --exclude='.git' -c -f - -C $(top_srcdir)/ext/ curl | (cd ${distdir} ; tar -xpf -)
	cp -f $(top_srcdir)/ext/Makefile.am $(top_srcdir)/ext/Makefile.in ${distdir}
