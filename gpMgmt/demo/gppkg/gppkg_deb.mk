PGXS := $(shell pg_config --pgxs)
include $(PGXS)
include $(DIR)/release.mk
GP_VERSION_NUM := $(GP_MAJORVERSION)

ifeq ($(ARCH), x86_64)
ARCH=amd64
endif
PWD=$(shell pwd)
PLJAVA_DEB=pkgname-$(VERSION)-$(RELEASE).$(ARCH).deb
CONTROL_NAME=control.sample
TARGET_GPPKG=$(GPPKG)

.PHONY: distro
distro: $(TARGET_GPPKG)

%.deb:
	rm -rf UBUNTU 2>/dev/null
	mkdir UBUNTU/DEBIAN -p
	cat $(PWD)/$(CONTROL_NAME) | sed -r "s|#version|$(VERSION)-$(RELEASE)|" | sed -r "s|#arch|$(ARCH)|" > $(PWD)/UBUNTU/DEBIAN/control
	$(MAKE) -C $(DIR) install DESTDIR=$(PWD)/UBUNTU libdir=/lib/postgresql pkglibdir=/lib/postgresql datadir=/share/postgresql
	dpkg-deb --build $(PWD)/UBUNTU "$(DEB)"

gppkg_spec.yml: gppkg_spec.yml.in
	cat $< | sed "s/#arch/$(ARCH)/g" | sed "s/#os/$(OS)/g" | sed 's/#gpver/$(GP_VERSION_NUM)/g' | sed "s/#gppkgver/$(VERSION)/g" > $@

%.gppkg: gppkg_spec.yml $(DEB) $(DEPENDENT_DEBS)
	rm -rf gppkg
	mkdir -p gppkg/deps 
	cp gppkg_spec.yml gppkg/
	cp $(DEB) gppkg/ 
ifdef DEPENDENT_DEBS
	for dep_deb in $(DEPENDENT_DEBS); do \
		cp $${dep_deb} gppkg/deps; \
	done
endif
	gppkg --build gppkg 

clean:
	rm -rf UBUNTU
	rm -rf gppkg
	rm -f gppkg_spec.yml
	rm -f $(DEB)
	rm -f $(TARGET_GPPKG)
ifdef EXTRA_CLEAN
	rm -rf $(EXTRA_CLEAN)
endif

install: $(TARGET_GPPKG)
	gppkg -i $(TARGET_GPPKG)

.PHONY: install clean
