MODULE_big = gphdfs
OBJS = gphdfsformatter.o gphdfsprotocol.o
REGRESS = basic privileges

PG_CPPFLAGS = -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport)

ANT = ant
TAR = tar
ANT_OPTS="-Djavax.net.ssl.trustStore=../../releng/make/dependencies/cacerts"

ifdef USE_PGXS
	PGXS := $(shell pg_config --pgxs)
include $(PGXS)
else
	top_builddir = ../../..
	include $(top_builddir)/src/Makefile.global
	include $(top_srcdir)/contrib/contrib-global.mk
endif

JAR_FILES = dist/hadoop2-gnet-1.2.0.0.jar
JAVADOC_TARS = gnet-1.2-javadoc.tar

all: $(JAR_FILES) unittest $(JAVADOC_TARS)

dist/hadoop2-gnet-1.2.0.0.jar:
	$(ANT) clean
	ANT_OPTS=$(ANT_OPTS) $(ANT) dist -Dgphdgnet.version=hadoop2-gnet-1.2.0.0 \
                -Dgpgnet.src=1.2 -Dgpgnet.configuration=hadoop2
	cp dist/hadoop2-gnet-1.2.0.0.jar dist/hdp2.0-gnet-1.2.0.0.jar
	cp dist/hadoop2-gnet-1.2.0.0.jar dist/cdh5.0-gnet-1.2.0.0.jar
	cp dist/hadoop2-gnet-1.2.0.0.jar dist/mpr4.0-gnet-1.2.0.0.jar

unittest: $(JAR_FILES)
	$(ANT) clean
	ANT_OPTS=$(ANT_OPTS) $(ANT) test -Dgphdgnet.version=hadoop2-gnet-1.2.0.0 \
                -Dgpgnet.src=1.2 -Dgpgnet.configuration=ut

gnet-1.2-javadoc.tar: $(JAR_FILES)
	$(ANT) javadoc -Dgphdgnet.version=gnet-1.2 -Dgpgnet.src=1.2 -Dgpgnet.configuration=hadoop2
	$(TAR) -cf gnet-1.2-javadoc.tar gnet-1.2-javadoc

install-jars: $(JAR_FILES) $(JAVADOC_TARS)
	$(INSTALL_PROGRAM) dist/*.jar '$(DESTDIR)$(libdir)/hadoop/'
	$(INSTALL_PROGRAM) hadoop_env.sh$(X) '$(DESTDIR)$(libdir)/hadoop/hadoop_env.sh$(X)'
	$(INSTALL_PROGRAM) jaas.conf$(X) '$(DESTDIR)$(libdir)/hadoop/jaas.conf$(X)'
	$(INSTALL_PROGRAM) *-javadoc.tar '$(DESTDIR)$(docdir)/javadoc'

.PHONY: install-gphdfs
install-gphdfs: gphdfs.so
	$(INSTALL_PROGRAM) gphdfs.so $(DESTDIR)$(pkglibdir)
	$(INSTALL_PROGRAM) gphdfs.sql '$(DESTDIR)$(datadir)/cdb_init.d/gphdfs.sql'

install: install-jars install-gphdfs

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(libdir)/hadoop'
	$(MKDIR_P) '$(DESTDIR)$(docdir)/javadoc'

uninstall:
	rm -f '$(DESTDIR)$(bindir)/*.jar'
	rm -f '$(DESTDIR)$(bindir)/hadoop_env.sh$(X)'
	rm -f '$(DESTDIR)$(bindir)/jaas.conf$(X)'
	rm -f '$(DESTDIR)$(datadir)/cdb_init.d/gphdfs.sql'

clean: clean-extras

clean-extras:
	$(ANT) clean
	rm -rf *-javadoc
	rm -f *-javadoc.tar
	rm -rf result
