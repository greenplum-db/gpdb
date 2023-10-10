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
else
installcheck: gpdiff.pl gpstringsubs.pl
	@echo "doing test in OS: "; echo $(TEST_OS)
	@cd gpload && ./TEST.py
	@cd gpload2 && export PGSSLMODE=disable && PYTHONIOENCODING=utf-8 pytest TEST_local_*
endif

clean distclean:
	rm -f gpdiff.pl atmsort.pm explain.pm GPTest.pm gpstringsubs.pl
