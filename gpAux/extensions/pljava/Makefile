#-------------------------------------------------------------------------
# Copyright (c) 2004, 2005, 2006 TADA AB - Taby Sweden
# Distributed under the terms shown in the file COPYRIGHT
# found in the root folder of this project or at
# http://eng.tada.se/osprojects/COPYRIGHT.html
#
# @author Thomas Hallgren
#
# Top level Makefile for PLJava
#
# To compile a PLJava for PostgreSQL 8.x the makefile system will utilize
# the PostgreSQL pgxs system. The only prerequisite for such a compile is
# that a PostgreSQL 8.x is installed on the system and that the PATH is set
# so that the binaries of this installed can be executed.
#
# The following options are recognized (aside from normal options like
# CFLAGS etc.)
#
#   PGSQLDIR=<pgsql root>  For old style (not pgxs based) compilation
#   USE_GCJ=1              Builds a shared object file containing both
#                          C and Java code. Requires GCJ 3.4 or later.
#
#-------------------------------------------------------------------------

# Can't use $(shell pwd) directly since /bin/pwd doesn't handle mounted
# volumes very well on some systems. The /mnt/hgfs that enables host shares
# in a vmware client is one example of this.
#
export PROJDIR          := $(shell $(SHELL) -c pwd)

PG_CONFIG = pg_config
export PGXS := $(shell $(PG_CONFIG) --pgxs)
export PGSQLSRC := $(dir $(PGXS))..
export top_builddir := $(PGSQLSRC)/..

export TARGETDIR		:= $(PROJDIR)/build
export SQLDIR			:= $(PROJDIR)/src/sql
export OBJDIR			:= $(TARGETDIR)/objs
export JNIDIR			:= $(TARGETDIR)/jni
export CLASSDIR			:= $(TARGETDIR)/classes
export PLJAVA_MAJOR_VER	:= 1
export PLJAVA_MINOR_VER	:= 4
export PLJAVA_PATCH_VER	:= 1
export PLJAVA_VERSION	:= $(PLJAVA_MAJOR_VER).$(PLJAVA_MINOR_VER).$(PLJAVA_PATCH_VER)
export TAR				:= /bin/tar

OS := $(shell uname -s)
MACHINE := $(shell uname -m)

.PHONY: all clean docs javadoc install uninstall depend release \
	c_all c_install c_uninstall c_depend \
	pljava_all pljava_javadoc \
	deploy_all deploy_javadoc \
	examples_all examples_javadoc \
	test_all test_javadoc install_extra

all: pljava_all deploy_all c_all examples_all

install: c_install

uninstall: c_uninstall
	rm -f $(pkglibdir)/pljava.jar
	rm -f -r $(datadir)/pljava

depend: c_depend

docs:
	@-mkdir -p $(TARGETDIR)
	@find docs \( \
		   -name CVS \
		-o -name .cvsignore \
		\) -prune -o \( -type f -exec cp --parents {} $(TARGETDIR) \; \)
	@cp COPYRIGHT $(TARGETDIR)/docs/COPYRIGHT.txt

javadoc: pljava_javadoc deploy_javadoc examples_javadoc

clean:
	@-rm -rf $(TARGETDIR)

pljava_all pljava_javadoc: pljava_%:
	@echo JAVA_HOME='$(JAVA_HOME)'
	@echo JAVAC_FLAGS='$(JAVAC_FLAGS)'
	@echo JAVAC=$(JAVAC)
	@echo JAVAH=$(JAVAH)
	@echo JAR=$(JAR)
	@echo JAVADOC=$(JAVADOC)
	@-mkdir -p $(CLASSDIR)/pljava
	$(MAKE) -r -C $(CLASSDIR)/pljava -f $(PROJDIR)/src/java/pljava/Makefile \
	MODULEROOT=$(PROJDIR)/src/java $*

deploy_all deploy_javadoc: deploy_%:
	@-mkdir -p $(CLASSDIR)/deploy
	@$(MAKE) -r -C $(CLASSDIR)/deploy -f $(PROJDIR)/src/java/deploy/Makefile \
	MODULEROOT=$(PROJDIR)/src/java $*

examples_all: examples_%: pljava_all
	@-mkdir -p $(CLASSDIR)/examples
	@$(MAKE) -r -C $(CLASSDIR)/examples -f $(PROJDIR)/src/java/examples/Makefile \
	MODULEROOT=$(PROJDIR)/src/java $*

test_all: test_%:
	@-mkdir -p $(CLASSDIR)/test
	@$(MAKE) -r -C $(CLASSDIR)/test -f $(PROJDIR)/src/java/test/Makefile \
	MODULEROOT=$(PROJDIR)/src/java $*


test:
	@-gpconfig -c pljava_classpath -v \'$(PWD)/build/\'
	@-echo 'host    all         test                 0.0.0.0/0 trust' >> $(MASTER_DATA_DIRECTORY)/pg_hba.conf
	@-echo 'local   all         test                trust' >> $(MASTER_DATA_DIRECTORY)/pg_hba.conf
	@-gpstop -ar
	cd src && \
	../../../../src/test/regress/pg_regress  --dbname=test --create-role=test pljava 

# GPDB: we've hacked the c_install rule in the sub-makefile to install
# examples.jar too. So examples.jar must be built before c_install.
c_install: examples_all

c_all c_install c_uninstall c_depend: c_%: pljava_all
	@-mkdir -p $(OBJDIR)
	@$(MAKE) -r -C $(OBJDIR) -f $(PROJDIR)/src/C/pljava/Makefile \
	MODULEROOT=$(PROJDIR)/src/C build_$*

release: all docs javadoc
	@-mkdir -p $(TARGETDIR)/distrib
	@$(MAKE) -r -C $(TARGETDIR) -f $(PROJDIR)/packaging/Makefile $@

