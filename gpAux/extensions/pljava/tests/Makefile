#-------------------------------------------------------------------------
# Copyright (c) 2004, 2005, 2006 TADA AB - Taby Sweden
# Distributed under the terms shown in the file COPYRIGHT
# found in the root folder of this project or at
# http://eng.tada.se/osprojects/COPYRIGHT.html
#
# @author Thomas Hallgren
#
#-------------------------------------------------------------------------

PLJAVADIR = $(shell $(SHELL) -c pwd)/..
TOPDIR    = ../../../..
REGRESS   = pljava_install pljava_test

.PHONY: all test

all: test

test:
	@-gpconfig -c pljava_classpath -v \'$(PLJAVADIR)/build/\'
	$(TOPDIR)/src/test/regress/pg_regress --dbname=pljava $(REGRESS)
