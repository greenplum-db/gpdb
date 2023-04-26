# gpMgmt/bin/Makefile

default: install

top_builddir=../..
ifneq "$(wildcard $(top_builddir)/src/Makefile.global)" ""
include $(top_builddir)/src/Makefile.global
endif

SUBDIRS = stream gpcheckcat_modules gpconfig_modules gpssh_modules gppylib lib
SUBDIRS += ifaddrs

$(recurse)

PROGRAMS= analyzedb gpactivatestandby gpaddmirrors gpcheckcat gpcheckperf \
	gpcheckresgroupimpl gpconfig gpdeletesystem gpexpand gpinitstandby \
	gpinitsystem gpload gpload.py gplogfilter gpmovemirrors \
	gppkg gprecoverseg gpreload gpscp gpsd gpssh gpssh-exkeys gpstart \
	gpstate gpstop gpsys1 minirepro gpmemwatcher gpmemreport 

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)/lib'

installprograms: installdirs
	for file in $(PROGRAMS); do \
		$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(bindir)/'$$file ; \
		$(PERL) $(top_builddir)/putversion '$(DESTDIR)$(bindir)/'$$file ; \
	done
	# Symlink gpcheckcat from bin to bin/lib to maintain backward compatibility
	if [ ! -L $(DESTDIR)$(bindir)/lib/gpcheckcat  ]; then \
		$(LN_S) ../gpcheckcat $(DESTDIR)$(bindir)/lib/gpcheckcat; \
	fi
	$(INSTALL_DATA) gpload.bat '$(DESTDIR)$(bindir)/gpload.bat'

uninstall:
	for file in $(PROGRAMS); do \
		rm -f '$(DESTDIR)$(bindir)/'$$file ; \
	done
	rm -f '$(DESTDIR)$(bindir)/gpload.bat'

#
# SOURCE DIRECTORIES
#
SRC=$(CURDIR)
PYLIB_SRC=$(SRC)/pythonSrc
PYLIB_SRC_EXT=$(PYLIB_SRC)/ext
SBIN_DIR=$(SRC)/../sbin
SERVER_SRC=$(SRC)
SERVER_SBIN=$(SERVER_SRC)/../sbin


#
# INSTALL DIRECTORY
#
LIB_DIR=$(SRC)/lib
PYLIB_DIR=$(SRC)/ext

core: pygresql subprocess32
	python gpconfig_modules/parse_guc_metadata.py $(DESTDIR)$(prefix)

ifeq ($(with_pythonsrc_ext), yes)
install: installdirs installprograms core psutil pygresql pyyaml
else
install: installdirs installprograms core
endif

#
# Python Libraries
#

#
# PyGreSQL
#
PYGRESQL_VERSION=4.0
PYGRESQL_DIR=PyGreSQL-$(PYGRESQL_VERSION)

pygresql:
	@echo "--- PyGreSQL"
	if [ ! -f $(DESTDIR)$(prefix)/greenplum_path.sh ]; then \
		unset LIBPATH; \
		./generate-greenplum-path.sh > $(DESTDIR)$(prefix)/greenplum_path.sh ; \
	fi
	PATH=$(bindir):$$PATH && \
	if [ `uname -s` = 'OpenBSD' ]; then \
	    cd $(PYLIB_SRC)/$(PYGRESQL_DIR) && DESTDIR="$(DESTDIR)" CC=cc python setup.py build; \
	else \
	    cd $(PYLIB_SRC)/$(PYGRESQL_DIR) && DESTDIR="$(DESTDIR)" CC="$(CC)" LDFLAGS='$(LDFLAGS) $(PYGRESQL_LDFLAGS)' python setup.py build; \
	fi
	mkdir -p $(PYLIB_DIR)/pygresql
	if [ `uname -s` = 'Darwin' ]; then \
	  cp -r $(PYLIB_SRC)/$(PYGRESQL_DIR)/build/lib.macosx*/* $(PYLIB_DIR)/pygresql; \
	elif [ `uname -s` = 'OpenBSD' ]; then \
	  cp -r $(PYLIB_SRC)/$(PYGRESQL_DIR)/build/lib.openbsd*/* $(PYLIB_DIR)/pygresql; \
	else \
	  cp -r $(PYLIB_SRC)/$(PYGRESQL_DIR)/build/lib.linux*/* $(PYLIB_DIR)/pygresql; \
	fi
	touch $(PYLIB_DIR)/__init__.py

#
# subprocess32
#
subprocess32:
	@echo "--- subprocess32, Linux only"
	@if [ `uname -s` = 'Linux' ]; then \
		  cd $(PYLIB_SRC)/subprocess32 && CC="$(CC)" python setup.py build; \
		  cp -f $(PYLIB_SRC)/subprocess32/build/lib.*/* $(PYLIB_DIR)/;  \
		  cp -f $(PYLIB_SRC)/subprocess32/ChangeLog $(PYLIB_DIR)/subprocess32-ChangeLog;  \
	  fi

