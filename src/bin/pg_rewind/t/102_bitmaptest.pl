use strict;
use warnings;
use TestLib;
use Test::More tests => 13;

use FindBin;
use lib $FindBin::RealBin;

use RewindTest;

sub run_test
{
	my $test_mode = shift;

	RewindTest::setup_cluster($test_mode);
	RewindTest::start_master();

	# Create a test table and insert a row in master.
	master_psql("CREATE TABLE tb1 (a INT, b INT)");
	master_psql("CREATE INDEX on tb1 USING bitmap(b)");
	master_psql("CHECKPOINT");

	RewindTest::create_standby($test_mode);
	RewindTest::promote_standby();

	
	# The restart of master is done to force master and standby having
	# same oid and relfilenode. pg_control file records NextRelfilenode
	# which will be used on restart by master and standby uses the same
	# after promotion.
	$node_master->restart();
	master_psql("INSERT INTO tb1 SELECT 0, 1 FROM generate_series(1, 128)");
	master_psql("INSERT INTO tb1 SELECT 0, 0 FROM generate_series(1, 64)");
	master_psql("DELETE FROM tb1 WHERE b = 1");
	# To create hole for generating UPDATE_WORD and UPDATE_WORDS wal
	# records.
	master_psql("VACUUM verbose tb1");
	# make sure vacuum worked
 	check_query(
 		'SET gp_select_invisible to on; SELECT COUNT(*) FROM tb1',
 		qq(64
),
 		'table content after vacuum');

	# each insert serves the purpose to generate the specific wal record
	# type for bitmap.
	master_psql("INSERT INTO tb1 VALUES(0,0)");
	master_psql("INSERT INTO tb1 VALUES(0,0)");
	master_psql("INSERT INTO tb1 VALUES(0,1)");

	standby_psql("INSERT INTO tb1 SELECT 2, 3 FROM generate_series(1, 128)");
	standby_psql("INSERT INTO tb1 SELECT 2, 2 FROM generate_series(1, 64)");
	standby_psql("DELETE FROM tb1 WHERE b = 3");
	# To create hole for generating UPDATE_WORD and UPDATE_WORDS wal
	# records.
	standby_psql("VACUUM tb1");
	# make sure vacuum worked
	standby_psql("SET gp_select_invisible to on; select count(*) from tb1");
	# each insert serves the purpose to generate the specific wal record
	# type for bitmap.
	standby_psql("INSERT INTO tb1 VALUES(2,2)");
	standby_psql("INSERT INTO tb1 VALUES(2,2)");
	standby_psql("INSERT INTO tb1 VALUES(2,3)");

	RewindTest::run_pg_rewind($test_mode);

	check_query(
		'SELECT count(*) FROM tb1',
		qq(67
),
		'table content');

	check_query(
		'SET enable_seqscan=off; SET optimizer_enable_tablescan=off; SELECT count(*) from tb1 WHERE b=2',
		qq(66
),
		'for b=2');

	check_query(
		'SET enable_seqscan=off; SET optimizer_enable_tablescan=off; SELECT count(*) from tb1 WHERE b=3',
		qq(1
),
		'for b=3');

	# Permissions on PGDATA should be default
  SKIP:
	{
		skip "unix-style permissions not supported on Windows", 1
		  if ($windows_os);

		ok(check_mode_recursive($node_master->data_dir(), 0700, 0600),
			'check PGDATA permissions');
	}

	RewindTest::clean_rewind_test();
	return;
}

# Run the test in both modes
run_test('local');
run_test('remote');

exit(0);
