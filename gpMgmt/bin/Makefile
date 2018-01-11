
################################################################################ 
# Makefile  Makefile for Python Modules and other utilities
# Copyright Greenplum 2006-2011
################################################################################

default: install

top_builddir=../..
ifneq "$(wildcard $(top_builddir)/src/Makefile.global)" ""
include $(top_builddir)/src/Makefile.global
endif

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
	python gpconfig_modules/parse_guc_metadata.py $(prefix)

ifneq "$(wildcard $(CURDIR)/pythonSrc/ext/*.tar.gz)" ""
install: core lockfile paramiko pycrypto stream psutil
else
install: core stream
endif

#
# Python Libraries
#

#
#  STREAM which is used by gpcheckperf for memory bandwidth testing.
#
STREAM_DIR=$(SRC)/src/stream
stream:
	@echo "--- stream"
	cd $(STREAM_DIR) && NO_M64=TRUE $(CC) stream.c -o stream
	cp $(STREAM_DIR)/stream $(SERVER_SRC)/lib/stream

#
# PyGreSQL
#
PYGRESQL_VERSION=4.0
PYGRESQL_DIR=PyGreSQL-$(PYGRESQL_VERSION)

pygresql:
	@echo "--- PyGreSQL"
	. $(prefix)/greenplum_path.sh && unset PYTHONHOME && \
	if [ `uname -s` = 'HP-UX' ]; then \
	    cd $(PYLIB_SRC)/$(PYGRESQL_DIR) && DESTDIR="$(DESTDIR)" CC="$(CC)" LDFLAGS="-L../../../../gpAux/ext/hpux_ia64/python-2.5.6/lib" python setup.py build; \
	elif [ "$(BLD_ARCH)" = 'aix7_ppc_64' ]; then \
	    unset PYTHONPATH && cd $(PYLIB_SRC)/$(PYGRESQL_DIR) && DESTDIR="$(DESTDIR)" CC="$(CC)" python_64 setup.py build; \
	else \
	    cd $(PYLIB_SRC)/$(PYGRESQL_DIR) && DESTDIR="$(DESTDIR)" CC="$(CC)" python setup.py build; \
	fi
	mkdir -p $(PYLIB_DIR)/pygresql
	if [ `uname -s` = 'Darwin' ]; then \
	  cp -r $(PYLIB_SRC)/$(PYGRESQL_DIR)/build/lib.macosx*/* $(PYLIB_DIR)/pygresql; \
	elif [ `uname -s` = 'AIX' ]; then \
	  cp -r $(PYLIB_SRC)/$(PYGRESQL_DIR)/build/lib.aix*/* $(PYLIB_DIR)/pygresql; \
	else \
	  cp -r $(PYLIB_SRC)/$(PYGRESQL_DIR)/build/lib.linux*/* $(PYLIB_DIR)/pygresql; \
	fi
	touch $(PYLIB_DIR)/__init__.py

#
# PARAMIKO
#
PARAMIKO_VERSION=1.7.6-9
PARAMIKO_DIR=paramiko-$(PARAMIKO_VERSION)
paramiko:
	@echo "--- paramiko"
	cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(PARAMIKO_DIR).tar.gz
	cd $(PYLIB_SRC_EXT)/$(PARAMIKO_DIR)/ && python setup.py build
	cp -r $(PYLIB_SRC_EXT)/$(PARAMIKO_DIR)/build/lib*/paramiko $(PYLIB_DIR)/

#
# LOCKFILE
#
# note the awk commands are used to eliminate references to code in __init__.py
# that we don't use and also to insert a comment to tell pylint not to complain
# about these files since we are not in a position to correct those warnings.
#
LOCKFILE_VERSION=0.9.1
LOCKFILE_DIR=lockfile-$(LOCKFILE_VERSION)
LOCKFILE_SRC=$(PYLIB_SRC_EXT)/$(LOCKFILE_DIR)/build/lib*/lockfile
LOCKFILE_DST=$(PYLIB_DIR)/lockfile
lockfile:
	@echo "--- lockfile"
	cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(LOCKFILE_DIR).tar.gz
	cd $(PYLIB_SRC_EXT)/$(LOCKFILE_DIR)/ && python setup.py build
	mkdir -p $(LOCKFILE_DST)
	awk 'BEGIN{print "# pylint: disable-all"} /^if hasattr/ {exit} { print }' < `echo $(LOCKFILE_SRC)/__init__.py`    > $(LOCKFILE_DST)/__init__.py
	awk 'BEGIN{print "# pylint: disable-all"} { print }'                      < `echo $(LOCKFILE_SRC)/pidlockfile.py` > $(LOCKFILE_DST)/pidlockfile.py

