use strict;
use warnings;
use PostgresNode;
use TestLib;
use Test::More tests => 3;

my $node = get_new_node('master');

# Create a data directory with initdb
$node->init(has_archiving    => 1);
$node->append_conf(
		'postgresql.conf', q{
		wal_level = 'archive'
	});

# Start the PostgreSQL server
$node->start;

# Take a backup of a running server
$node->backup_fs_hot('testbackup');

# Create a couple of tables: heap, append-optimized, columnar append-optimized
# Restored cluster should replay these actions later
$node->safe_psql(
	'postgres', "
	BEGIN;
	CREATE TABLE heap AS SELECT a FROM generate_series(1,10) AS a;
	CREATE TABLE ao(a int, b int) WITH (appendoptimized = true) DISTRIBUTED BY (a);
	CREATE TABLE co(a int, b int) WITH (appendoptimized = true, orientation = column) DISTRIBUTED BY (a);
	INSERT INTO ao select i, i FROM generate_series(1,10)i;
	INSERT INTO co select i, i FROM generate_series(1,10)i;
	COMMIT;");

my $lsn =
  $node->safe_psql('postgres', "SELECT pg_current_wal_lsn();");

# Stop the PostgreSQL server
$node->stop;

# Restore it to create a new independent node
my $restored_node = get_new_node('restored_node');

# Recovery in non-standby mode
$restored_node->init_from_backup($node, 'testbackup', has_restoring => 1, standby => 0);

# Start the PostgreSQL server
$restored_node->start;

# Wait until restored node has replayed the data.
my $caughtup_query =
	"SELECT '$lsn'::pg_lsn <= pg_last_wal_replay_lsn()";
$restored_node->poll_query_until('postgres', $caughtup_query)
	or die "Timed out while waiting for restored node to catch up";

is($restored_node->safe_psql('postgres', 'SELECT count(*) from ao'), '10', 'AO table read check');
is($restored_node->safe_psql('postgres', 'SELECT count(*) from co'), '10', 'AOCS table read check');
is($restored_node->safe_psql('postgres', 'SELECT count(*) from heap'), '10', 'Heap table read check');

