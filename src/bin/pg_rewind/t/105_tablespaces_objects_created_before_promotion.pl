use strict;
use warnings;
use File::Path qw(rmtree);
use Cwd qw(abs_path realpath);
use PostgreSQL::Test::Utils;
use Test::More tests => 15;

use FindBin;
use lib $FindBin::RealBin;

use RewindTest;

sub run_test
{
	my $test_mode = shift;

	my $tablespace_location = "${PostgreSQL::Test::Utils::tmp_check}/ts";

	rmtree($tablespace_location);
	mkdir $tablespace_location;

	RewindTest::setup_cluster($test_mode);
	RewindTest::start_primary();
	RewindTest::create_standby($test_mode);

	# Create a new tablespace in the standby that will not be on the old primary.
	primary_psql("CREATE TABLESPACE ts LOCATION '$tablespace_location'");
	# Now create objects in that tablespace
	primary_psql("CREATE TABLE t_heap(i int) TABLESPACE ts");
	primary_psql("CREATE INDEX t_heap_idx ON t_heap(i) TABLESPACE ts");
	primary_psql("INSERT INTO t_heap VALUES(generate_series(1, 100))");
	primary_psql("CREATE TABLE t_ao(i int) WITH (appendonly=true) TABLESPACE ts");
	primary_psql("INSERT INTO t_ao VALUES(generate_series(1, 100))");
	primary_psql("CHECKPOINT");

	RewindTest::promote_standby();
	
	standby_psql("CREATE TABLE t_heap_after_promotion(i int) TABLESPACE ts");
	standby_psql("INSERT INTO t_heap_after_promotion VALUES(generate_series(1, 100))");
	standby_psql("INSERT INTO t_heap VALUES(generate_series(1, 100))");
	standby_psql("INSERT INTO t_ao VALUES(generate_series(1, 100))");
	standby_psql("CHECKPOINT");

	RewindTest::run_pg_rewind($test_mode, do_not_start_primary => 1);

	# Confirm that after rewind the primary has the correct symlink set up.
	my $primary_pgdata = $node_primary->data_dir;
	my $ts_oid = $node_standby->safe_psql('postgres', q{SELECT oid FROM pg_tablespace WHERE spcname = 'ts'});
	my $db_oid = $node_standby->safe_psql('postgres', q{SELECT oid FROM pg_database WHERE datname = 'postgres'});
	# Confirm that after rewind the tablespace symlink target directory has been created.
	ok(-l "$primary_pgdata/pg_tblspc/$ts_oid", "symbolic for tablespace was created: $primary_pgdata/pg_tblspc/$ts_oid");
	my $absolute_path = realpath("$primary_pgdata/pg_tblspc/$ts_oid");
	ok(-d $absolute_path, "symlink target directory for tablespace ts exists.");

	# Test that relfilenodes appear after rewind completes in primary.
	my $num_entries = `ls $absolute_path/GPDB_*/${db_oid}/ | wc -l`;
	chomp($num_entries);
	# Expect 7 relfiles (for t_heap, t_heap_idx, t_ao (with its 3 metadata tables) and t_heap_after_promotion)
	$num_entries == 7 or die "found $num_entries expected 7 files in $absolute_path/GPDB_*/${db_oid}/";

	# Restart and promote the primary to check that rewind went
	# correctly
	$node_primary->start;
	RewindTest::promote_primary();

	check_query(
		'SELECT count(*) from t_heap',
		qq(200
),
		'heap table content');

	check_query(
		'SELECT count(*) from t_ao',
		qq(200
),
		'ao table content');

	check_query(
		'SET enable_seqscan=off; SET optimizer_enable_tablescan=off; SELECT i FROM t_heap WHERE i=5',
		qq(5
5
),
		'heap index scan');

	check_query(
		'SELECT count(*) from t_heap_after_promotion',
		qq(100
),
		't_heap_after_promotion table content');

	RewindTest::clean_rewind_test();
	return;
}

# Run the test in both modes
run_test('local');
run_test('remote');

exit(0);
