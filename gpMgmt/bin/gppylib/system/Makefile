# gpMgmt/bin/gppylib/system/Makefile

top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

DATA= __init__.py configurationImplGpdb.py configurationInterface.py environment.py \
	faultProberImplGpdb.py faultProberInterface.py fileSystemImplOs.py fileSystemImplTest.py \
	fileSystemInterface.py info.py osImplNative.py osInterface.py

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(libdir)/python/gppylib/system'

install: installdirs
	$(INSTALL_SCRIPT) ComputeCatalogUpdate.py '$(DESTDIR)$(libdir)/python/gppylib/system/ComputeCatalogUpdate.py'
	for file in $(DATA); do \
		$(INSTALL_DATA) $$file '$(DESTDIR)$(libdir)/python/gppylib/system/'$$file ; \
	done

uninstall:
	for file in ComputeCatalogUpdate.py $(DATA); do \
		rm -f '$(DESTDIR)$(libdir)/python/gppylib/system/'$$file ; \
	done

clean distclean:
	rm -f *.pyc
