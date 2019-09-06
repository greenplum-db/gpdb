# gpMgmt/bin/gppylib/util/Makefile

top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(libdir)/python/gppylib/util'

install: installdirs
	$(INSTALL_DATA) __init__.py '$(DESTDIR)$(libdir)/python/gppylib/util/__init__.py'
	$(INSTALL_DATA) gp_utils.py '$(DESTDIR)$(libdir)/python/gppylib/util/gp_utils.py'
	$(INSTALL_DATA) ssh_utils.py '$(DESTDIR)$(libdir)/python/gppylib/util/ssh_utils.py'

uninstall:
	rm -f '$(DESTDIR)$(libdir)/python/gppylib/util/__init__.py'
	rm -f '$(DESTDIR)$(libdir)/python/gppylib/util/gp_utils.py'
	rm -f '$(DESTDIR)$(libdir)/python/gppylib/util/ssh_utils.py'

clean distclean:
	rm -f *.pyc

