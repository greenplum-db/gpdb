gppkg: buildgppkg
# get GP_MAJORVERSION from makefile in the installed gpdb folder
PGXS := $(shell pg_config --pgxs)
include $(PGXS)

buildgppkg:
	cat gppkg_spec.yml.in | sed "s/#gpver/$(GP_MAJORVERSION)/" > gppkg_spec.yml
	rm gppkg_spec.yml.in
	tar czf sample.gppkg *
