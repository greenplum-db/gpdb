#-------------------------------------------------------------------------
#
# Makefile for src/bin/gpupgrade
#
# Portions Copyright (c) 2007, Greenplum
#
# $PostgreSQL$
#
#-------------------------------------------------------------------------

PGFILEDESC = "gpupgrade - Greenplum upgrader"
subdir = src/bin/gpupgrade
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

# The frontend doesn't need everything that's in LIBS, some are backend only
LIBS := $(filter-out -lresolv -lbz2, $(LIBS))
LIBS := $(filter-out -lreadline -ledit -ltermcap -lncurses -lcurses -lcurl -lssl -lz -lpam -lkrb5 -lgssapi_krb5 -lcom_err -lk5crypto -lnuma -lkrb5support, $(LIBS))

MODCAT_OBJS= gpmodcontrol.o pg_crc.o 
DATADIR=$(top_srcdir)/src/test/regress/data

UPGRADEFILES = $(DATADIR)/upgrade/upg2_catupgrade.sql \
			   $(DATADIR)/upgrade/upg2_catupgrade_check.sql \
			   $(DATADIR)/upgrade43/upg2_catupgrade_43.sql

FIXUPGRADE = fix_ao_upgrade.py
FIXVISIOWNER = fix_visimap_owner.sql

DATAFILES = $(DATADIR)/upgrade43/pg_operator.data $(DATADIR)/upgrade43/pg_type.data \
		$(DATADIR)/upgrade43/$(FIXUPGRADE) $(DATADIR)/upgrade43/$(FIXVISIOWNER).in

# This sets things up to create a "contrib" module for upgrade extensions
MODULE_big = gpupgrade
OBJS= gpviewcp.o truncate_acl.o
include $(top_srcdir)/contrib/contrib-global.mk

# In addition to the contrib module we are also building the gpmodcontrol program and the upgrade scripts
all: gpmodcontrol upgradesql

clean distclean maintainer-clean: clean-lib gpmodcontrol_clean upgradesql_clean

install: all installdirs installupgradefiles gpmodcontrol_install

uninstall: gpmodcontrol_uninstall

# build the gpmodcontrol program and the upgrade script
gpmodcontrol: $(MODCAT_OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@$(X)

pg_crc.c: $(top_srcdir)/src/backend/utils/hash/pg_crc.c
	rm -f $@ && $(LN_S) $< .

upgradesql:
	@for i in $(UPGRADEFILES); \
	do \
        sed -e 's/\@gpupgradeschemaname\@/pg_catalog/g' -e 's/\@gpupgradetoolkitschema\@/gp_toolkit/g' $$i.in > $$i; \
	done

gpmodcontrol_clean:
	rm -f gpmodcontrol$(X)
	rm -f $(MODCAT_OBJS)
	rm -f pg_crc.c

upgradesql_clean:
	rm -f $(DATADIR)/upgrade43/*.sql

gpmodcontrol_install: gpmodcontrol
	$(INSTALL_PROGRAM) gpmodcontrol$(X) \
		'$(DESTDIR)$(bindir)/lib'/gpmodcontrol$(X)

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(bindir)' '$(DESTDIR)$(bindir)/lib'
	$(MKDIR_P) '$(DESTDIR)$(datadir)' '$(DESTDIR)$(datadir)/upgrade'

installupgradefiles: $(UPGRADEFILES) $(DATAFILES)
	@for i in $^; \
	do \
		$(INSTALL_DATA) $$i '$(DESTDIR)$(datadir)/upgrade/'`basename $$i`; \
	done
	chmod +x $(DESTDIR)$(datadir)/upgrade/$(FIXUPGRADE)
	mv $(DESTDIR)$(datadir)/upgrade/$(FIXVISIOWNER).in \
		$(DESTDIR)$(datadir)/upgrade/$(FIXVISIOWNER)

gpmodcontrol_unistall:
	rm -f '$(DESTDIR)$(bindir)'/lib, gpmodcontrol$(X)

