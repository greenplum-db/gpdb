top_builddir = $(shell cd ../../../.. && pwd)
include $(top_builddir)/src/Makefile.global

installcheck:
	$(top_builddir)/src/test/regress/pg_regress --psqldir=$$GPHOME/bin/ --schedule=regress_schedule

clean:
	rm -rf results regression.* optimizer_status.out

.PHONY: installcheck clean
