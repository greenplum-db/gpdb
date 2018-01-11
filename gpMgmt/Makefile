top_builddir = ..
include $(top_builddir)/src/Makefile.global
include $(top_builddir)/gpMgmt/Makefile.behave

#---------------------------------------------------------------------
# Set Version
#---------------------------------------------------------------------

SET_VERSION_SCRIPTS = \
	bin/gpactivatestandby \
	bin/gpaddmirrors \
	bin/gpcheckperf \
	bin/gpcrondump \
	bin/gpdbrestore \
	bin/gpdeletesystem \
	bin/gpexpand \
	bin/gpfilespace \
	bin/gpinitstandby \
	bin/gpinitsystem \
	bin/gpload.py \
	bin/gplogfilter \
	bin/gpmovemirrors \
	bin/gprecoverseg \
	bin/gpreload \
	bin/gpscp \
	bin/gpssh \
	bin/gpssh-exkeys \
	bin/gpstart \
	bin/gpstate \
	bin/gpstop \
	bin/gpsys1 \
	bin/gpcheckcat \
	sbin/gpaddconfig.py \
	sbin/gpchangeuserpassword \
	sbin/gpcleansegmentdir.py \
	sbin/gpfixuserlimts \
	sbin/gpgetstatususingtransition.py \
	sbin/gprepairmirrorseg.py \
	sbin/gpsegstart.py \
	sbin/gpsegstop.py \
	sbin/gpsegtoprimaryormirror.py \
	sbin/gpsetdbid.py \
	sbin/gpupgrademirror.py \
	lib/python/gppylib/programs/clsAddMirrors.py \
	lib/python/gppylib/programs/clsHostCacheLookup.py \
	lib/python/gppylib/programs/clsInjectFault.py \
	lib/python/gppylib/programs/clsRecoverSegment.py \
	lib/python/gppylib/programs/clsSystemState.py \
	lib/python/gppylib/programs/gppkg.py \
	lib/python/gppylib/programs/kill.py \
	lib/python/gppylib/mainUtils.py \
	$(NULL)

set_scripts_version : 
	@for file in $(SET_VERSION_SCRIPTS); do \
	    if [ -f $(DESTDIR)$(prefix)/$${file} ]; then \
	        perl $(top_builddir)/putversion $(DESTDIR)$(prefix)/$${file} ; \
	    fi ; \
	done

generate_greenplum_path_file:
	mkdir -p $(DESTDIR)$(prefix)
	unset LIBPATH; \
	bin/generate-greenplum-path.sh $(prefix) > $(DESTDIR)$(prefix)/greenplum_path.sh

install: generate_greenplum_path_file
	# Generate some python libraries
	$(MAKE) -C bin $@ prefix=$(DESTDIR)$(prefix)

	# Copy the management utilities
	mkdir -p $(DESTDIR)$(prefix)/bin
	mkdir -p $(DESTDIR)$(prefix)/lib
	mkdir -p $(DESTDIR)$(prefix)/lib/python
	mkdir -p $(DESTDIR)$(prefix)/sbin

	# Setup /lib/python contents
	cp -rp bin/gppylib $(DESTDIR)$(prefix)/lib/python
	if [ -e bin/ext/Crypto ]; then \
	    cp -rp bin/ext/Crypto $(DESTDIR)$(prefix)/lib/python ; \
	fi
	if [ -e bin/ext/__init__.py ]; then \
	    cp -rp bin/ext/__init__.py $(DESTDIR)$(prefix)/lib/python ; \
	fi
	if [ -e bin/ext/lockfile ]; then \
	    cp -rp bin/ext/lockfile $(DESTDIR)$(prefix)/lib/python ; \
	fi
	if [ -e bin/ext/paramiko ]; then \
	    cp -rp bin/ext/paramiko $(DESTDIR)$(prefix)/lib/python ; \
	fi
	if [ -e bin/ext/psutil ]; then \
	    cp -rp bin/ext/psutil $(DESTDIR)$(prefix)/lib/python ; \
	fi
	if [ -e bin/ext/pygresql ]; then \
	    cp -rp bin/ext/pygresql $(DESTDIR)$(prefix)/lib/python ; \
	fi
	if [ -e bin/ext/yaml ]; then \
	    cp -rp bin/ext/yaml $(DESTDIR)$(prefix)/lib/python ; \
	fi

	# Setup /bin contents
	cp -rp bin $(DESTDIR)$(prefix)
	# Symlink gpcheckcat from bin to bin/lib to maintain backward compatibility
	if [ -f $(DESTDIR)$(prefix)/bin/gpcheckcat  ]; then \
		ln -sf ../gpcheckcat $(DESTDIR)$(prefix)/bin/lib/gpcheckcat; \
	fi

#ifeq "$(findstring $(BLD_ARCH),$(GPPKG_PLATFORMS))" ""
#	@echo "Removing gppkg from distribution"
#	rm -f $(prefix)/bin/gppkg
#endif

	$(MAKE) -C sbin $@ prefix=$(DESTDIR)$(prefix)

	cp $(top_builddir)/src/test/regress/*.pl $(DESTDIR)$(prefix)/bin
	cp $(top_builddir)/src/test/regress/*.pm $(DESTDIR)$(prefix)/bin
	if [ ! -d ${DESTDIR}${prefix}/docs ] ; then mkdir ${DESTDIR}${prefix}/docs ; fi
	if [ -d doc ]; then cp -rp doc $(DESTDIR)$(prefix)/docs/cli_help; fi
	mkdir -p $(DESTDIR)$(prefix)/etc
	cp -p doc/gpconfigs/gpcheck.cnf $(DESTDIR)$(prefix)/etc/

#	THIS NEEDS TO MOVE
#	if [ -d demo/gpfdist_transform ]; then \
#	  mkdir -p $(prefix)/demo; \
#	  $(TAR) -C demo -czf $(prefix)/demo/gpfdist_transform.tar.gz gpfdist_transform; \
#	fi

	$(MAKE) set_scripts_version prefix=$(prefix)
	# Remove unwanted files.
	rm -rf $(DESTDIR)$(prefix)/bin/ext
	rm -rf $(DESTDIR)$(prefix)/bin/pythonSrc
	rm -rf $(DESTDIR)$(prefix)/bin/Makefile
	rm -rf $(DESTDIR)$(prefix)/bin/src
	rm -f $(DESTDIR)$(prefix)/bin/gpchecksubnetcfg
	rm -rf $(DESTDIR)$(prefix)/bin/gppylib
	rm -rf $(DESTDIR)$(prefix)/bin/gpload_test
	find $(DESTDIR)$(prefix)/lib/python/gppylib -name test -type d | xargs rm -rf

clean distclean:
	$(MAKE) -C bin $@
	$(MAKE) -C sbin $@
