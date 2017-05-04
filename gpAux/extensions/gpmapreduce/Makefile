
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

BLD_OS := $(shell uname -s)
BLD_OS_VER := $(shell uname -r)
ifneq "$(findstring $(BLD_OS)-$(BLD_OS_VER),SunOS-5.8 SunOS-5.9)" ""
BLD_CPPFLAGS_SOLARIS_OLD_VERSIONS=-D_REENTRANT
endif

ifdef srcdir
INCDIR= $(srcdir)/include
SRCDIR= $(srcdir)/src
BUILDDIR= ./src
else
INCDIR= include
SRCDIR= src
BUILDDIR= src
endif

override CPPFLAGS += $(BLD_CPPFLAGS_SOLARIS_OLD_VERSIONS) -DFRONTEND -I$(SRCDIR) -I$(INCDIR) -I$(libpq_srcdir)
override LDFLAGS  += -lyaml

FILES     = main.o mapred.o parse.o yaml_scan.o yaml_parse.o 
GENFILES  = yaml_parse.h yaml_parse.c yaml_scan.c yaml_scan.h

OBJS      = $(addprefix $(BUILDDIR)/, $(FILES))
GENOBJS   = $(addprefix $(SRCDIR)/, $(GENFILES))

FLEXFLAGS = -i
BISONFLAGS = -d

all: submake-libpq gpmapreduce

gpmapreduce: $(OBJS) $(libpq_builddir)/libpq.a
	$(CC) $(CFLAGS) $(OBJS) $(libpq_pgport) $(LDFLAGS) $(LIBS) -o $@$(X)

internal:
	rm -f $(SRCDIR)/mapred.o $(SRCDIR)/main.o
	$(MAKE) CFLAGS="-DINTERNAL_BUILD $(CFLAGS)" all

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
	$(BISON) $(BISONFLAGS) yaml_parse.y
else
	@$(missing) bison $< $@
endif

install: all installdirs
	$(INSTALL_PROGRAM) gpmapreduce$(X) '$(DESTDIR)$(bindir)/gpmapreduce$(X)'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/gpmapreduce$(X)'

clean distclean maintainer-clean:
	rm -f gpmapreduce$(X) $(OBJS) $(GENOBJS)

# Other dependencies
$(SRCDIR)/yaml_parse.c  : $(INCDIR)/parser.h $(INCDIR)/mapred.h
$(SRCDIR)/yaml_scan.c   : $(INCDIR)/parser.h $(INCDIR)/mapred.h
$(BUILDDIR)/parse.o       : $(SRCDIR)/yaml_parse.h
$(BUILDDIR)/parse.o       : $(INCDIR)/parser.h $(INCDIR)/mapred.h
$(BUILDDIR)/mapred.o      : $(INCDIR)/parser.h $(INCDIR)/mapred.h
$(BUILDDIR)/main.o        : $(INCDIR)/mapred.h

