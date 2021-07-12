OS := $(shell cat /etc/redhat-release)
top_builddir=../../..
include $(top_builddir)/src/Makefile.global

gpstringsubs.pl:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/gpstringsubs.pl

gpdiff.pl: atmsort.pm explain.pm GPTest.pm
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/gpdiff.pl

atmsort.pm:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/atmsort.pm

explain.pm:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/explain.pm

GPTest.pm:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/GPTest.pm

.PHONY: installcheck
	
ifeq "$(findstring ubuntu, $(TEST_OS))" "ubuntu"
installcheck: gpdiff.pl gpstringsubs.pl
	@echo "skip gpload test for ubuntu"
else ifeq "$(findstring CentOS release 6., $(OS))" "centos6"
installcheck: gpdiff.pl gpstringsubs.pl
	@echo "skip gpload test for centos6"
else
installcheck: gpdiff.pl gpstringsubs.pl
	@cd gpload && ./TEST.py
	@export PYTHONPATH=$PYTHONPATH:/usr/lib/python2.7/site-packages/:/usr/lib64/python2.7/site-packages/:/usr/local/greenplum-db-devel/lib/python\
	&& cd gpload2 && python -m pytest TEST_local_*
endif

clean distclean:
	rm -f gpdiff.pl atmsort.pm explain.pm GPTest.pm gpstringsubs.pl
