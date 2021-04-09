top_builddir = ..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/gpMgmt/Makefile.behave

SUBDIRS= sbin bin doc

$(recurse)

generate_greenplum_path_file:
	mkdir -p $(DESTDIR)$(prefix)
	unset LIBPATH; \
	bin/generate-greenplum-path.sh > $(DESTDIR)$(prefix)/greenplum_path.sh

install: generate_greenplum_path_file
	mkdir -p $(DESTDIR)$(libdir)/python

	# Setup $(libdir)/python contents
	if [ -e bin/ext/__init__.py ]; then \
	    cp -rp bin/ext/__init__.py $(DESTDIR)$(libdir)/python ; \
	fi
	if [ -e bin/ext/psutil ]; then \
	    cp -rp bin/ext/psutil $(DESTDIR)$(libdir)/python ; \
	fi
	if [ -e bin/ext/pgdb.py ]; then \
	    cp -rp bin/ext/pgdb.py $(DESTDIR)$(libdir)/python && \
	    cp -rp bin/ext/pg.py $(DESTDIR)$(libdir)/python && \
	    cp -rp bin/ext/_pg*.so $(DESTDIR)$(libdir)/python ; \
	fi
	if [ -e bin/ext/yaml ]; then \
	    cp -rp bin/ext/yaml $(DESTDIR)$(libdir)/python ; \
	fi

clean distclean:
	$(MAKE) -C bin $@
