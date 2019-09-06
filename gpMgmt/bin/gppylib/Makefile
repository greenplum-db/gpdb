# gpMgmt/bin/gppylib/Makefile

top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

SUBDIRS= commands data db gpMgmttest operations programs system util

$(recurse)

PROGRAMS= gparray.py gpunit unit2

DATA= __init__.py datetimeutils.py fault_injection.py gp_era.py gpcatalog.py \
	gphostcache.py gplog.py gpparseopts.py gpresgroup.py gpsubprocess.py \
	gpversion.py heapchecksum.py logfilter.py mainUtils.py parseutils.py \
	pgconf.py userinput.py utils.py

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(libdir)/python/gppylib'

install: installdirs
	for file in $(PROGRAMS); do \
		$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(libdir)/python/gppylib/'$$file ; \
	done
	for file in $(DATA); do \
		$(INSTALL_DATA) $$file '$(DESTDIR)$(libdir)/python/gppylib/'$$file ; \
	done
	$(PERL) $(top_builddir)/putversion '$(DESTDIR)$(libdir)/python/gppylib/mainUtils.py'

uninstall:
	for file in $(PROGRAMS) $(DATA); do \
		rm -f '$(DESTDIR)$(libdir)/python/gppylib/'$$file ; \
	done

clean distclean:
	rm -f *.pyc

