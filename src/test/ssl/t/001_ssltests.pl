use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

use File::Copy;

use FindBin;
use lib $FindBin::RealBin;

use SSLServer;

if ($ENV{with_openssl} ne 'yes')
{
	plan skip_all => 'SSL not supported by this build';
}

#### Some configuration

# This is the hostname used to connect to the server. This cannot be a
# hostname, because the server certificate is always for the domain
# postgresql-ssl-regression.test.
my $SERVERHOSTADDR = '127.0.0.1';

# Allocation of base connection string shared among multiple tests.
my $common_connstr;

# The client's private key must not be world-readable, so take a copy
# of the key stored in the code tree and update its permissions.
copy("ssl/client.key", "ssl/client_tmp.key");
chmod 0600, "ssl/client_tmp.key";
copy("ssl/client-revoked.key", "ssl/client-revoked_tmp.key");
chmod 0600, "ssl/client-revoked_tmp.key";

# Also make a copy of that explicitly world-readable.  We can't
# necessarily rely on the file in the source tree having those
# permissions.
copy("ssl/client.key", "ssl/client_wrongperms_tmp.key");
chmod 0644, "ssl/client_wrongperms_tmp.key";

#### Set up the server.

note "setting up data directory";
my $node = PostgreSQL::Test::Cluster->new('primary');
$node->init;

# PGHOST is enforced here to set up the node, subsequent connections
# will use a dedicated connection string.
$ENV{PGHOST} = $node->host;
$ENV{PGPORT} = $node->port;
$node->start;

# Run this before we lock down access below.
my $result = $node->safe_psql('postgres', "SHOW ssl_library");
is($result, 'OpenSSL', 'ssl_library parameter');

configure_test_server_for_ssl($node, $SERVERHOSTADDR, 'trust');

note "testing password-protected keys";

open my $sslconf, '>', $node->data_dir . "/sslconfig.conf";
print $sslconf "ssl=on\n";
print $sslconf "ssl_cert_file='server-cn-only.crt'\n";
print $sslconf "ssl_key_file='server-password.key'\n";
print $sslconf "ssl_passphrase_command='echo wrongpassword'\n";
close $sslconf;

command_fails(
	[ 'pg_ctl', '-D', $node->data_dir, '-l', $node->logfile, 'restart' ],
	'restart fails with password-protected key file with wrong password');
$node->_update_pid(0);

open $sslconf, '>', $node->data_dir . "/sslconfig.conf";
print $sslconf "ssl=on\n";
print $sslconf "ssl_cert_file='server-cn-only.crt'\n";
print $sslconf "ssl_key_file='server-password.key'\n";
print $sslconf "ssl_passphrase_command='echo secret1'\n";
close $sslconf;

command_ok(
	[ 'pg_ctl', '-D', $node->data_dir, '-l', $node->logfile, 'restart' ],
	'restart succeeds with password-protected key file');
$node->_update_pid(1);

### Run client-side tests.
###
### Test that libpq accepts/rejects the connection correctly, depending
### on sslmode and whether the server's certificate looks correct. No
### client certificate is used in these tests.

note "running client tests";

switch_server_cert($node, 'server-cn-only');

# Set of default settings for SSL parameters in connection string.  This
# makes the tests protected against any defaults the environment may have
# in ~/.postgresql/.
my $default_ssl_connstr = "sslkey=invalid sslcert=invalid sslrootcert=invalid sslcrl=invalid";

$common_connstr =
  "$default_ssl_connstr user=ssltestuser dbname=trustdb hostaddr=$SERVERHOSTADDR host=common-name.pg-ssltest.test";

# The server should not accept non-SSL connections.
$node->connect_fails(
	"$common_connstr sslmode=disable",
	"server doesn't accept non-SSL connections",
	expected_stderr => qr/\Qno pg_hba.conf entry\E/);

# Try without a root cert. In sslmode=require, this should work. In verify-ca
# or verify-full mode it should fail.
$node->connect_ok(
	"$common_connstr sslrootcert=invalid sslmode=require",
	"connect without server root cert sslmode=require");
$node->connect_fails(
	"$common_connstr sslrootcert=invalid sslmode=verify-ca",
	"connect without server root cert sslmode=verify-ca",
	expected_stderr => qr/root certificate file "invalid" does not exist/);
$node->connect_fails(
	"$common_connstr sslrootcert=invalid sslmode=verify-full",
	"connect without server root cert sslmode=verify-full",
	expected_stderr => qr/root certificate file "invalid" does not exist/);

# Try with wrong root cert, should fail. (We're using the client CA as the
# root, but the server's key is signed by the server CA.)
$node->connect_fails(
	"$common_connstr sslrootcert=ssl/client_ca.crt sslmode=require",
	"connect with wrong server root cert sslmode=require",
	expected_stderr => qr/SSL error: certificate verify failed/);
