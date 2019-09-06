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
installcheck: gpdiff.pl gpstringsubs.pl
	@cd gpload && ./TEST.py
	@cd gpload2 && ./TEST.py

clean distclean:
	rm -f gpdiff.pl atmsort.pm explain.pm GPTest.pm gpstringsubs.pl
