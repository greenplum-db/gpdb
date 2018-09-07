top_builddir = $(shell cd ../../../.. && pwd)
pgxs_dir = $(top_builddir)

config_file := \/home\/gpadmin\/s3\.conf
read_prefix := s3test\.pivotal\.io\/regress
write_prefix := s3test\.pivotal\.io\/regress\/s3write\/$(shell date +%Y%m%d)-$(shell cat /dev/urandom | env LC_ALL=C tr -dc 'a-zA-Z0-9' | fold -w 8 | head -n 1)
write_encrypt_prefix := s3test\.encrypt\.pivotal\.io\/regress\/s3write\/$(shell date +%Y%m%d)-$(shell cat /dev/urandom | env LC_ALL=C tr -dc 'a-zA-Z0-9' | fold -w 8 | head -n 1)

installcheck:
	@echo "[ Regression starts:" `date`
	@echo "The sub-directory for this test instance is: $(write_prefix)"
	@rm -rf source_replaced
	@mkdir -p source_replaced
	@cp -rf input source_replaced/input
	@cp -rf output source_replaced/output
	@ssh -CfNqT -D 1080 localhost
	@./generate_config_file.sh $(config_file)
	@perl -p -i -e 's/\@config_file\@/$(config_file)/;s/\@read_prefix\@/$(read_prefix)/;s/\@write_prefix\@/$(write_prefix)/;s/\@write_encrypt_prefix\@/$(write_encrypt_prefix)/' source_replaced/input/*.source source_replaced/output/*.source
	@cp $(top_builddir)/src/test/regress/init_file source_replaced/init_file
	@perl -p -i -e 's/-- end_matchsubs/\nm\/\\(\.\*\\.cpp:\\d\+\\)\/\ns\/\\(\.\*\\.cpp:\\d\+\\)\/\/\n-- end_matchsubs/' source_replaced/init_file
	-$(pgxs_dir)/src/test/regress/pg_regress --psqldir=$$GPHOME/bin/ --schedule=regress_schedule --init-file=source_replaced/init_file --inputdir=source_replaced --outputdir=.
	@pkill -xf "ssh -CfNqT -D 1080 localhost"
	@echo "Regression ends:" `date` ]

clean:
	rm -rf source_replaced sql expected results regression.*

.PHONY: installcheck clean
