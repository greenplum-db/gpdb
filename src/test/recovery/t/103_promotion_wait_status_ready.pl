# Checks that if promotion is requested in recovery_target_action
# we are waiting for recovery to complete before let pg_ctl to return
use strict;
use warnings;

use PostgresNode;
use TestLib;
use Test::More tests => 1;

# Initialize node to backup
my $node_to_backup = get_new_node('to_backup');
$node_to_backup->init(
	has_archiving    => 1,
	allows_streaming => 1);
$node_to_backup->start;

# And some content
$node_to_backup->safe_psql('postgres',
	"CREATE TABLE tab_1 AS SELECT generate_series(1, 10) AS a1");
$node_to_backup->safe_psql('postgres',
	"SELECT pg_create_restore_point('first_restore_point');");
$node_to_backup->safe_psql('postgres',
	"SELECT pg_switch_wal();");

# Take backup
my $backup_name = 'my_backup';
$node_to_backup->backup($backup_name);

# Before running the transaction get the
# current timestamp that will be used as a comparison base.
my $trx_to_archive_time = time();
# Add some more content, create restore point and switch WAL to make sure it's archived
$node_to_backup->safe_psql('postgres',
	"CREATE TABLE tab_2 AS SELECT generate_series(1, 10) AS a2");
$node_to_backup->safe_psql('postgres',
	"SELECT pg_create_restore_point('second_restore_point');");
$node_to_backup->safe_psql('postgres',
	"SELECT pg_switch_wal();");

# Create new node from from backup
my $node_restored = get_new_node('restored');
my $delay         = 5;
$node_restored->init_from_backup($node_to_backup, $backup_name,
	standby => 1, has_restoring => 1);
$node_restored->append_conf(
	'postgresql.conf', qq(
recovery_target_name = 'second_restore_point'
recovery_target_action = 'promote'
recovery_min_apply_delay = '${delay}s'
hot_standby = 'off'
));
$node_restored->start;

#Promotion can happen only after the recovery process spends
# recovery_min_apply_delay seconds en route to the recovery_target_name.
#Thus, this check ensures that pg_ctl waited for promotion to have completed before returning.

ok(time() - $trx_to_archive_time >= $delay,
	"pg_ctl starts restored node only after replication delay if recovery_target_action = 'promote' is specified");
