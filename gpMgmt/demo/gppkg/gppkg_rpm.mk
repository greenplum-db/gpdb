PGXS := $(shell pg_config --pgxs)
include $(PGXS)
include $(DIR)/release.mk
GP_VERSION_NUM := $(GP_MAJORVERSION)

RPM_FLAGS=--define 'dir $(DIR)' --define 'ver $(VERSION)' --define 'rel $(RELEASE)'
RPM=pkgname-$(VERSION)-$(RELEASE).$(ARCH).rpm
SPEC_NAME=pkg.spec
TARGET_GPPKG=$(GPPKG)
PWD=$(shell pwd)

.PHONY: distro
distro: $(TARGET_GPPKG)

%.rpm: 
	rm -rf RPMS BUILD SPECS
	mkdir RPMS BUILD SPECS
	cp $(SPEC_NAME) SPECS/
	rpmbuild -bb SPECS/$(SPEC_NAME) --buildroot $(PWD)/BUILD --define '_topdir $(PWD)' --define '__os_install_post \%{nil}' --define 'buildarch $(ARCH)' $(RPM_FLAGS)
	mv RPMS/$(ARCH)/$*.rpm .
	rm -rf RPMS BUILD SPECS

gppkg_spec.yml: gppkg_spec.yml.in
	cat $< | sed "s/#arch/$(ARCH)/g" | sed "s/#os/$(OS)/g" | sed 's/#gpver/$(GP_VERSION_NUM)/g' | sed "s/#gppkgver/$(VERSION)/g"> $@ > $@

%.gppkg: $(RPM) gppkg_spec.yml $(RPM) $(DEPENDENT_RPMS)
	rm -rf gppkg
	mkdir -p gppkg/deps 
	cp gppkg_spec.yml gppkg/
	cp $(PLJAVA_RPM) gppkg/ 
ifdef DEPENDENT_RPMS
	for dep_rpm in $(DEPENDENT_RPMS); do \
		cp $${dep_rpm} gppkg/deps; \
	done
endif
	gppkg --build gppkg 

clean:
	rm -rf RPMS BUILD SPECS
	rm -rf gppkg
	rm -f gppkg_spec.yml
	rm -rf BUILDROOT
	rm -rf SOURCES
	rm -rf SRPMS
	rm -rf $(RPM)
	rm -rf $(TARGET_GPPKG)
ifdef EXTRA_CLEAN
	rm -rf $(EXTRA_CLEAN)
endif

install: $(TARGET_GPPKG)
	gppkg -i $(TARGET_GPPKG)

.PHONY: install clean