$node->connect_fails(
	"$common_connstr sslrootcert=ssl/client_ca.crt sslmode=verify-ca",
	"connect with wrong server root cert sslmode=verify-ca",
	expected_stderr => qr/SSL error: certificate verify failed/);
$node->connect_fails(
	"$common_connstr sslrootcert=ssl/client_ca.crt sslmode=verify-full",
	"connect with wrong server root cert sslmode=verify-full",
	expected_stderr => qr/SSL error: certificate verify failed/);

# Try with just the server CA's cert. This fails because the root file
# must contain the whole chain up to the root CA.
$node->connect_fails(
	"$common_connstr sslrootcert=ssl/server_ca.crt sslmode=verify-ca",
	"connect with server CA cert, without root CA",
	expected_stderr => qr/SSL error: certificate verify failed/);

# And finally, with the correct root cert.
$node->connect_ok(
	"$common_connstr sslrootcert=ssl/root+server_ca.crt sslmode=require",
	"connect with correct server CA cert file sslmode=require");
$node->connect_ok(
	"$common_connstr sslrootcert=ssl/root+server_ca.crt sslmode=verify-ca",
	"connect with correct server CA cert file sslmode=verify-ca");
$node->connect_ok(
	"$common_connstr sslrootcert=ssl/root+server_ca.crt sslmode=verify-full",
	"connect with correct server CA cert file sslmode=verify-full");

# Test with cert root file that contains two certificates. The client should
# be able to pick the right one, regardless of the order in the file.
$node->connect_ok(
	"$common_connstr sslrootcert=ssl/both-cas-1.crt sslmode=verify-ca",
	"cert root file that contains two certificates, order 1");
$node->connect_ok(
	"$common_connstr sslrootcert=ssl/both-cas-2.crt sslmode=verify-ca",
	"cert root file that contains two certificates, order 2");

# CRL tests

# Invalid CRL filename is the same as no CRL, succeeds
$node->connect_ok(
	"$common_connstr sslrootcert=ssl/root+server_ca.crt sslmode=verify-ca sslcrl=invalid",
	"sslcrl option with invalid file name");

# A CRL belonging to a different CA is not accepted, fails
$node->connect_fails(
	"$common_connstr sslrootcert=ssl/root+server_ca.crt sslmode=verify-ca sslcrl=ssl/client.crl",
	"CRL belonging to a different CA",
	expected_stderr => qr/SSL error: certificate verify failed/);

# With the correct CRL, succeeds (this cert is not revoked)
$node->connect_ok(
	"$common_connstr sslrootcert=ssl/root+server_ca.crt sslmode=verify-ca sslcrl=ssl/root+server.crl",
	"CRL with a non-revoked cert");

# Check that connecting with verify-full fails, when the hostname doesn't
# match the hostname in the server's certificate.
$common_connstr =
  "$default_ssl_connstr user=ssltestuser dbname=trustdb sslrootcert=ssl/root+server_ca.crt hostaddr=$SERVERHOSTADDR";

$node->connect_ok("$common_connstr sslmode=require host=wronghost.test",
	"mismatch between host name and server certificate sslmode=require");
$node->connect_ok(
	"$common_connstr sslmode=verify-ca host=wronghost.test",
	"mismatch between host name and server certificate sslmode=verify-ca");
$node->connect_fails(
	"$common_connstr sslmode=verify-full host=wronghost.test",
	"mismatch between host name and server certificate sslmode=verify-full",
	expected_stderr =>
	  qr/\Qserver certificate for "common-name.pg-ssltest.test" does not match host name "wronghost.test"\E/
);

# Test Subject Alternative Names.
switch_server_cert($node, 'server-multiple-alt-names');

$common_connstr =
  "$default_ssl_connstr user=ssltestuser dbname=trustdb sslrootcert=ssl/root+server_ca.crt hostaddr=$SERVERHOSTADDR sslmode=verify-full";

$node->connect_ok(
	"$common_connstr host=dns1.alt-name.pg-ssltest.test",
	"host name matching with X.509 Subject Alternative Names 1");
$node->connect_ok(
	"$common_connstr host=dns2.alt-name.pg-ssltest.test",
	"host name matching with X.509 Subject Alternative Names 2");
$node->connect_ok("$common_connstr host=foo.wildcard.pg-ssltest.test",
	"host name matching with X.509 Subject Alternative Names wildcard");

