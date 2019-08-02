# gpMgmt/bin/gppylib/commands/Makefile

top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

DATA= __init__.py dca.py gp.py pg.py unix.py

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(libdir)/python/gppylib/commands'

install: installdirs
	$(INSTALL_SCRIPT) base.py '$(DESTDIR)$(libdir)/python/gppylib/commands/base.py'
	for file in $(DATA); do \
		$(INSTALL_DATA) $$file '$(DESTDIR)$(libdir)/python/gppylib/commands/'$$file ; \
	done

uninstall:
	for file in base.py $(DATA); do \
		rm -f '$(DESTDIR)$(libdir)/python/gppylib/commands/'$$file ; \
	done

clean distclean:
	rm -f *.pyc

