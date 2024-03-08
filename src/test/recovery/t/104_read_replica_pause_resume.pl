# Test pause in recovery and resume
use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More tests => 17;

# Initialize primary node
my $node_primary = PostgreSQL::Test::Cluster->new('primary');
$node_primary->init(has_archiving => 1, allows_streaming => 1);
# Start it
$node_primary->start;

# Enable gp_pitr extension
$node_primary->safe_psql('postgres', "CREATE EXTENSION gp_pitr;");

# Create data before taking the backupj
$node_primary->safe_psql('postgres', "CREATE TABLE table_foo AS SELECT generate_series(1,1000);");
# Take backup from which all operations will be run
$node_primary->backup('my_backup');
$node_primary->safe_psql('postgres', "SELECT pg_create_restore_point('rp0');");
my $lsn0 = $node_primary->safe_psql('postgres', "SELECT pg_current_wal_lsn();");
$node_primary->safe_psql('postgres', "SELECT pg_switch_wal();");

# Add more data, create restore points and switch wal
# rp1
$node_primary->safe_psql('postgres', "INSERT INTO table_foo VALUES (generate_series(1001,2000))");
$node_primary->safe_psql('postgres', "SELECT pg_create_restore_point('rp1');");
my $lsn1 = $node_primary->safe_psql('postgres', "SELECT pg_current_wal_lsn();");
$node_primary->safe_psql('postgres', "SELECT pg_switch_wal();");

# rp2
$node_primary->safe_psql('postgres', "INSERT INTO table_foo VALUES (generate_series(2001, 3000))");
$node_primary->safe_psql('postgres', "SELECT pg_create_restore_point('rp2');");
my $lsn2 = $node_primary->safe_psql('postgres', "SELECT pg_current_wal_lsn();");
$node_primary->safe_psql('postgres', "SELECT pg_switch_wal();");

# rp3
$node_primary->safe_psql('postgres', "INSERT INTO table_foo VALUES (generate_series(3001, 4000))");
$node_primary->safe_psql('postgres', "SELECT pg_create_restore_point('rp3');");
my $lsn3 = $node_primary->safe_psql('postgres', "SELECT pg_current_wal_lsn();");
$node_primary->safe_psql('postgres', "SELECT pg_switch_wal();");

# Restore the backup
my $node_standby = PostgreSQL::Test::Cluster->new("standby");
	$node_standby->init_from_backup($node_primary, 'my_backup',
		has_restoring => 1);

# Set `hot_standby` and `gp_pause_in_recovery` GUCs
$node_standby->append_conf('postgresql.conf', qq(hot_standby = 'on'));
$node_standby->append_conf('postgresql.conf', qq(gp_pause_in_recovery = 'on'));
# Start the standby, it should pause before lsn0
$node_standby->start;

# Make sure recovery is paused ASAP
ok($node_standby->safe_psql('postgres', 'SELECT pg_is_in_recovery();') eq 't',
	'standby is in recovery after restore');
ok($node_standby->safe_psql('postgres', 'SELECT pg_is_wal_replay_paused();') eq 't',
	'standby is paused in recovery after restore');
ok($node_standby->safe_psql('postgres', "SELECT pg_last_wal_replay_lsn() < '$lsn0'::pg_lsn;")  eq 't',
	'standby has not replayed rp0 yet');

sub test_pause_in_recovery
{
	my ($restore_point, $test_lsn, $num_rows) = @_;
	# Set recovery target to pause and resume
	$node_standby->safe_psql('postgres', "SELECT gp_set_recovery_pause_target('$restore_point');");
	$node_standby->safe_psql('postgres', "SELECT pg_wal_replay_resume();");
	# Wait until standby has replayed enough data
	my $caughtup_query = "SELECT '$test_lsn'::pg_lsn <= pg_last_wal_replay_lsn()";
	$node_standby->poll_query_until('postgres', $caughtup_query)
		or die "Timed out while waiting for standby to catch up";
	# Check data has been replayed
	my $result = $node_standby->safe_psql('postgres', "SELECT count(*) FROM table_foo;");
	is($result, $num_rows, "check standby content for $restore_point");
	ok($node_standby->safe_psql('postgres', 'SELECT pg_is_in_recovery();') eq 't',
		"standby is in recovery on $restore_point");
	ok($node_standby->safe_psql('postgres', 'SELECT pg_is_wal_replay_paused();') eq 't',
		"standby is paused in recovery on $restore_point");
}

test_pause_in_recovery('rp0', $lsn0, 1000);
test_pause_in_recovery('rp1', $lsn1, 2000);
test_pause_in_recovery('rp2', $lsn2, 3000);
test_pause_in_recovery('rp3', $lsn3, 4000);

# Run promote then resume recovery
$node_standby->safe_psql('postgres', "SELECT pg_promote(false);");
$node_standby->safe_psql('postgres', "SELECT pg_wal_replay_resume();");
# Wait for standby to promote
$node_standby->poll_query_until('postgres', "SELECT NOT pg_is_in_recovery();")
	or die "Timed out while waiting for standby to exit recovery";
my $result = $node_standby->safe_psql('postgres', "SELECT count(*) FROM table_foo;");
is($result, 4000, "check standby content after promotion");
# Make sure former standby is now writable
$node_standby->safe_psql('postgres', "CREATE TABLE table_boo AS SELECT generate_series(1,1000);");
$result = $node_standby->safe_psql('postgres', "SELECT count(*) FROM table_boo;");
is($result, 1000, "check standby is writable after promotion");

$node_primary->teardown_node;
$node_standby->teardown_node;

done_testing();