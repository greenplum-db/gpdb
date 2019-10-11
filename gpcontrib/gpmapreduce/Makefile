
top_builddir = ../..
include $(top_builddir)/src/Makefile.global

ifdef srcdir
INCDIR= $(srcdir)/include
SRCDIR= $(srcdir)/src
BUILDDIR= ./src
else
INCDIR= include
SRCDIR= src
BUILDDIR= src
endif

# Build dynamically-loaded object file for CREATE FUNCTION ... LANGUAGE C
# functions in regression tests

NAME = regress
OBJS = mapred_test.o

include $(top_srcdir)/src/Makefile.shlib

override CPPFLAGS += -I$(SRCDIR) -I$(INCDIR) -I$(libpq_srcdir)

# Disable -Wimplicit-fallthrough for this module, since it is hard to change some
# macros using switch-case.
# -Wimplicit-fallthrough is appended to CFLAGS, so we append it to CFLAGS as well.
ifeq ($(shell echo "$(CFLAGS)" | grep -q "implicit-fallthrough" && echo yes), yes)
override CFLAGS += -Wno-implicit-fallthrough
endif

FILES     = main.o mapred.o parse.o yaml_scan.o yaml_parse.o 
GENFILES  = yaml_parse.h yaml_parse.c yaml_scan.c yaml_scan.h

MAPREDOBJS= $(addprefix $(BUILDDIR)/, $(FILES))
GENOBJS   = $(addprefix $(SRCDIR)/, $(GENFILES))

FLEXFLAGS = -i

$(MAPREDOBJS): override CPPFLAGS += -DFRONTEND

all: submake-libpq gpmapreduce all-lib

gpmapreduce: $(MAPREDOBJS) $(libpq_builddir)/libpq.a
	$(CC) $(CFLAGS) $(MAPREDOBJS) $(libpq_pgport) $(LDFLAGS) $(LIBS) -o $@$(X)

$(SRCDIR)/yaml_scan.c: $(SRCDIR)/yaml_parse.h $(SRCDIR)/yaml_scan.l
ifdef FLEX
	cd $(SRCDIR) ; \
	$(FLEX) $(FLEXFLAGS) yaml_scan.l
else
	@$(missing) flex $< $@
endif

$(SRCDIR)/yaml_parse.h: $(SRCDIR)/yaml_parse.c
$(SRCDIR)/yaml_parse.c: $(SRCDIR)/yaml_parse.y
ifdef BISON
	@cd $(SRCDIR) ; \
	$(BISON) -d $(BISONFLAGS) yaml_parse.y
else
	@$(missing) bison $< $@
endif

abs_srcdir := $(shell cd "$(srcdir)" && pwd)

installcheck: all
	$(top_builddir)/src/test/regress/pg_regress --init-file=init_file --dbname=mapred_regression --inputdir=$(srcdir) --top-builddir=$(top_builddir) mapred

install: all installdirs
	$(INSTALL_PROGRAM) gpmapreduce$(X) '$(DESTDIR)$(bindir)/gpmapreduce$(X)'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/gpmapreduce$(X)'

clean distclean maintainer-clean: clean-lib
	rm -f gpmapreduce$(X) $(MAPREDOBJS) $(GENOBJS) $(OBJS)

# Other dependencies
$(SRCDIR)/yaml_parse.c  : $(INCDIR)/parser.h $(INCDIR)/mapred.h
$(SRCDIR)/yaml_scan.c   : $(INCDIR)/parser.h $(INCDIR)/mapred.h
$(BUILDDIR)/parse.o       : $(SRCDIR)/yaml_parse.h
$(BUILDDIR)/parse.o       : $(INCDIR)/parser.h $(INCDIR)/mapred.h
$(BUILDDIR)/mapred.o      : $(INCDIR)/parser.h $(INCDIR)/mapred.h
$(BUILDDIR)/main.o        : $(INCDIR)/mapred.h

