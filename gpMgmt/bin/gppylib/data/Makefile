# gpMgmt/bin/gppylib/data/Makefile

top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

CATALOG_JSON= $(addsuffix .json,$(GP_MAJORVERSION))

$(CATALOG_JSON):
	$(MAKE) -C $(top_builddir)/src/backend/catalog catalog_json

all: $(CATALOG_JSON)

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(libdir)/python/gppylib/data'

install: all installdirs
	$(INSTALL_DATA) $(CATALOG_JSON) '$(DESTDIR)$(libdir)/python/gppylib/data/$(CATALOG_JSON)'

uninstall:
	rm -f '$(DESTDIR)$(libdir)/python/gppylib/data/$(CATALOG_JSON)' ;

distclean: clean
	rm -f *.json

clean:
	rm -f $(CATALOG_JSON)