$node->connect_fails(
	"$common_connstr host=wronghost.alt-name.pg-ssltest.test",
	"host name not matching with X.509 Subject Alternative Names",
	expected_stderr =>
	  qr/\Qserver certificate for "dns1.alt-name.pg-ssltest.test" (and 2 other names) does not match host name "wronghost.alt-name.pg-ssltest.test"\E/
);
$node->connect_fails(
	"$common_connstr host=deep.subdomain.wildcard.pg-ssltest.test",
	"host name not matching with X.509 Subject Alternative Names wildcard",
	expected_stderr =>
	  qr/\Qserver certificate for "dns1.alt-name.pg-ssltest.test" (and 2 other names) does not match host name "deep.subdomain.wildcard.pg-ssltest.test"\E/
);

# Test certificate with a single Subject Alternative Name. (this gives a
# slightly different error message, that's all)
switch_server_cert($node, 'server-single-alt-name');

$common_connstr =
  "$default_ssl_connstr user=ssltestuser dbname=trustdb sslrootcert=ssl/root+server_ca.crt hostaddr=$SERVERHOSTADDR sslmode=verify-full";

$node->connect_ok(
	"$common_connstr host=single.alt-name.pg-ssltest.test",
	"host name matching with a single X.509 Subject Alternative Name");

$node->connect_fails(
	"$common_connstr host=wronghost.alt-name.pg-ssltest.test",
	"host name not matching with a single X.509 Subject Alternative Name",
	expected_stderr =>
	  qr/\Qserver certificate for "single.alt-name.pg-ssltest.test" does not match host name "wronghost.alt-name.pg-ssltest.test"\E/
);
$node->connect_fails(
	"$common_connstr host=deep.subdomain.wildcard.pg-ssltest.test",
	"host name not matching with a single X.509 Subject Alternative Name wildcard",
	expected_stderr =>
	  qr/\Qserver certificate for "single.alt-name.pg-ssltest.test" does not match host name "deep.subdomain.wildcard.pg-ssltest.test"\E/
);

# Test server certificate with a CN and SANs. Per RFCs 2818 and 6125, the CN
# should be ignored when the certificate has both.
switch_server_cert($node, 'server-cn-and-alt-names');

$common_connstr =
  "$default_ssl_connstr user=ssltestuser dbname=trustdb sslrootcert=ssl/root+server_ca.crt hostaddr=$SERVERHOSTADDR sslmode=verify-full";

$node->connect_ok("$common_connstr host=dns1.alt-name.pg-ssltest.test",
	"certificate with both a CN and SANs 1");
$node->connect_ok("$common_connstr host=dns2.alt-name.pg-ssltest.test",
	"certificate with both a CN and SANs 2");
$node->connect_fails(
	"$common_connstr host=common-name.pg-ssltest.test",
	"certificate with both a CN and SANs ignores CN",
	expected_stderr =>
	  qr/\Qserver certificate for "dns1.alt-name.pg-ssltest.test" (and 1 other name) does not match host name "common-name.pg-ssltest.test"\E/
);

# Finally, test a server certificate that has no CN or SANs. Of course, that's
# not a very sensible certificate, but libpq should handle it gracefully.
switch_server_cert($node, 'server-no-names');
$common_connstr =
  "$default_ssl_connstr user=ssltestuser dbname=trustdb sslrootcert=ssl/root+server_ca.crt hostaddr=$SERVERHOSTADDR";

$node->connect_ok(
	"$common_connstr sslmode=verify-ca host=common-name.pg-ssltest.test",
	"server certificate without CN or SANs sslmode=verify-ca");
