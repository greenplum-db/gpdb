top_builddir = ..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/gpMgmt/Makefile.behave

SUBDIRS= sbin bin doc

$(recurse)

generate_greenplum_path_file:
	mkdir -p $(DESTDIR)$(prefix)
	unset LIBPATH; \
	bin/generate-greenplum-path.sh $(prefix) > $(DESTDIR)$(prefix)/greenplum_path.sh

install: generate_greenplum_path_file
	mkdir -p $(DESTDIR)$(prefix)/lib/python

	# Setup /lib/python contents
	if [ -e bin/ext/__init__.py ]; then \
	    cp -rp bin/ext/__init__.py $(DESTDIR)$(prefix)/lib/python ; \
	fi
	if [ -e bin/ext/psutil ]; then \
	    cp -rp bin/ext/psutil $(DESTDIR)$(prefix)/lib/python ; \
	fi
	if [ -e bin/ext/pgdb.py ]; then \
	    cp -rp bin/ext/pgdb.py $(DESTDIR)$(prefix)/lib/python && \
	    cp -rp bin/ext/pg.py $(DESTDIR)$(prefix)/lib/python && \
	    cp -rp bin/ext/_pg.so $(DESTDIR)$(prefix)/lib/python ; \
	fi
	if [ -e bin/ext/yaml ]; then \
	    cp -rp bin/ext/yaml $(DESTDIR)$(prefix)/lib/python ; \
	fi
	if [ -e bin/ext/subprocess32.py ]; then \
	    cp -p bin/ext/subprocess32.py $(DESTDIR)$(prefix)/lib/python ; \
	    cp -p bin/ext/subprocess32-ChangeLog $(DESTDIR)$(prefix)/lib/python ; \
	    cp -p bin/ext/_posixsubprocess.so $(DESTDIR)$(prefix)/lib/python ; \
	fi

clean distclean:
	$(MAKE) -C bin $@
