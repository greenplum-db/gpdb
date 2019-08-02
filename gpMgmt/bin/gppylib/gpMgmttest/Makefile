# gpMgmt/bin/gppylib/gpMgmttest/Makefile

top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(libdir)/python/gppylib/gpMgmttest'

install: installdirs
	$(INSTALL_DATA) __init__.py '$(DESTDIR)$(libdir)/python/gppylib/gpMgmttest/__init__.py'

uninstall:
	rm -f '$(DESTDIR)$(libdir)/python/gppylib/gpMgmttest/__init__.py'

clean distclean:
	rm -f *.pyc

