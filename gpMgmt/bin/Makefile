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
	gppkg gprecoverseg gpreload gpsync gpsd gpssh gpssh-exkeys gpstart \
	gpstate gpstop minirepro gpmemwatcher gpmemreport gpcheckresgroupv2impl

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)/lib'

installprograms: installdirs
	for file in $(PROGRAMS); do \
		$(INSTALL_SCRIPT) $$file '$(DESTDIR)$(bindir)/'$$file ; \
		$(PERL) $(top_builddir)/putversion '$(DESTDIR)$(bindir)/'$$file ; \
	done
	# Symlink gpcheckcat from bin to bin/lib to maintain backward compatibility
	if [ ! -L $(DESTDIR)$(bindir)/lib/gpcheckcat  ]; then \
		cd $(DESTDIR)$(bindir)/lib/ && $(LN_S) ../gpcheckcat gpcheckcat; \
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

core:
	python3 gpconfig_modules/parse_guc_metadata.py $(DESTDIR)$(prefix)

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
PYGRESQL_VERSION=5.2
PYGRESQL_DIR=PyGreSQL-$(PYGRESQL_VERSION)
pygresql:
	@echo "--- PyGreSQL"
	cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(PYGRESQL_DIR).tar.gz
	cd $(PYLIB_SRC_EXT)/$(PYGRESQL_DIR)/ && PATH=$(DESTDIR)$(bindir):$$PATH LDFLAGS='$(LDFLAGS) $(PYGRESQL_LDFLAGS)' python3 setup.py build
	cp -r $(PYLIB_SRC_EXT)/$(PYGRESQL_DIR)/build/lib*-3*/* $(PYLIB_DIR)/


#
# PSUTIL
#
PSUTIL_VERSION=5.7.0
PSUTIL_DIR=psutil-$(PSUTIL_VERSION)

psutil:
	@echo "--- psutil"
ifeq "$(findstring $(BLD_ARCH),aix7_ppc_64 )" ""
	cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(PSUTIL_DIR).tar.gz
	cd $(PYLIB_SRC_EXT)/$(PSUTIL_DIR)/ && env -u CC python3 setup.py build
	cp -r $(PYLIB_SRC_EXT)/$(PSUTIL_DIR)/build/lib.*/psutil $(PYLIB_DIR)
endif

#
# PYYAML
#
PYYAML_VERSION=5.3.1
PYYAML_DIR=PyYAML-$(PYYAML_VERSION)

