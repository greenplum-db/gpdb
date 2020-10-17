use strict;
use warnings;
use File::Path qw(rmtree);
use Cwd qw(abs_path realpath);
use TestLib;
use Test::More tests => 13;

use FindBin;
use lib $FindBin::RealBin;

use RewindTest;

sub run_test
{
	my $test_mode = shift;

	my $tablespace_location = "${TestLib::tmp_check}/ts";
	my $drop_tablespace_location = "${TestLib::tmp_check}/drop_ts";

	rmtree($tablespace_location);
	mkdir $tablespace_location;

	rmtree($drop_tablespace_location);
	mkdir $drop_tablespace_location;
	
	RewindTest::setup_cluster($test_mode);
	RewindTest::start_master();
	RewindTest::create_standby($test_mode);

	my $master_pgdata = $node_master->data_dir;

	# Create a new tablespace in the standby that will not be on the old master.
	master_psql("CREATE TABLESPACE ts LOCATION '$tablespace_location'");
	master_psql("CREATE TABLESPACE drop_ts LOCATION '$drop_tablespace_location'");

	my $drop_ts_oid = $node_master->safe_psql('postgres', q{SELECT oid FROM pg_tablespace WHERE spcname = 'drop_ts'});
	ok(-l "$master_pgdata/pg_tblspc/$drop_ts_oid", "symbolic for tablespace was created: $master_pgdata/pg_tblspc/$drop_ts_oid");
	my $drop_ts_absolute_path = realpath("$master_pgdata/pg_tblspc/$drop_ts_oid");
	ok(-e $drop_ts_absolute_path, "drop tablespace directory created");
	
	# Now create objects in that tablespace
	master_psql("CREATE TABLE t_heap(i int) TABLESPACE ts");
	master_psql("CREATE INDEX t_heap_idx ON t_heap(i) TABLESPACE ts");
	master_psql("INSERT INTO t_heap VALUES(generate_series(1, 100))");
	master_psql("CREATE TABLE t_ao(i int) WITH (appendonly=true) TABLESPACE ts");
	master_psql("INSERT INTO t_ao VALUES(generate_series(1, 100))");
	master_psql("CHECKPOINT");

	RewindTest::promote_standby();

	standby_psql("DROP TABLE t_heap");
	standby_psql("DROP TABLE t_ao");
	standby_psql("DROP TABLESPACE drop_ts");
	standby_psql("CHECKPOINT");

	RewindTest::run_pg_rewind($test_mode, do_not_start_master => 1);

	# Confirm that after rewind the master has the correct symlink set up.
	my $ts_oid = $node_standby->safe_psql('postgres', q{SELECT oid FROM pg_tablespace WHERE spcname = 'ts'});
	my $db_oid = $node_standby->safe_psql('postgres', q{SELECT oid FROM pg_database WHERE datname = 'postgres'});
	# Confirm that after rewind the tablespace symlink target directory has been created.
	ok(-l "$master_pgdata/pg_tblspc/$ts_oid", "symbolic for tablespace was created: $master_pgdata/pg_tblspc/$ts_oid");
	my $absolute_path = realpath("$master_pgdata/pg_tblspc/$ts_oid");
	ok(-d $absolute_path, "symlink target directory for tablespace ts exists.");

	# Test that relfilenodes appear after rewind completes in master.
	my $num_entries = `ls $absolute_path/GPDB_*/${db_oid}/ | wc -l`;
	chomp($num_entries);
	# Test that there are 0 relfilenodes as all objects have been deleted for tablespace ts.
	$num_entries == 0 or die "found $num_entries expected 0 files in $absolute_path/GPDB_*/${db_oid}/";

	ok(!-l "$master_pgdata/pg_tblspc/$drop_ts_oid", "symbolic for tablespace was created: $master_pgdata/pg_tblspc/$drop_ts_oid");

	RewindTest::clean_rewind_test();
	return;
}

# Run the test in both modes
run_test('local');
run_test('remote');

exit(0);
