top_builddir = $(shell cd ../../../.. && pwd)
include $(top_builddir)/src/Makefile.global

installcheck:
	$(top_builddir)/src/test/regress/pg_regress --psqldir=$$GPHOME/bin/ --schedule=s3_regress_schedule --init-file=$(top_builddir)/src/test/regress/init_file

clean:
	rm -rf sql expected results regression.* optimizer_status.out

.PHONY: installcheck clean
