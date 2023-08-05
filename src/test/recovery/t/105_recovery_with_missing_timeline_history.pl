# Test that we can do a recovery when the timeline history file is
# unavailable and the recovery_target_timeline requested is equal to
# the timeline in the control file.
use strict;
use warnings;
use File::Path qw(rmtree);
use PostgresNode;
use TestLib;
use Test::More tests => 3;

$ENV{PGDATABASE} = 'postgres';

# Initialize master node
my $node_master = get_new_node('master');
$node_master->init(allows_streaming => 1);
$node_master->start;

# Take a backup
my $backup_name = 'my_backup_1';
$node_master->backup($backup_name);

# Create a standby linking to it
my $node_standby_1 = get_new_node('standby_1');
$node_standby_1->init_from_backup($node_master, $backup_name,
	has_streaming => 1);
$node_standby_1->start;

# Stop and remove master
$node_master->teardown_node;

# Promote standby using "pg_promote", switching it to a new timeline
my $psql_out = '';
$node_standby_1->psql(
	'postgres',
	"SELECT pg_promote(wait_seconds => 300)",
	stdout => \$psql_out);
is($psql_out, 't', "promotion of standby with pg_promote");

# Take a backup for new standby
$backup_name = 'my_backup_2';
$node_standby_1->backup($backup_name);
$node_standby_1->teardown_node;

# Scenario 1: Initialize new standby node from the backup. This node
# will recover and promote onto the same timeline designated in the
# control file. The timeline 2 history file is removed to show that
# this type of recovery does not require a timeline history file to be
# retrieved.
my $node_standby_2 = get_new_node('standby_2');
$node_standby_2->init_from_backup($node_standby_1, $backup_name,
	has_restoring => 1);
$node_standby_2->append_conf('postgresql.conf', qq{
recovery_target_timeline = 'current'
recovery_target_action = 'promote'
});
unlink $node_standby_2->data_dir . "/pg_wal/00000002.history";
$node_standby_2->start;

# Sanity check that the node came up and is queryable
my $result_1 =
  $node_standby_2->safe_psql('postgres', "SELECT 1");
is($result_1, qq(1), 'check that the node is queryable');
$node_standby_2->teardown_node;

# Scenario 2: Initialize new standby node from the backup. This node
# will already be on timeline 2 according to the control file and will
# finish recovery onto the same timeline by setting
# recovery_target_timeline to '2'. The timeline 2 history file is
# removed to test that this scenario acts the same as setting
# recovery_target_timeline to 'current' which does not require a
# timeline history file to be retrieved.
my $node_standby_3 = get_new_node('standby_3');
$node_standby_3->init_from_backup($node_standby_1, $backup_name,
	has_restoring => 1);
$node_standby_3->append_conf('postgresql.conf', qq{
recovery_target_timeline = '2'
recovery_target_action = 'promote'
});
unlink $node_standby_3->data_dir . "/pg_wal/00000002.history";
$node_standby_3->start;

# Sanity check that the node came up and is queryable
my $result_2 =
  $node_standby_3->safe_psql('postgres', "SELECT 1");
is($result_2, qq(1), 'check that the node is queryable');
$node_standby_3->teardown_node;
