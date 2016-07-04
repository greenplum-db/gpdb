TOP_BUILDDIR = $(shell cd ../../../.. && pwd)
include $(TOP_BUILDDIR)/src/Makefile.global

installcheck:
	$(TOP_BUILDDIR)/src/test/regress/pg_regress --psqldir=$$GPHOME/bin/ --schedule=regress_schedule

clean:
	rm -rf results regression.* optimizer_status.out

.PHONY: installcheck clean
