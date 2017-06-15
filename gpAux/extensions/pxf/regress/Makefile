top_builddir = $(shell cd ../../../.. && pwd)
pgxs_dir = $(top_builddir)

all: installcheck

installcheck:
	@echo "[ Regression starts:" `date`
	-$(pgxs_dir)/src/test/regress/pg_regress \
         --psqldir=$$GPHOME/bin/ \
         --schedule=pxf_schedule
	@echo "Regression ends:" `date` ]

clean:
	rm -rf results regression.*

.PHONY: installcheck clean