subprocess32:
	@echo "--- subprocess32, Linux only"
	@if [ `uname -s` = 'Linux' ]; then \
		  cd $(PYLIB_SRC)/subprocess32 && CC="$(CC)" python setup.py build; \
		  cp -f $(PYLIB_SRC)/subprocess32/build/lib.*/* $(PYLIB_DIR)/;  \
	  fi

#
# PYCRYPTO
#
PYCRYPTO_VERSION=2.0.1
PYCRYPTO_DIR=pycrypto-$(PYCRYPTO_VERSION)

pycrypto:
	@echo "--- pycrypto"
ifeq "$(findstring $(BLD_ARCH),aix7_ppc_64 )" ""
	cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(PYCRYPTO_DIR).tar.gz
	cd $(PYLIB_SRC_EXT)/$(PYCRYPTO_DIR)/ && CC="$(CC)" python setup.py build
	cp -r $(PYLIB_SRC_EXT)/$(PYCRYPTO_DIR)/build/lib.*/Crypto $(PYLIB_DIR)
endif

#
# PSI
PSI_DIR=PSI-$(PSI_VERSION)

 psi:
	@echo "--- psi"
ifeq "$(findstring $(BLD_ARCH),aix7_ppc_64 )" ""
	cd $(PYLIB_SRC)/ && $(TAR) xzf $(PSI_DIR).tar.gz
	cd $(PYLIB_SRC)/$(PSI_DIR)/ && CC="$(CC)" python setup.py build
	cp -r $(PYLIB_SRC)/$(PSI_DIR)/build/lib.*/psi $(PYLIB_DIR)
endif


#
# PSUTIL
#
PSUTIL_VERSION=4.0.0
PSUTIL_DIR=psutil-$(PSUTIL_VERSION)

psutil:
	@echo "--- psutil"
ifeq "$(findstring $(BLD_ARCH),aix7_ppc_64 )" ""
	cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(PSUTIL_DIR).tar.gz
	cd $(PYLIB_SRC_EXT)/$(PSUTIL_DIR)/ && CC="$(CC)" python setup.py build
	cp -r $(PYLIB_SRC_EXT)/$(PSUTIL_DIR)/build/lib.*/psutil $(PYLIB_DIR)
endif

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
BEHAVE_VERSION=1.2.4
SETUP_TOOLS_VERSION=36.6.0
PARSE_VERSION=1.5.3
ARG_PARSE_VERSION=1.2.1
BEHAVE_DIR=behave-$(BEHAVE_VERSION)
SETUP_TOOLS_DIR=setuptools-$(SETUP_TOOLS_VERSION)
PARSE_DIR=parse-$(PARSE_VERSION)
ARG_PARSE_DIR=argparse-$(ARG_PARSE_VERSION)
PYTHONSRC_INSTALL=$(PYLIB_SRC_EXT)/install
PYTHON_VERSION=$(shell python -c "import sys; print '%s.%s' % (sys.version_info[0:2])")
PYTHONSRC_INSTALL_SITE=$(PYLIB_SRC_EXT)/install/lib/python$(PYTHON_VERSION)/site-packages
PYTHONSRC_INSTALL_PYTHON_PATH=$(PYTHONPATH):$(PYTHONSRC_INSTALL_SITE)
BEHAVE_BIN=$(PYTHONSRC_INSTALL)/bin/behave
# TODO: mock-1.0.1-py2.6.egg package should be updated.
MOCK_BIN=$(PYTHONSRC_INSTALL)/lib/python$(PYTHON_VERSION)/site-packages/mock-1.0.1-py2.6.egg
UBUNTU_PLATFORM=$(shell if lsb_release -a 2>/dev/null | grep -q 'Ubuntu' ; then echo "Ubuntu"; fi)

