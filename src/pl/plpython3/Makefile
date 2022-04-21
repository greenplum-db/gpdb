# src/pl/plpython3/Makefile

subdir = src/pl/plpython3
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global


# We need libpython as a shared library.  In Python >=2.5, configure
# asks Python directly.  But because this has been broken in Debian
# for a long time (http://bugs.debian.org/695979), and to support
# older Python versions, we see if there is a file that is named like
# a shared library as a fallback.
ifeq (1,$(python3_enable_shared))
shared_libpython3 = yes
else
ifeq ($(PORTNAME), darwin)
# OS X does supply a .dylib even though Py_ENABLE_SHARED does not get set
shared_libpython3 = yes
else
ifneq (,$(wildcard $(python3_libdir)/libpython*$(DLSUFFIX)*))
shared_libpython3 = yes
endif
endif
endif

# Windows needs to convert backslashed paths to normal slashes,
# and we have to remove -lpython from the link since we are building our own
ifeq ($(PORTNAME), win32)
shared_libpython3 = yes
python3_includespec := $(subst \,/,$(python3_includespec))
override python3_libspec =
endif

override python3_libspec := -L${python3_configdir}/../.. ${python3_libspec}

# If we don't have a shared library, we have to skip it.
ifeq ($(shared_libpython3),yes)

# do not fail build due to warnings in this code
#override CPPFLAGS := -Wno-error -I. -I$(srcdir) $(python_includespec) $(CPPFLAGS) -DPLPYTHON_SHOW_DEBUG_INFO
override CPPFLAGS := -Wno-error -I. -I$(srcdir) $(python3_includespec) $(CPPFLAGS)

rpathdir = $(python3_libdir):$(INSTLOC)/ext/python/lib

NAME = plpython$(python3_majorversion)

OBJS = \
	plpy_cursorobject.o \
	plpy_elog.o \
	plpy_exec.o \
	plpy_main.o \
	plpy_planobject.o \
	plpy_plpymodule.o \
	plpy_procedure.o \
	plpy_resultobject.o \
	plpy_spi.o \
	plpy_subxactobject.o \
	plpy_typeio.o \
	plpy_util.o

DATA = plpython3u.control plpython3u--1.0.sql plpython3u--unpackaged--1.0.sql


# Python on win32 ships with import libraries only for Microsoft Visual C++,
# which are not compatible with mingw gcc. Therefore we need to build a
# new import library to link with.
ifeq ($(PORTNAME), win32)

pytverstr=$(subst .,,${python3_version})
PYTHONDLL=$(subst \,/,$(WINDIR))/system32/python${pytverstr}.dll

OBJS += libpython${pytverstr}.a

libpython${pytverstr}.a: python${pytverstr}.def
	dlltool --dllname python${pytverstr}.dll --def python${pytverstr}.def --output-lib libpython${pytverstr}.a

python${pytverstr}.def:
	pexports $(PYTHONDLL) > $@

endif # win32


SHLIB_LINK = $(python3_libspec) $(python3_additional_libs) $(filter -lintl,$(LIBS))

REGRESS_OPTS = --dbname=$(PL_TESTDB) --init-file=$(srcdir)/init_file
# Only load plpythonu with Python 2.  The test files themselves load
# the versioned language plpython(2|3)u.
# FIXME, check this
#REGRESS_OPTS += --load-extension=plpython3u

REGRESS = \
	plpython_schema \
	plpython_populate \
	plpython_test \
	plpython3_path_guc \
	plpython_do \
	plpython_global \
	plpython_import \
	plpython_spi \
	plpython_newline \
	plpython_void \
	plpython_params \
	plpython_setof \
	plpython_record \
	plpython_trigger \
	plpython_types \
	plpython_error \
	plpython_unicode \
	plpython_quote \
	plpython_composite \
	plpython_subtransaction \
	plpython_returns \
	plpython_gpdb \
	plpython_drop

REGRESS_PLPYTHON3_MANGLE := $(REGRESS)

# where to find psql for running the tests
PSQLDIR = $(bindir)

include $(top_srcdir)/src/Makefile.shlib

all: all-lib


install: all install-lib install-data

installdirs: installdirs-lib
	$(MKDIR_P) '$(DESTDIR)$(datadir)/extension'

