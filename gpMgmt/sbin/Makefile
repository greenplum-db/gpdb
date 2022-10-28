# gpMgmt/sbin/Makefile

top_builddir = ../..
ifneq "$(wildcard $(top_builddir)/src/Makefile.global)" ""
include $(top_builddir)/src/Makefile.global
endif


PYCS= gpconfig_helper.pyc gpoperation.pyc gpsegstop.pyc gpcleansegmentdir.pyc \
	gpgetstatususingtransition.pyc gpsegstart.pyc seg_update_pg_hba.pyc \
	gpsegrecovery.pyc gpsegsetuprecovery.pyc recovery_base.pyc

PROGRAMS= gpconfig_helper.py gpoperation.py gpsegstop.py gpcleansegmentdir.py \
	gpgetstatususingtransition.py gpsegstart.py packcore seg_update_pg_hba.py \
	gpsegrecovery.py gpsegsetuprecovery.py recovery_base.py

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(sbindir)'

standalone_dir=./standalone/gpmgmt_sbin
installstandalone: installdirs
ifeq ($(enable_standalone_utils),yes)
	python3 ../setup.py build --force && \
	python3 ../setup.py install_exe --skip-build --install-dir="$(libdir)/$(standalone_dir)"; \
	for file_name in $(PROGRAMS); do \
		if [ -e $(libdir)/$(standalone_dir)/__frozen__$$file_name ]; then \
			ln -sf $(libdir)/$(standalone_dir)/__frozen__$$file_name $(DESTDIR)$(sbindir)/$$file_name; \
		fi \
	done
endif

install: installdirs installstandalone
	for file in $(PROGRAMS); do \
		if [ ! -e $(DESTDIR)$(sbindir)/$$file ]; then \
			$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(sbindir)/'$$file ; \
			$(PERL) $(top_builddir)/putversion '$(DESTDIR)$(sbindir)/'$$file ; \
		fi \
	done

uninstall:
	for file in $(PROGRAMS); do \
		rm -f '$(DESTDIR)$(sbindir)/'$$file ; \
	done

clean distclean:
	rm -rf build/
	rm -f $(PYCS)
