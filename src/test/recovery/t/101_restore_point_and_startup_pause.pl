# test for pausing on startup and on a specified restore point
use strict;
use warnings;
use PostgresNode;
use TestLib;
use Test::More tests => 1;
use File::Copy;

# Initialize primary node with WAL archiving setup
my $node_primary = get_new_node('primary');
$node_primary->init(
    has_archiving    => 1,
    allows_streaming => 1);
$node_primary->append_conf('postgresql.conf', "wal_level = 'replica'");
$node_primary->append_conf('postgresql.conf', "max_wal_senders = 10");
my $backup_name = 'my_backup';

# Start primary
$node_primary->start;

# Initialize standby node from backup, fetching WAL from archives
$node_primary->backup($backup_name);
my $node_standby = get_new_node('standby');
$node_standby->init_from_backup($node_primary, $backup_name,
    has_restoring => 1);
$node_standby->append_conf('postgresql.conf', "gp_pause_on_restore_point_replay = on");

# Start standby
$node_standby->start;

# Create a restore point on the primary
my $restore_point_lsn =
    $node_primary->safe_psql('postgres', "SELECT pg_create_restore_point('rp')");

# Force archival of WAL file to make it present on standby
$node_primary->safe_psql('postgres', "SELECT pg_switch_wal()");

# Wait until enough replay has been done on the standby before checking if replay
# is paused at the restore point
my $caughtup_query =
    "SELECT '$restore_point_lsn'::pg_lsn <= pg_last_wal_replay_lsn()";
$node_standby->poll_query_until('postgres', $caughtup_query)
    or die "Timed out while waiting for standby to catch up";

my $paused_at_restore_point_query =
    "SELECT pg_is_wal_replay_paused() and pg_last_wal_replay_lsn() = '$restore_point_lsn'::pg_lsn";
my $result2 = $node_standby->safe_psql('postgres', $paused_at_restore_point_query);
is($result2, qq(t), 'check if WAL replay is paused at restore point');