uninstall: uninstall-lib uninstall-data

install-data: installdirs
	$(INSTALL_DATA) $(addprefix $(srcdir)/, $(DATA)) '$(DESTDIR)$(datadir)/extension/'

uninstall-data:
	rm -f $(addprefix '$(DESTDIR)$(datadir)/extension'/, $(notdir $(DATA)))

.PHONY: install-data uninstall-data


ifeq ($(python3_majorversion),3)
# Adjust regression tests for Python 3 compatibility
#
# Mention those regression test files that need to be mangled in the
# variable REGRESS_PLPYTHON3_MANGLE.  They will be copied to a
# subdirectory python3/ and have their Python syntax and other bits
# adjusted to work with Python 3.

# Note that the order of the tests needs to be preserved in this
# expression.
REGRESS := $(foreach test,$(REGRESS),$(if $(filter $(test),$(REGRESS_PLPYTHON3_MANGLE)),python3/$(test),$(test)))

.PHONY: pgregress-python3-mangle
pgregress-python3-mangle:
	$(MKDIR_P) sql/python3 expected/python3 results/python3
	# for python and python3 types and returns test are not the same.
	cp -f ../plpython/expected/plpython_returns_3.out ../plpython/expected/python3/plpython_returns.out
	for file in $(patsubst %,$(srcdir)/sql/%.sql,$(REGRESS_PLPYTHON3_MANGLE)) $(patsubst %,$(srcdir)/expected/%*.out,$(REGRESS_PLPYTHON3_MANGLE)); do \
	  sed -e 's/except \([[:alpha:]][[:alpha:].]*\), *\([[:alpha:]][[:alpha:]]*\):/except \1 as \2:/g' \
	      -e "s/<type 'exceptions\.\([[:alpha:]]*\)'>/<class '\1'>/g" \
	      -e "s/<type 'long'>/<class 'int'>/g" \
	      -e "s/<type 'list'>/<class 'list'>/g" \
	      -e "s/<type 'NoneType'>/<class 'NoneType'>/g" \
	      -e "s/\([0-9][0-9]*\)L/\1/g" \
	      -e 's/\([ [{]\)u"/\1"/g' \
	      -e "s/\([ [{]\)u'/\1'/g" \
	      -e "s/def next/def __next__/g" \
	      -e "s/LANGUAGE plpythonu/LANGUAGE plpython3u/Ig" \
	      -e "s/LANGUAGE plpython2u/LANGUAGE plpython3u/Ig" \
	      -e "s/EXTENSION plpythonu/EXTENSION plpython3u/Ig" \
	      -e "s/EXTENSION plpython2u/EXTENSION plpython3u/Ig" \
	    $$file >`echo $$file | sed 's,^.*/\([^/][^/]*/\)\([^/][^/]*\)$$,\1python3/\2,'` || exit; \
	done

check installcheck: pgregress-python3-mangle

pg_regress_clean_files += sql/python3/ expected/python3/ results/python3/

endif # Python 3


check: all submake
	$(pg_regress_check) $(REGRESS_OPTS) $(REGRESS)

installcheck: submake
	$(pg_regress_installcheck) $(REGRESS_OPTS) $(REGRESS)


.PHONY: submake
submake:
	$(MAKE) -C $(top_builddir)/src/test/regress pg_regress$(X)

clean distclean: clean-lib
	rm -f $(OBJS)
	rm -rf $(pg_regress_clean_files)
ifeq ($(PORTNAME), win32)
	rm -f python${pytverstr}.def
endif

else # can't build

all:
	@echo ""; \
	 echo "*** Cannot build PL/Python because libpython is not a shared library." ; \
	 echo "*** You might have to rebuild your Python installation.  Refer to"; \
	 echo "*** the documentation for details."; \
	 echo ""

endif # can't build

# distprep and maintainer-clean rules should be run even if we can't build.

# Force this dependency to be known even without dependency info built:
plpy_plpymodule.o: spiexceptions.h

spiexceptions.h: $(top_srcdir)/src/backend/utils/errcodes.txt generate-spiexceptions.pl
	$(PERL) $(srcdir)/generate-spiexceptions.pl $< > $@

distprep: spiexceptions.h

maintainer-clean: distclean
	rm -f spiexceptions.h
