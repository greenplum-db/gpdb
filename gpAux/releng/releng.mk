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
## Removing EXTRA_EXT for osx as we do copy libgpos and optimizer into ext and
## we will be using it from there rather than /opt
##-------------------------------------------------------------------------------------

# by default use optimized build libraries of GP Optimizer
# use 'make BLD_TYPE=debug' to work with debug build libraries of GP Optimizer
BLD_TYPE=opt

OBJDIR_DEFAULT = .obj.$(UNAME_ALL)$(ARCH_FLAGS).$(BLD_TYPE)

GREP_SED_VAR = $(BLD_TOP)/releng/make/dependencies/ivy.xml | sed -e 's|\(.*\)rev="\(.*\)"[ \t]*conf\(.*\)|\2|'

XERCES_VER  = $(shell grep "\"xerces-c\""    $(GREP_SED_VAR))
LIBGPOS_VER = $(shell grep "\"libgpos\""     $(GREP_SED_VAR))
OPTIMIZER_VER = $(shell grep "\"optimizer\"" $(GREP_SED_VAR))

LIBSTDC++_VER = $(shell grep "\"libstdc\""   $(GREP_SED_VAR))

XERCES = $(BLD_TOP)/ext/$(BLD_ARCH)
XERCES_LIBDIR = $(XERCES)/lib

LIBGPOS = $(BLD_TOP)/ext/$(BLD_ARCH)/libgpos
LIBGPOS_LIBDIR = $(LIBGPOS)/$(OBJDIR_DEFAULT)

OPTIMIZER = $(BLD_TOP)/ext/$(BLD_ARCH)
LIBGPOPT_LIBDIR = $(OPTIMIZER)/libgpopt/$(OBJDIR_DEFAULT)
LIBGPOPTUDF_LIBDIR = $(OPTIMIZER)/libgpoptudf/$(OBJDIR_DEFAULT)
LIBNAUCRATES_LIBDIR = $(OPTIMIZER)/libnaucrates/$(OBJDIR_DEFAULT)
LIBGPDBCOST_LIBDIR = $(OPTIMIZER)/libgpdbcost/$(OBJDIR_DEFAULT)

LIBSTDC++_BASEDIR = $(BLD_TOP)/ext/$(BLD_ARCH)

ifeq (Darwin, $(UNAME))
LIBSTDC++_LIBDIR = $(LIBSTDC++_BASEDIR)/lib
endif

ifeq "$(BLD_ARCH)" "rhel5_x86_32"
LIBSTDC++_LIBDIR = $(LIBSTDC++_BASEDIR)/lib
endif

ifeq "$(BLD_ARCH)" "rhel5_x86_64"
LIBSTDC++_LIBDIR = $(LIBSTDC++_BASEDIR)/lib64
endif

ifeq "$(BLD_ARCH)" "suse10_x86_64"
LIBSTDC++_LIBDIR = $(LIBSTDC++_BASEDIR)/lib64
endif

ifeq "$(BLD_ARCH)" "suse11_x86_64"
LIBSTDC++_LIBDIR = $(LIBSTDC++_BASEDIR)/lib64
endif

ifeq "$(BLD_ARCH)" "sol10_x86_32"
LIBSTDC++_LIBDIR = $(LIBSTDC++_BASEDIR)/lib
endif

ifeq "$(BLD_ARCH)" "sol10_x86_64"
LIBSTDC++_LIBDIR = $(LIBSTDC++_BASEDIR)/lib/amd64
endif


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
	else wget http://releng.sanmateo.greenplum.com/tools/apache-ant.1.8.1.tar.gz -O /tmp/apache-ant.1.8.1.tar.gz; fi; \
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
	@LCK_FILES=$$( find /opt/releng/tools -name "*.lck" ); \
	if [ -n "$${LCK_FILES}" ]; then \
		echo "Removing existing .lck files!"; \
		find /opt/releng/tools -name "*.lck" | xargs rm; \
	fi

	@cd releng/make/dependencies; \
	 (umask 002; ANT_OPTS="-Djavax.net.ssl.trustStore=$(BLD_TOP)/releng/make/dependencies/cacerts" /opt/releng/apache-ant/bin/ant -DBLD_ARCH=$(BLD_ARCH) resolve);
	@echo "Resolve finished";
ifneq "$(findstring rhel,$(BLD_ARCH))" ""
	@echo "Init Anaconda for building"
	@if [ ! -d $(BLD_TOP)/ext/$(BLD_ARCH)/conda/.index ]; then  $(BLD_TOP)/ext/$(BLD_ARCH)/conda/init.sh; fi
endif

clean_tools: opt_write_test
	@cd releng/make/dependencies; \
	/opt/releng/apache-ant/bin/ant clean; \
	rm -rf /opt/releng/apache-ant; \
