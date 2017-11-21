MODULE_big = gphdfs
OBJS       = gphdfsformatter.o gphdfsprotocol.o

REGRESS = basic privileges

PG_CPPFLAGS = -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport)

ANT = ant
ANT_OPTS="-Djavax.net.ssl.trustStore=../../releng/make/dependencies/cacerts"

PG_CONFIG := pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

JAR_FILES = dist/hdp2.0-gnet-1.2.0.0.jar

JAVADOC_TARS = gnet-1.2-javadoc.tar

all: $(JAR_FILES) unittest $(JAVADOC_TARS)

dist/hdp2.0-gnet-1.2.0.0.jar:
	$(ANT) clean
	ANT_OPTS=$(ANT_OPTS) $(ANT) dist -Dgphdgnet.version=hdp2.0-gnet-1.2.0.0 \
                -Dgpgnet.src=1.2 -Dgpgnet.configuration=hadoop2
	cp dist/hdp2.0-gnet-1.2.0.0.jar dist/cdh4.1-gnet-1.2.0.0.jar
	cp dist/hdp2.0-gnet-1.2.0.0.jar dist/gpmr-1.2-gnet-1.2.0.0.jar

unittest: $(JAR_FILES)
	$(ANT) clean
	ANT_OPTS=$(ANT_OPTS) $(ANT) test -Dgphdgnet.version=hdp2.0-gnet-1.2.0.0 \
                -Dgpgnet.src=1.2 -Dgpgnet.configuration=ut

gnet-1.2-javadoc.tar: $(JAR_FILES)
	$(ANT) javadoc -Dgphdgnet.version=gnet-1.2 -Dgpgnet.src=1.2 -Dgpgnet.configuration=hadoop2
	$(TAR) -cf gnet-1.2-javadoc.tar gnet-1.2-javadoc

install-jars: $(JAR_FILES)
	$(INSTALL_PROGRAM) dist/*.jar '$(DESTDIR)$(libdir)/hadoop/'
	$(INSTALL_PROGRAM) hadoop_env.sh$(X) '$(DESTDIR)$(libdir)/hadoop/hadoop_env.sh$(X)'
	$(INSTALL_PROGRAM) jaas.conf$(X) '$(DESTDIR)$(libdir)/hadoop/jaas.conf$(X)'

.PHONY: install-javadocs
install-javadocs: $(JAVADOC_TARS)
	$(INSTALL_DATA) *-javadoc.tar '$(DESTDIR)$(docdir)/javadoc'

.PHONY: install-gphdfs.so
install-gphdfs.so:
	$(INSTALL_PROGRAM) gphdfs.so $(DESTDIR)$(pkglibdir)

.PHONY: install-data
install-data:
	$(INSTALL_DATA) gphdfs.sql '$(DESTDIR)$(datadir)/cdb_init.d/gphdfs.sql'

install: install-jars install-javadocs install-gphdfs.so install-data

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
