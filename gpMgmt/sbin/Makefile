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

install: installdirs
	for file in $(PROGRAMS); do \
		$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(sbindir)/'$$file ; \
		$(PERL) $(top_builddir)/putversion '$(DESTDIR)$(sbindir)/'$$file ; \
	done

uninstall:
	for file in $(PROGRAMS); do \
		rm -f '$(DESTDIR)$(sbindir)/'$$file ; \
	done

clean distclean:
	rm -f $(PYCS)