#
# PSUTIL
#
PSUTIL_VERSION=5.7.0
PSUTIL_DIR=psutil-$(PSUTIL_VERSION)

psutil:
	@echo "--- psutil"
	cd $(PYLIB_SRC_EXT)/ && tar xzf $(PSUTIL_DIR).tar.gz
	cd $(PYLIB_SRC_EXT)/$(PSUTIL_DIR)/ && CC="$(CC)" python setup.py build
	cp -r $(PYLIB_SRC_EXT)/$(PSUTIL_DIR)/build/lib.*/psutil $(PYLIB_DIR)


#
# PYYAML
#
PYYAML_VERSION=5.3.1
PYYAML_DIR=PyYAML-$(PYYAML_VERSION)

pyyaml:
	@echo "--- pyyaml"
	cd $(PYLIB_SRC_EXT)/ && tar xzf $(PYYAML_DIR).tar.gz
	cd $(PYLIB_SRC_EXT)/$(PYYAML_DIR)/ && env -u CC python setup.py build
	cp -r $(PYLIB_SRC_EXT)/$(PYYAML_DIR)/build/lib*/* $(PYLIB_DIR)

#
# PYLINT
#

PYLINT_VERSION=0.21.0
PYLINT_DIR=pylint-$(PYLINT_VERSION)
LOGILAB_ASTNG_VERSION=0.20.1
LOGILAB_ASTNG_DIR=logilab-astng-$(LOGILAB_ASTNG_VERSION)
LOGILAB_COMMON_VERSION=0.50.1
LOGILAB_COMMON_DIR=logilab-common-$(LOGILAB_COMMON_VERSION)
PYLINT_PYTHONPATH=$(PYLIB_DIR):$(PYLIB_SRC_EXT)/$(PYLINT_DIR)/build/lib/
MOCK_VERSION=1.0.1
MOCK_DIR=mock-$(MOCK_VERSION)
SETUP_TOOLS_VERSION=36.6.0
PARSE_VERSION=1.8.2
ARG_PARSE_VERSION=1.2.1
SETUP_TOOLS_DIR=setuptools-$(SETUP_TOOLS_VERSION)
PARSE_DIR=parse-$(PARSE_VERSION)
ARG_PARSE_DIR=argparse-$(ARG_PARSE_VERSION)
PYTHONSRC_INSTALL=$(PYLIB_SRC_EXT)/install
PYTHON_VERSION=$(shell python -c "import sys; print ('%s.%s' % (sys.version_info[0:2]))")
PYTHONSRC_INSTALL_SITE=$(PYLIB_SRC_EXT)/install/lib/python$(PYTHON_VERSION)/site-packages
PYTHONSRC_INSTALL_PYTHON_PATH=$(PYTHONPATH):$(PYTHONSRC_INSTALL_SITE)
# TODO: mock-1.0.1-py2.6.egg package should be updated.
MOCK_BIN=$(PYTHONSRC_INSTALL)/lib/python$(PYTHON_VERSION)/site-packages/mock-1.0.1-py2.6.egg
UBUNTU_PLATFORM=$(shell if lsb_release -a 2>/dev/null | grep -q 'Ubuntu' ; then echo "Ubuntu"; fi)

pylint:
	@echo "--- pylint"
	@cd $(PYLIB_SRC_EXT)/ && tar xzf $(PYLINT_DIR).tar.gz
	@cd $(PYLIB_SRC_EXT)/ && tar xzf $(LOGILAB_ASTNG_DIR).tar.gz
	@cd $(PYLIB_SRC_EXT)/ && tar xzf $(LOGILAB_COMMON_DIR).tar.gz
	@cd $(PYLIB_SRC_EXT)/$(PYLINT_DIR)/ && python setup.py build 1> /dev/null
	@cd $(PYLIB_SRC_EXT)/$(LOGILAB_ASTNG_DIR)/ && python setup.py build 1> /dev/null
	@cd $(PYLIB_SRC_EXT)/$(LOGILAB_COMMON_DIR)/ && python setup.py build 1> /dev/null
	@cp -r $(PYLIB_SRC_EXT)/$(LOGILAB_COMMON_DIR)/build/lib/logilab $(PYLIB_SRC_EXT)/$(PYLINT_DIR)/build/lib/
	@cp -r $(PYLIB_SRC_EXT)/$(LOGILAB_ASTNG_DIR)/build/lib/logilab $(PYLIB_SRC_EXT)/$(PYLINT_DIR)/build/lib/
	@touch $(PYLIB_SRC_EXT)/$(PYLINT_DIR)/build/lib/__init__.py
	@touch $(PYLIB_SRC_EXT)/$(PYLINT_DIR)/build/lib/logilab/__init__.py

$(MOCK_BIN):
	@echo "--- mock for platform $(UBUNTU_PLATFORM)"
	@if [ $(UBUNTU_PLATFORM) = "Ubuntu" ]; then\
		wget https://bootstrap.pypa.io/pip/2.7/get-pip.py && \
		python2 get-pip.py && \
		pip install mock==1.0.1;\
     else\
       mkdir -p $(PYTHONSRC_INSTALL_SITE) && \
	   cd $(PYLIB_SRC_EXT)/ && tar xzf $(MOCK_DIR).tar.gz && \
	   cd $(PYLIB_SRC_EXT)/$(MOCK_DIR)/ && \
	   PYTHONPATH=$(PYTHONSRC_INSTALL_PYTHON_PATH) python setup.py install --prefix $(PYTHONSRC_INSTALL) ; \
	 fi;

PYTHON_FILES=`grep -l --exclude=Makefile --exclude=gplogfilter "/bin/env python" *`\
			 `grep -l "/bin/env python" $(SRC)/../sbin/*`\
			 `find ./gppylib -name "*.py"`\
			 `find $(SRC)/../sbin -name "*.py"`

checkcode: pylint
	@echo "Running pylint on management scripts..."
	@PYTHONPATH=$(PYTHONPATH):$(PYLINT_PYTHONPATH) $(PYLIB_SRC_EXT)/$(PYLINT_DIR)/bin/pylint -i y $(PYTHON_FILES) --rcfile=.rcfile > $(SRC)/../pylint.txt || true
	@echo -n "pylint_score=" > $(SRC)/../pylint_score.properties
	@grep "Your code has been rated at" $(SRC)/../pylint.txt | sed -e "s|Your .* \(.*\)/.*|\1|" >> $(SRC)/../pylint_score.properties

check-regress:
	@echo "Running regression tests..."
	@PYTHONPATH=$(SRC):$(SRC)/ext:$(PYTHONPATH) \
	gppylib/gpunit discover --verbose -s gppylib -p "test_regress*.py" 2> $(SRC)/../gpMgmt_testregress_results.log 1> $(SRC)/../gpMgmt_testregress_output.log

check: $(MOCK_BIN)
	@echo "Running pure unit and also "unit" tests that require cluster to be up..."
	@TMPDIR=/tmp PYTHONPATH=$(SERVER_SRC):$(SERVER_SBIN):$(PYTHONPATH):$(PYTHONSRC_INSTALL_PYTHON_PATH):$(SRC)/ext:$(SBIN_DIR):$(LIB_DIR):$(PYLIB_DIR)/mock-1.0.1 \
	gppylib/gpunit discover --verbose -s $(SRC)/gppylib -p "test_unit*.py" 2> $(SRC)/../gpMgmt_testunit_results.log 1> $(SRC)/../gpMgmt_testunit_output.log
	@TMPDIR=/tmp PYTHONPATH=$(SERVER_SRC):$(SERVER_SBIN):$(PYTHONPATH):$(PYTHONSRC_INSTALL_PYTHON_PATH):$(SRC)/ext:$(SBIN_DIR):$(LIB_DIR):$(PYLIB_DIR)/mock-1.0.1 \
	gppylib/gpunit discover --verbose -s $(SRC)/gppylib -p "test_cluster*.py" 2>> $(SRC)/../gpMgmt_testunit_results.log 1>> $(SRC)/../gpMgmt_testunit_output.log

unitdevel:
	@echo "Running pure unit tests..."
	PYTHONPATH=$(SERVER_SRC):$(SERVER_SBIN):$(PYTHONPATH):$(PYTHONSRC_INSTALL_PYTHON_PATH):$(SRC)/ext:$(SBIN_DIR):$(LIB_DIR):$(PYLIB_DIR)/mock-1.0.1 \
	    python -m unittest discover --verbose -s $(SRC)/gppylib -p "test_unit*.py"


.PHONY: installcheck-bash
installcheck-bash:
	./test-generate-greenplum-path.bash
	./test/suite.bash

.PHONY: installcheck
installcheck: installcheck-bash
	$(MAKE) -C gpload_test $@

clean distclean:
	rm -rf $(PYLIB_SRC_EXT)/$(PYLINT_DIR)
	rm -rf $(PYLIB_SRC_EXT)/$(LOGILAB_COMMON_DIR)
	rm -rf $(PYLIB_SRC_EXT)/$(LOGILAB_ASTNG_DIR)
	rm -rf $(PYLIB_SRC_EXT)/$(PYGRESQL_DIR)/build
	rm -rf $(PYLIB_SRC)/$(PYGRESQL_DIR)/build
	rm -rf $(PYLIB_SRC)/subprocess32/build
	rm -rf *.pyc
	rm -f analyzedbc gpactivatestandbyc gpaddmirrorsc gpcheckcatc \
		  gpcheckperfc gpcheckresgroupimplc gpchecksubnetcfgc gpconfigc \
		  gpdeletesystemc gpexpandc gpinitstandbyc gplogfilterc gpmovemirrorsc \
		  gppkgc gprecoversegc gpreloadc gpscpc gpsdc gpssh-exkeysc gpsshc \
		  gpstartc gpstatec gpstopc gpsys1c minireproc
	rm -f gpconfig_modules/gucs_disallowed_in_file.txt
