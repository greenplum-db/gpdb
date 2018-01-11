# Include
include ./include/makefile.inc

# Options
DEBUG_S3_SYMBOL = y

# Flags
SHLIB_LINK += $(COMMON_LINK_OPTIONS)
PG_CPPFLAGS += $(COMMON_CPP_FLAGS) -Iinclude -Ilib -I$(libpq_srcdir) -I$(libpq_srcdir)/postgresql/server/utils

ifeq ($(DEBUG_S3_SYMBOL),y)
	PG_CPPFLAGS += -g
endif

# Targets
MODULE_big = gpcloud
OBJS = src/gpcloud.o lib/http_parser.o lib/ini.o $(addprefix src/,$(COMMON_OBJS))

# Launch
ifdef USE_PGXS
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
else
top_builddir = ../../..
-include $(top_builddir)/src/Makefile.global
-include $(top_srcdir)/contrib/contrib-global.mk
endif

gpcheckcloud:
	@$(MAKE) -C bin/gpcheckcloud

install: install-symlink

install-symlink:
	ln -sf gpcloud.so $(pkglibdir)/gps3ext.so

test: format
	@$(MAKE) -C test test

coverage: format
	@$(MAKE) -C test coverage

tags:
	-ctags -R --c++-kinds=+p --fields=+ialS --extra=+q
	-cscope -Rbq
	-gtags -i

lint:
	cppcheck -v --enable=warning src/*.cpp bin/gpcheckcloud/*.cpp test/*.cpp include/*.h

format:
	@-[ -n "`command -v dos2unix`" ] && dos2unix -k -q src/*.cpp bin/gpcheckcloud/*.cpp test/*.cpp include/*.h
	@-[ -n "`command -v clang-format`" ] && clang-format -style="{BasedOnStyle: Google, IndentWidth: 4, ColumnLimit: 100, AllowShortFunctionsOnASingleLine: None}" -i src/*.cpp bin/gpcheckcloud/*.cpp test/*.cpp include/*.h

cleanall:
	@-$(MAKE) clean # incase PGXS not included
	@-$(MAKE) -C bin/gpcheckcloud clean
	@$(MAKE) -C test clean
	rm -f *.o *.so *.a
	rm -f *.gcov src/*.gcov src/*.gcda src/*.gcno
	rm -f src/*.o src/*.d bin/gpcheckcloud/*.o bin/gpcheckcloud/*.d test/*.o test/*.d test/*.a lib/*.o lib/*.d

.PHONY: format lint tags test coverage cleanall