pylint:
	@echo "--- pylint"
	@cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(PYLINT_DIR).tar.gz
	@cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(LOGILAB_ASTNG_DIR).tar.gz
	@cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(LOGILAB_COMMON_DIR).tar.gz
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
       pip install mock;\
     else\
       mkdir -p $(PYTHONSRC_INSTALL_SITE) && \
	   cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(MOCK_DIR).tar.gz && \
	   cd $(PYLIB_SRC_EXT)/$(MOCK_DIR)/ && \
	   PYTHONPATH=$(PYTHONSRC_INSTALL_PYTHON_PATH) python setup.py install --prefix $(PYTHONSRC_INSTALL) ; \
	 fi;

#
# BEHAVE 
#

$(BEHAVE_BIN):
	@echo "--- build $(BEHAVE_BIN)"
	@mkdir -p $(PYTHONSRC_INSTALL_SITE)
	@cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(BEHAVE_DIR).tar.gz
	@cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(SETUP_TOOLS_DIR).tar.gz
	@cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(PARSE_DIR).tar.gz
	@cd $(PYLIB_SRC_EXT)/ && $(TAR) xzf $(ARG_PARSE_DIR).tar.gz
	@cd $(PYLIB_SRC_EXT)/$(SETUP_TOOLS_DIR)/ && PYTHONPATH=$(PYTHONSRC_INSTALL_PYTHON_PATH) python setup.py install --prefix $(PYTHONSRC_INSTALL)
	@cd $(PYLIB_SRC_EXT)/$(PARSE_DIR)/ && PYTHONPATH=$(PYTHONSRC_INSTALL_PYTHON_PATH) python setup.py install --prefix $(PYTHONSRC_INSTALL)
	@cd $(PYLIB_SRC_EXT)/$(ARG_PARSE_DIR)/ && PYTHONPATH=$(PYTHONSRC_INSTALL_PYTHON_PATH) python setup.py install --prefix $(PYTHONSRC_INSTALL)
	# We're forcing a change in the requirements since we're in python
	# 2.7.12. There's a new syntax in the python requirements for
	# parse_type 0.4.2 which breaks if you're under 2.7.13
	@cd $(PYLIB_SRC_EXT)/$(BEHAVE_DIR)/ && sed -i 's/parse_type>=0.3.4/parse_type==0.4.1/' setup.py \
		&& PYTHONPATH=$(PYTHONSRC_INSTALL_PYTHON_PATH) python setup.py install --prefix $(PYTHONSRC_INSTALL)
	@echo "--- behave done"

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
	python -m unittest discover --verbose -s $(SRC)/gppylib -p "test_unit*.py"

solarisTest:
	@if [ `uname -s` = 'SunOS' ]; then \
		echo "SOLARIS" ; \
	fi

.PHONY: installcheck
installcheck:
	$(MAKE) -C gpload_test $@

clean distclean:
	rm -rf $(PYLIB_SRC_EXT)/$(LOCKFILE_DIR)
	rm -rf $(PYLIB_SRC_EXT)/$(PARAMIKO_DIR)
	rm -rf $(PYLIB_SRC_EXT)/$(PYCRYPTO_DIR)
	rm -rf $(PYLIB_SRC_EXT)/$(PYLINT_DIR)
	rm -rf $(PYLIB_SRC_EXT)/$(LOGILAB_COMMON_DIR)
	rm -rf $(PYLIB_SRC_EXT)/$(LOGILAB_ASTNG_DIR)
	rm -rf $(PYLIB_SRC_EXT)/$(PYGRESQL_DIR)/build
	rm -rf $(PYLIB_SRC)/$(PYGRESQL_DIR)/build
	rm -rf $(PYLIB_SRC)/subprocess32/build
	rm -rf $(STREAM_DIR)/stream lib/stream
	rm -rf *.pyc lib/*.pyc
	rm -f gpcheckcatc gpcrondumpc gpexpandc gptransferc
	rm -f gpconfig_modules/gucs_disallowed_in_file.txt
	rm -f gppylib/data/$(GP_MAJORVERSION).json
