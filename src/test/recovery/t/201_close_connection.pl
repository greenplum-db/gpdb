# Check if backend stopped after client disconnection
# FIXME: this test should really be among the src/test/modules tests. 
# However because currently modules are not recursed into for installcheck,
# we put this test here for now in order to have it running in pipeline. 
# We should move it to modules/connection like 6X once we could run modules tests
# in pipeline (either let installcheck recurse into modules, or run modules separately).
use strict;
use warnings;
use PostgresNode;
use TestLib;
use Test::More;
use File::Copy;

if ($ENV{with_openssl} eq 'yes')
{
    plan tests => 3;
}
else
{
    plan tests => 2;
}

my $long_query = q{
    SELECT pg_sleep(60);
};
my $set_guc_on = q{
    SET client_connection_check_interval = 1000;
};
my $set_guc_off = q{
    SET client_connection_check_interval = 0;
};
my ($pid, $timed_out);

my $node = get_new_node('node');
$node->init;
$node->start;

#########################################################
# TEST 1: GUC client_connection_check_interval: enabled #
#########################################################

# Set GUC options, get backend pid and run a long time query
# Leverage the timeout argument to psql to cause client 
# termination without causing immediate backend termination
$node->psql('postgres', "$set_guc_on SELECT pg_backend_pid(); $long_query",
            stdout => \$pid, timeout => 1, timed_out => \$timed_out);

# Give time to the backend to detect client disconnected
sleep 4;
# Check if backend is still alive
my $is_alive = $node->safe_psql('postgres', "SELECT count(*) FROM pg_stat_activity where pid = $pid;");
is($is_alive, '0', 'Test: client_connection_check_interval enable');
$node->stop;

##########################################################
# TEST 2: GUC client_connection_check_interval: disabled #
##########################################################

$node->start;
$node->psql('postgres', "$set_guc_off SELECT pg_backend_pid(); $long_query",
            stdout => \$pid, timeout => 1, timed_out => \$timed_out);
# Give time to the client to disconnect
sleep 4;
# Check if backend is still alive
$is_alive = $node->safe_psql('postgres', "SELECT count(*) FROM pg_stat_activity where pid = $pid;");
is($is_alive, '1', 'Test: client_connection_check_interval disable');
$node->stop;

##########################################################
# TEST 3: Using client_connection_check_interval when    #
#         client connected using SSL                     #
##########################################################

if ($ENV{with_openssl} eq 'yes')
{
    # The client's private key must not be world-readable, so take a copy
    # of the key stored in the code tree and update its permissions.
    copy("../../ssl/ssl/client.key", "../../ssl/ssl/client_tmp.key");
    chmod 0600, "../../ssl/ssl/client_tmp.key";
    copy("../../ssl/ssl/client-revoked.key", "../../ssl/ssl/client-revoked_tmp.key");
    chmod 0600, "../../ssl/ssl/client-revoked_tmp.key";
    $ENV{PGHOST} = $node->host;
    $ENV{PGPORT} = $node->port;

    open my $sslconf, '>', $node->data_dir . "/sslconfig.conf";
    print $sslconf "ssl=on\n";
    print $sslconf "ssl_cert_file='server-cn-only.crt'\n";
    print $sslconf "ssl_key_file='server-password.key'\n";
    print $sslconf "ssl_passphrase_command='echo secret1'\n";
    close $sslconf;

    $node->start;
    $node->psql('postgres', "$set_guc_on SELECT pg_backend_pid(); $long_query",
                stdout => \$pid, timeout => 1, timed_out => \$timed_out,
                sslmode => 'require');

    # Give time to the backend to detect client disconnected
    sleep 4;
    # Check if backend is still alive
    my $is_alive = $node->safe_psql('postgres', "SELECT count(*) FROM pg_stat_activity where pid = $pid;");
    is($is_alive, '0', 'Test: client_connection_check_interval enabled, SSL');
    $node->stop;
}
