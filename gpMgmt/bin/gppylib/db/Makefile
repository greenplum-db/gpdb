# gpMgmt/bin/gppylib/db/Makefile

top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

DATA= __init__.py catalog.py dbconn.py

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(libdir)/python/gppylib/db'

install: installdirs
	for file in $(DATA); do \
		$(INSTALL_DATA) $$file '$(DESTDIR)$(libdir)/python/gppylib/db/'$$file ; \
	done

uninstall:
	for file in $(DATA); do \
		rm -f '$(DESTDIR)$(libdir)/python/gppylib/db/'$$file ; \
	done

clean distclean:
	rm -f *.pyc

