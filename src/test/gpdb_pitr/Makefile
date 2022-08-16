#
# Makefile for GPDB PITR tests
#

top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

sql_isolation_testcase.py:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/isolation2/sql_isolation_testcase.py

install: sql_isolation_testcase.py
	$(MAKE) -C $(top_builddir)/src/test/isolation2 install
	$(MAKE) -C $(top_builddir)/src/test/regress install

installcheck: install
	./test_gpdb_pitr.sh

clean:
	./test_gpdb_pitr_cleanup.sh
