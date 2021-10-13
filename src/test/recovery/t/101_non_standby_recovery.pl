use strict;
use warnings;
use PostgresNode;
use TestLib;
use Test::More tests => 1;

my $node = get_new_node('master');

# Create a data directory with initdb
$node->init;

# Start the PostgreSQL server
$node->start;

# Take a backup of a stopped server
$node->stop;
my $ret = $node->backup_fs_cold('testbackup');

# Restore it to create a new independent node
my $restored_node = get_new_node('restored_node');

$restored_node->init_from_backup($node, 'testbackup', has_restoring => 1);

# Recovery in non-standby mode
$restored_node->append_conf(
	'recovery.conf', qq(
standby_mode=off
));

# Start the PostgreSQL server
$restored_node->start;

# Make sure that the server is up and running
is($restored_node->safe_psql('postgres', 'SELECT 1'), '1', 'Server start sanity check');
