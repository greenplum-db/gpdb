#
# Makefile for GPDB PITR tests
#

top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

setup:
	cp $(top_builddir)/src/test/isolation2/sql/setup.sql ./sql/setup.sql
	cp $(top_builddir)/src/test/isolation2/expected/setup.out ./expected/setup.out

sql_isolation_testcase.py:
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/isolation2/sql_isolation_testcase.py

install: sql_isolation_testcase.py setup
	$(MAKE) -C $(top_builddir)/src/test/isolation2 install
	$(MAKE) -C $(top_builddir)/src/test/regress install

installcheck: install
	./test_gpdb_pitr.sh

clean:
	rm -f sql_isolation_testcase.py
	rm -f ./sql/setup.sql
	rm -f ./expected/setup.out
	./test_gpdb_pitr_cleanup.sh
