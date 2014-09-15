MAINTAINERCLEANFILES = Makefile.in

SUBDIRS =
DIST_SUBDIRS = curl

if !USE_SYSTEM_CURL
all-local:
	cd curl && $(MAKE)

clean-local:
	cd curl && $(MAKE) clean

dist-clean:
	cd curl && $(MAKE) distclean

mantainer-clean:
	cd curl && $(MAKE) maintainer-clean

endif