$node->connect_fails(
	$common_connstr . " "
	  . "sslmode=verify-full host=common-name.pg-ssltest.test",
	"server certificate without CN or SANs sslmode=verify-full",
	expected_stderr =>
	  qr/could not get server's host name from server certificate/);

# Test that the CRL works
switch_server_cert($node, 'server-revoked');

$common_connstr =
  "$default_ssl_connstr user=ssltestuser dbname=trustdb hostaddr=$SERVERHOSTADDR host=common-name.pg-ssltest.test";

# Without the CRL, succeeds. With it, fails.
$node->connect_ok(
	"$common_connstr sslrootcert=ssl/root+server_ca.crt sslmode=verify-ca",
	"connects without client-side CRL");
$node->connect_fails(
	"$common_connstr sslrootcert=ssl/root+server_ca.crt sslmode=verify-ca sslcrl=ssl/root+server.crl",
	"does not connect with client-side CRL file",
	expected_stderr => qr/SSL error: certificate verify failed/);

# pg_stat_ssl
command_like(
	[
		'psql',                                '-X',
		'-A',                                  '-F',
		',',                                   '-P',
		'null=_null_',                         '-d',
		"$common_connstr sslrootcert=invalid", '-c',
		"SELECT * FROM pg_stat_ssl WHERE pid = pg_backend_pid()"
	],
	qr{^pid,ssl,version,cipher,bits,compression,client_dn,client_serial,issuer_dn\r?\n
				^\d+,t,TLSv[\d.]+,[\w-]+,\d+,f,_null_,_null_,_null_\r?$}mx,
	'pg_stat_ssl view without client certificate');

### Server-side tests.
###
### Test certificate authorization.

note "running server tests";

$common_connstr =
  "$default_ssl_connstr sslrootcert=ssl/root+server_ca.crt sslmode=require dbname=certdb hostaddr=$SERVERHOSTADDR";

# no client cert
$node->connect_fails(
	"$common_connstr user=ssltestuser sslcert=invalid",
	"certificate authorization fails without client cert",
	expected_stderr => qr/connection requires a valid client certificate/);

# correct client cert
$node->connect_ok(
	"$common_connstr user=ssltestuser sslcert=ssl/client.crt sslkey=ssl/client_tmp.key",
	"certificate authorization succeeds with correct client cert");

# pg_stat_ssl
command_like(
	[
		'psql',
		'-X',
		'-A',
		'-F',
		',',
		'-P',
		'null=_null_',
		'-d',
		"$common_connstr user=ssltestuser sslcert=ssl/client.crt sslkey=ssl/client_tmp.key",
		'-c',
		"SELECT * FROM pg_stat_ssl WHERE pid = pg_backend_pid()"
	],
	qr{^pid,ssl,version,cipher,bits,compression,client_dn,client_serial,issuer_dn\r?\n
				^\d+,t,TLSv[\d.]+,[\w-]+,\d+,f,/CN=ssltestuser,1,\Q/CN=Test CA for PostgreSQL SSL regression test client certs\E\r?$}mx,
	'pg_stat_ssl with client certificate');

# client key with wrong permissions
SKIP:
{
	skip "Permissions check not enforced on Windows", 2 if ($windows_os);

	$node->connect_fails(
		"$common_connstr user=ssltestuser sslcert=ssl/client.crt sslkey=ssl/client_wrongperms_tmp.key",
		"certificate authorization fails because of file permissions",
		expected_stderr =>
		  qr!\Qprivate key file "ssl/client_wrongperms_tmp.key" has group or world access\E!
	);
}

# client cert belonging to another user
$node->connect_fails(
	"$common_connstr user=anotheruser sslcert=ssl/client.crt sslkey=ssl/client_tmp.key",
	"certificate authorization fails with client cert belonging to another user",
	expected_stderr =>
	  qr/certificate authentication failed for user "anotheruser"/);

# revoked client cert
$node->connect_fails(
	"$common_connstr user=ssltestuser sslcert=ssl/client-revoked.crt sslkey=ssl/client-revoked_tmp.key",
	"certificate authorization fails with revoked client cert",
	expected_stderr => qr/SSL error: sslv3 alert certificate revoked/);

# Check that connecting with auth-option verify-full in pg_hba:
# works, iff username matches Common Name
# fails, iff username doesn't match Common Name.
$common_connstr =
  "$default_ssl_connstr sslrootcert=ssl/root+server_ca.crt sslmode=require dbname=verifydb hostaddr=$SERVERHOSTADDR";

$node->connect_ok(
	"$common_connstr user=ssltestuser sslcert=ssl/client.crt sslkey=ssl/client_tmp.key",
	"auth_option clientcert=verify-full succeeds with matching username and Common Name"
);

$node->connect_fails(
	"$common_connstr user=anotheruser sslcert=ssl/client.crt sslkey=ssl/client_tmp.key",
	"auth_option clientcert=verify-full fails with mismatching username and Common Name",
	expected_stderr =>
	  qr/FATAL: .* "trust" authentication failed for user "anotheruser"/,);

# Check that connecting with auth-optionverify-ca in pg_hba :
# works, when username doesn't match Common Name
$node->connect_ok(
	"$common_connstr user=yetanotheruser sslcert=ssl/client.crt sslkey=ssl/client_tmp.key",
	"auth_option clientcert=verify-ca succeeds with mismatching username and Common Name"
);

# intermediate client_ca.crt is provided by client, and isn't in server's ssl_ca_file
switch_server_cert($node, 'server-cn-only', 'root_ca');
$common_connstr =
  "$default_ssl_connstr user=ssltestuser dbname=certdb sslkey=ssl/client_tmp.key sslrootcert=ssl/root+server_ca.crt hostaddr=$SERVERHOSTADDR";

$node->connect_ok(
	"$common_connstr sslmode=require sslcert=ssl/client+client_ca.crt",
	"intermediate client certificate is provided by client");
$node->connect_fails(
	$common_connstr . " " . "sslmode=require sslcert=ssl/client.crt",
	"intermediate client certificate is missing",
	expected_stderr => qr/SSL error: tlsv1 alert unknown ca/);

# clean up
unlink("ssl/client_tmp.key", "ssl/client_wrongperms_tmp.key",
	"ssl/client-revoked_tmp.key");

done_testing();