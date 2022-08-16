# This module sets up a test server, for the SSL regression tests.
#
# The server is configured as follows:
#
# - SSL enabled, with the server certificate specified by argument to
#   switch_server_cert function.
# - ssl/root+client_ca.crt as the CA root for validating client certs.
# - reject non-SSL connections
# - a database called trustdb that lets anyone in
# - another database called certdb that uses certificate authentication, ie.
#   the client must present a valid certificate signed by the client CA
# - two users, called ssltestuser and anotheruser.
#
# The server is configured to only accept connections from localhost. If you
# want to run the client from another host, you'll have to configure that
# manually.
package ServerSetup;

use strict;
use warnings;
use PostgresNode;
use TestLib;
use File::Basename;
use File::Copy;
use Test::More;

use Exporter 'import';
our @EXPORT = qw(
  configure_test_server_for_ssl switch_server_cert test_connect_ok
);

# Define a couple of helper functions to test connecting to the server.

# Attempt connection to server with given connection string.
sub run_test_psql
{
	my $connstr   = $_[0];
	my $logstring = $_[1];

	local $ENV{PGOPTIONS} = '-c gp_session_role=utility';

	my $cmd = [
		'psql', '-X', '-A', '-t', '-c', "SELECT 'connected with $connstr'",
		'-d', "$connstr" ];

	my $result = run_log($cmd);
	return $result;
}

#
# The first argument is a base connection string to use for connection.
# The second argument is a complementary connection string, and it's also
# printed out as the test case name.
sub test_connect_ok
{
	my $common_connstr = $_[0];
	my $connstr = $_[1];
	my $test_name = $_[2];

	my $result =
	  run_test_psql("$common_connstr $connstr", "(should succeed)");
	ok($result, $test_name || $connstr);
}

sub test_connect_fails
{
	my $common_connstr = $_[0];
	my $connstr = $_[1];
	my $test_name = $_[2];

	my $result = run_test_psql("$common_connstr $connstr", "(should fail)");
	ok(!$result, $test_name || "$connstr (should fail)");
}

# Copy a set of files, taking into account wildcards
sub copy_files
{
	my $orig = shift;
	my $dest = shift;

	my @orig_files = glob $orig;
	foreach my $orig_file (@orig_files)
	{
		my $base_file = basename($orig_file);
		copy($orig_file, "$dest/$base_file")
		  or die "Could not copy $orig_file to $dest";
	}
}

sub configure_test_server_for_ssl
{
	my ($node, $serverhost, $authmethod, $password, $password_enc) = @_;

	my $pgdata = $node->data_dir;

	# Create test users and databases
	$node->psql('postgres', "CREATE USER ssltestuser");
	$node->psql('postgres', "CREATE USER anotheruser");
	$node->psql('postgres', "CREATE DATABASE trustdb");
	$node->psql('postgres', "CREATE DATABASE certdb");

	# Update password of each user as needed.
	if (defined($password))
	{
		$node->psql('postgres',
"SET password_hash_algorithm='$password_enc'; ALTER USER ssltestuser PASSWORD '$password';");
		$node->psql('postgres',
"SET password_hash_algorithm='$password_enc'; ALTER USER anotheruser PASSWORD '$password';");
	}

	# enable logging etc.
	open CONF, ">>$pgdata/postgresql.conf";
	print CONF "fsync=off\n";
	print CONF "log_connections=on\n";
	print CONF "log_hostname=on\n";
	print CONF "listen_addresses='$serverhost'\n";
	print CONF "log_statement=all\n";

	# enable SSL and set up server key
	print CONF "include 'sslconfig.conf'";

	close CONF;
	# ssl configuration will be placed here
	open my $sslconf, '>', "$pgdata/sslconfig.conf";
	close $sslconf;

# Copy all server certificates and keys, and client root cert, to the data dir
	copy_files("ssl/server-*.crt", $pgdata);
	copy_files("ssl/server-*.key", $pgdata);
	chmod(0600, glob "$pgdata/server-*.key") or die $!;
	copy_files("ssl/root+client_ca.crt", $pgdata);
	copy_files("ssl/root_ca.crt", $pgdata);
	copy_files("ssl/root+client.crl",    $pgdata);

	# Stop and restart server to load new listen_addresses.
	$node->restart;

	# Change pg_hba after restart because hostssl requires ssl=on
	configure_hba_for_ssl($node, $serverhost, $authmethod);
}

# Change the configuration to use given server cert file, and restart
# the server so that the configuration takes effect.
sub switch_server_cert
{
	my $node     = $_[0];
	my $certfile = $_[1];
	my $cafile = $_[2] || "root+client_ca";
	my $pgdata   = $node->data_dir;

	note "Restarting server with certfile \"$certfile\" and cafile \"$cafile\"...";

	open SSLCONF, ">$pgdata/sslconfig.conf";
	print SSLCONF "ssl=on\n";
	print SSLCONF "ssl_ca_file='$cafile.crt'\n";
	print SSLCONF "ssl_cert_file='$certfile.crt'\n";
	print SSLCONF "ssl_key_file='$certfile.key'\n";
	print SSLCONF "ssl_crl_file='root+client.crl'\n";
	close SSLCONF;

	# Stop and restart server to reload the new config.
	$node->restart;
}

sub configure_hba_for_ssl
{
	my ($node, $serverhost, $authmethod) = @_;
	my $pgdata     = $node->data_dir;

  # Only accept SSL connections from localhost. Our tests don't depend on this
  # but seems best to keep it as narrow as possible for security reasons.
  #
  # When connecting to certdb, also check the client certificate.
	open my $hba, '>', "$pgdata/pg_hba.conf";
	print $hba
"# TYPE  DATABASE        USER            ADDRESS                 METHOD\n";
	print $hba
"hostssl trustdb         ssltestuser     $serverhost/32            $authmethod\n";
	print $hba
"hostssl trustdb         ssltestuser     ::1/128                 $authmethod\n";
	print $hba
"hostssl certdb          ssltestuser     $serverhost/32            cert\n";
	print $hba
"hostssl certdb          ssltestuser     ::1/128                 cert\n";
	close $hba;
}
