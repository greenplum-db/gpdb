top_builddir ?= $(shell cd ../../../.. && pwd)
include $(top_builddir)/src/Makefile.global

config_file := \/home\/gpadmin\/s3\.conf
read_prefix := s3test\.pivotal\.io\/regress
write_prefix := s3test\.pivotal\.io\/regress\/s3write\/$(shell date +%Y%m%d)-$(shell cat /dev/urandom | env LC_CTYPE=C tr -dc 'a-zA-Z0-9' | fold -w 8 | head -n 1)

installcheck:
	@echo "[ Regression starts:" `date`
	@echo "The sub-directory for this test instance is: $(write_prefix)"
	@rm -rf source_replaced
	@mkdir -p source_replaced
	@cp -rf input source_replaced/input
	@cp -rf output source_replaced/output
	@perl -p -i -e 's/\@config_file\@/$(config_file)/;s/\@read_prefix\@/$(read_prefix)/;s/\@write_prefix\@/$(write_prefix)/' source_replaced/input/*.source
	@perl -p -i -e 's/\@config_file\@/$(config_file)/;s/\@read_prefix\@/$(read_prefix)/;s/\@write_prefix\@/$(write_prefix)/' source_replaced/output/*.source
	-$(top_builddir)/src/test/regress/pg_regress --psqldir=$$GPHOME/bin/ --schedule=s3_regress_schedule --init-file=$(top_builddir)/src/test/regress/init_file --srcdir=source_replaced --inputdir=source_replaced --outputdir=.
	@echo "Regression ends:" `date` ]

clean:
	rm -rf source_replaced sql expected results regression.* optimizer_status.out

.PHONY: installcheck clean
