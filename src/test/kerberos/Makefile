#
# Makefile for Kerberos authentication test.
#
# Most of the logic is in installcheck.sh.
#

subdir = src/test/kerberos
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

installcheck:
	PATH=/usr/sbin:/usr/local/sbin:$(PATH) bash installcheck.sh

# Stop the test KDC server, if it's running.
.PHONY: stop-kdc
stop-kdc:
	if [ -e ./krb5kdc.pid ]; then kill `cat ./krb5kdc.pid`; fi || true
	rm -f ./krb5kdc.pid

clean: stop-kdc
	rm -rf test-kdc-db
	rm -f krb5cc server.keytab client.keytab
	rm -f pg_hba.conf.orig pg_hba.conf.kerberized postgresql.conf.orig postgresql.conf.kerberized