pyyaml:
	@echo "--- pyyaml"
	cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(PYYAML_DIR).tar.gz
	cd $(PYLIB_SRC_EXT)/$(PYYAML_DIR)/ && env -u CC python3 setup.py build
	cp -r $(PYLIB_SRC_EXT)/$(PYYAML_DIR)/build/lib*-3*/* $(PYLIB_DIR)/

#
# PYLINT
#

PYLINT_VERSION=0.21.0
PYLINT_DIR=pylint-$(PYLINT_VERSION)
PYLINT_PYTHONPATH=$(PYLIB_DIR):$(PYLIB_SRC_EXT)/$(PYLINT_DIR)/build/lib/
MOCK_VERSION=1.0.1
MOCK_DIR=mock-$(MOCK_VERSION)
SETUP_TOOLS_VERSION=36.6.0
PARSE_VERSION=1.8.2
SETUP_TOOLS_DIR=setuptools-$(SETUP_TOOLS_VERSION)
PARSE_DIR=parse-$(PARSE_VERSION)
PYTHONSRC_INSTALL=$(PYLIB_SRC_EXT)/install
PYTHON_VERSION=$(shell python3 -c "import sys; print ('%s.%s' % (sys.version_info[0:2]))")
PYTHONSRC_INSTALL_SITE=$(PYLIB_SRC_EXT)/install/lib/python$(PYTHON_VERSION)/site-packages
PYTHONSRC_INSTALL_PYTHON_PATH=$(PYTHONPATH):$(PYTHONSRC_INSTALL_SITE)
# TODO: mock-1.0.1-py2.6.egg package should be updated.
MOCK_BIN=$(PYTHONSRC_INSTALL)/lib/python$(PYTHON_VERSION)/site-packages/mock-1.0.1-py2.6.egg
UBUNTU_PLATFORM=$(shell if lsb_release -a 2>/dev/null | grep -q 'Ubuntu' ; then echo "Ubuntu"; fi)

pylint:
	@echo "--- pylint"
	@cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(PYLINT_DIR).tar.gz
	@cd $(PYLIB_SRC_EXT)/$(PYLINT_DIR)/ && python3 setup.py build 1> /dev/null
	@touch $(PYLIB_SRC_EXT)/$(PYLINT_DIR)/build/lib/__init__.py

$(MOCK_BIN):
	@echo "--- mock for platform $(UBUNTU_PLATFORM)"
	@if [ "$(UBUNTU_PLATFORM)" = "Ubuntu" ]; then\
       pip3 install mock;\
     else\
       mkdir -p $(PYTHONSRC_INSTALL_SITE) && \
	   cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(MOCK_DIR).tar.gz && \
	   cd $(PYLIB_SRC_EXT)/$(MOCK_DIR)/ && \
	   PYTHONPATH=$(PYTHONSRC_INSTALL_PYTHON_PATH) python3 setup.py install --prefix $(PYTHONSRC_INSTALL) ; \
	 fi;

PYTHON_FILES=`grep -l --exclude=Makefile --exclude=gplogfilter "/bin/env python3" *`\
			 `grep -l "/bin/env python3" $(SRC)/../sbin/*`\
			 `find ./gppylib -name "*.py"`\
			 `find $(SRC)/../sbin -name "*.py"`

checkcode: pylint
	@echo "Running pylint on management scripts..."
	@PYTHONPATH=$(PYTHONPATH):$(PYLINT_PYTHONPATH) $(PYLIB_SRC_EXT)/$(PYLINT_DIR)/bin/pylint -i y $(PYTHON_FILES) --rcfile=.rcfile > $(SRC)/../pylint.txt || true
	@echo -n "pylint_score=" > $(SRC)/../pylint_score.properties
	@grep "Your code has been rated at" $(SRC)/../pylint.txt | sed -e "s|Your .* \(.*\)/.*|\1|" >> $(SRC)/../pylint_score.properties

check: $(MOCK_BIN)
	@echo "Running pure unit and also "unit" tests that require cluster to be up..."
	@TMPDIR=/tmp PYTHONPATH=$(SERVER_SRC):$(SERVER_SBIN):$(PYTHONPATH):$(PYTHONSRC_INSTALL_PYTHON_PATH):$(SRC)/ext:$(SBIN_DIR):$(LIB_DIR):$(PYLIB_DIR)/mock-1.0.1 \
	gppylib/gpunit discover --verbose -s $(SRC)/gppylib -p "test_unit*.py" 2> $(SRC)/../gpMgmt_testunit_results.log 1> $(SRC)/../gpMgmt_testunit_output.log
	@TMPDIR=/tmp PYTHONPATH=$(SERVER_SRC):$(SERVER_SBIN):$(PYTHONPATH):$(PYTHONSRC_INSTALL_PYTHON_PATH):$(SRC)/ext:$(SBIN_DIR):$(LIB_DIR):$(PYLIB_DIR)/mock-1.0.1 \
	gppylib/gpunit discover --verbose -s $(SRC)/gppylib -p "test_cluster*.py" 2>> $(SRC)/../gpMgmt_testunit_results.log 1>> $(SRC)/../gpMgmt_testunit_output.log

unitdevel:
	@echo "Running pure unit tests..."
	PYTHONPATH=$(SERVER_SRC):$(SERVER_SBIN):$(PYTHONPATH):$(PYTHONSRC_INSTALL_PYTHON_PATH):$(SRC)/ext:$(SBIN_DIR):$(LIB_DIR):$(PYLIB_DIR)/mock-1.0.1 \
	    python3 -m unittest discover --verbose -s $(SRC)/gppylib -p "test_unit*.py"


.PHONY: installcheck-bash
installcheck-bash:
	./test/suite.bash

.PHONY: installcheck
installcheck: installcheck-bash
	$(MAKE) -C gpload_test $@

clean distclean:
	rm -rf $(PYLIB_SRC_EXT)/$(PYLINT_DIR)
	rm -rf $(PYLIB_SRC_EXT)/$(PYGRESQL_DIR)/build
	rm -rf $(PYLIB_SRC)/$(PYGRESQL_DIR)/build
	rm -rf *.pyc
	rm -f analyzedbc gpactivatestandbyc gpaddmirrorsc gpcheckcatc \
		  gpcheckperfc gpcheckresgroupimplc gpchecksubnetcfgc gpconfigc \
		  gpdeletesystemc gpexpandc gpinitstandbyc gplogfilterc gpmovemirrorsc \
		  gppkgc gprecoversegc gpreloadc gpscpc gpsyncc gpsdc gpssh-exkeysc gpsshc \
		  gpstartc gpstatec gpstopc minireproc gpcheckresgroupv2implc
	rm -f gpconfig_modules/gucs_disallowed_in_file.txt
