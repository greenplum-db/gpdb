##-------------------------------------------------------------------------------------
##
## Copyright (C) 2011 EMC - Data Computing Division (DCD)
##
## @doc: Engineering Services makefile utilities 
##
## @author: eespino
##
##-------------------------------------------------------------------------------------

.PHONY: opt_write_test sync_tools clean_tools

#-------------------------------------------------------------------------------------
# machine and OS properties
#-------------------------------------------------------------------------------------

UNAME = $(shell uname)
UNAME_P = $(shell uname -p)
UNAME_M = $(shell uname -m)

UNAME_ALL = $(UNAME).$(UNAME_P)

# shared lib support
ifeq (Darwin, $(UNAME))
	LDSFX = dylib
else
	LDSFX = so
endif

ifeq (x86_64, $(UNAME_M))
	ARCH_FLAGS = -m64
else
	ARCH_FLAGS = -m32
endif

##-------------------------------------------------------------------------------------
## dependent modules
##
## NOTE: Dependent project module version is kept in $(BLD_TOP)/releng/make/dependencies/ivy.xml
##-------------------------------------------------------------------------------------

GREP_SED_VAR = $(BLD_TOP)/releng/make/dependencies/ivy.xml | sed -e 's|\(.*\)rev="\(.*\)"[ \t]*conf\(.*\)|\2|'

## ---------------------------------------
## R-Project support
## ---------------------------------------

R_VER = $(shell grep 'name="R"' $(GREP_SED_VAR))

ifneq "$(wildcard /opt/releng/tools/R-Project/R/$(R_VER)/$(BLD_ARCH)/lib64)" ""
R_HOME = /opt/releng/tools/R-Project/R/$(R_VER)/$(BLD_ARCH)/lib64/R
else
ifneq "$(wildcard /opt/releng/tools/R-Project/R/$(R_VER)/$(BLD_ARCH)/lib)" ""
R_HOME = /opt/releng/tools/R-Project/R/$(R_VER)/$(BLD_ARCH)/lib/R
endif
endif

display_dependent_vers:
	@echo ""
	@echo "======================================================================"
	@echo " R_HOME ........ : $(R_HOME)"
	@echo " R_VER ......... : $(R_VER)"
	@echo " CONFIGFLAGS ... : $(CONFIGFLAGS)"
	@echo "======================================================================"

## ----------------------------------------------------------------------
## Sync/Clean tools
## ----------------------------------------------------------------------
## Populate/clean up dependent releng supported tools.  The projects are
## downloaded and installed into /opt/releng/...
##
## Tool dependencies and platform config mappings are defined in:
##   * Apache Ivy dependency definition file
##       releng/make/dependencies/ivy.xml
## ----------------------------------------------------------------------

opt_write_test:
	@if [ ! -e /opt/releng -o ! -w /opt/releng ] && [ ! -w /opt ]; then \
	    echo ""; \
	    echo "======================================================================"; \
	    echo "ERROR: /opt is not writable."; \
	    echo "----------------------------------------------------------------------"; \
	    echo "  Supporting tools are stored in /opt.  Please ensure you have"; \
	    echo "  write access to /opt"; \
	    echo "======================================================================"; \
	    echo ""; \
	    exit 1; \
	fi

/opt/releng/apache-ant: 
	${MAKE} opt_write_test
	echo "Sync Ivy project dependency management framework ..."
	type curl; \
	if [ $$? = 0 ]; then curl --silent http://releng.sanmateo.greenplum.com/tools/apache-ant.1.8.1.tar.gz -o /tmp/apache-ant.1.8.1.tar.gz; \
	else wget -q http://releng.sanmateo.greenplum.com/tools/apache-ant.1.8.1.tar.gz -O /tmp/apache-ant.1.8.1.tar.gz; fi; \
	( umask 002; [ ! -d /opt/releng ] && mkdir -p /opt/releng; \
	   cd /opt/releng; \
	   gunzip -qc /tmp/apache-ant.1.8.1.tar.gz | tar xf -; \
	   rm -f /tmp/apache-ant.1.8.1.tar.gz; \
	   chmod -R a+w /opt/releng/apache-ant )

# ----------------------------------------------------------------------
# Populate dependent internal and thirdparty dependencies.  This
# will be retrieved and place in "ext" directory in root
# directory.
# ----------------------------------------------------------------------

sync_tools: opt_write_test /opt/releng/apache-ant
	@if [ -d /opt/releng/tools ]; then \
	    LCK_FILES=$$( find /opt/releng/tools -name "*.lck" ); \
	    if [ -n "$${LCK_FILES}" ]; then \
	        echo "Removing existing .lck files!"; \
	        find /opt/releng/tools -name "*.lck" | xargs rm; \
	    fi \
	fi

	@cd releng/make/dependencies; \
	 (umask 002; ANT_OPTS="-Djavax.net.ssl.trustStore=$(BLD_TOP)/releng/make/dependencies/cacerts" \
	/opt/releng/apache-ant/bin/ant -DBLD_ARCH=$(BLD_ARCH) \
	-Divyrepo.host=$(IVYREPO_HOST) -Divyrepo.realm="$(IVYREPO_REALM)" \
	-Divyrepo.user=$(IVYREPO_USER) -Divyrepo.passwd="$(IVYREPO_PASSWD)" -quiet resolve);

ifeq "$(findstring aix,$(BLD_ARCH))" ""
	LD_LIBRARY_PATH='' wget -q -O - https://github.com/greenplum-db/gporca/releases/download/v2.53.7/bin_orca_centos5_release.tar.gz | tar zxf - -C $(BLD_TOP)/ext/$(BLD_ARCH)
endif

clean_tools: opt_write_test
	@cd releng/make/dependencies; \
	/opt/releng/apache-ant/bin/ant clean; \
	rm -rf /opt/releng/apache-ant; \
