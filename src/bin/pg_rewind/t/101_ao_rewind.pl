use strict;
use warnings;
use TestLib;
use Test::More tests => 17;

use FindBin;
use lib $FindBin::RealBin;

use RewindTest;

sub run_test
{
	my $test_mode = shift;

	RewindTest::setup_cluster($test_mode);
	RewindTest::start_master();

	# Create a test table and insert a row in master.
	master_psql("CREATE TABLE aotbl1 (d text) with (appendonly=true)");
	master_psql("INSERT INTO aotbl1 VALUES ('in master')");

	# This test table will be used to test truncation, i.e. the table
	# is extended in the old master after promotion
	master_psql("CREATE TABLE trunc_aotbl (d text) with (appendonly=true)");
	master_psql("INSERT INTO trunc_aotbl VALUES ('in master')");

	# This test table will be used to test the "copy-tail" case, i.e. the
	# table is truncated in the old master after promotion
	master_psql("CREATE TABLE tail_aotbl (id integer, d text) with (appendonly=true)");
	master_psql("INSERT INTO tail_aotbl VALUES (0, 'in master')");

	# Create a test table and insert a row in master.
	master_psql("CREATE TABLE cotbl1 (d text) with (appendonly=true, orientation=column)");
	master_psql("INSERT INTO cotbl1 VALUES ('in master')");

	# This test table will be used to test truncation, i.e. the table
	# is extended in the old master after promotion
	master_psql("CREATE TABLE trunc_cotbl (d text) with (appendonly=true, orientation=column)");
	master_psql("INSERT INTO trunc_cotbl VALUES ('in master')");

	# This test table will be used to test the "copy-tail" case, i.e. the
	# table is truncated in the old master after promotion
	master_psql("CREATE TABLE tail_cotbl (id integer, d text) with (appendonly=true, orientation=column)");
	master_psql("INSERT INTO tail_cotbl VALUES (0, 'in master')");

	master_psql("CHECKPOINT");

	RewindTest::create_standby($test_mode);

	# Insert additional data on master that will be replicated to standby
	master_psql("INSERT INTO aotbl1 values ('in master, before promotion')");
	master_psql(
		"INSERT INTO trunc_aotbl values ('in master, before promotion')");
	master_psql(
		"INSERT INTO tail_aotbl SELECT g, 'in master, before promotion: ' || g FROM generate_series(1, 10000) g"
	);

	# Insert additional data on master that will be replicated to standby
	master_psql("INSERT INTO cotbl1 values ('in master, before promotion')");
	master_psql(
		"INSERT INTO trunc_cotbl values ('in master, before promotion')");
	master_psql(
		"INSERT INTO tail_cotbl SELECT g, 'in master, before promotion: ' || g FROM generate_series(1, 10000) g"
	);

	master_psql('CHECKPOINT');

	RewindTest::promote_standby();

	# Insert a row in the old master. This causes the master and standby
	# to have "diverged", it's no longer possible to just apply the
	# standy's logs over master directory - you need to rewind.
	master_psql("INSERT INTO aotbl1 VALUES ('in master, after promotion')");

	# Also insert a new row in the standby, which won't be present in the
	# old master.
	standby_psql("INSERT INTO aotbl1 VALUES ('in standby, after promotion')");

	# Insert enough rows to trunc_aotbl to extend the file. pg_rewind should
	# truncate it back to the old size.
	master_psql(
		"INSERT INTO trunc_aotbl SELECT 'in master, after promotion: ' || g FROM generate_series(1, 10000) g"
	);

	# Truncate tail_aotbl. pg_rewind should copy back the truncated part
	# (We cannot use an actual TRUNCATE command here, as that creates a
	# whole new relfilenode)
	master_psql("DELETE FROM tail_aotbl WHERE id > 10");
	master_psql("VACUUM tail_aotbl");

	# Insert a row in the old master. This causes the master and standby
	# to have "diverged", it's no longer possible to just apply the
	# standy's logs over master directory - you need to rewind.
	master_psql("INSERT INTO cotbl1 VALUES ('in master, after promotion')");

	# Also insert a new row in the standby, which won't be present in the
	# old master.
	standby_psql("INSERT INTO cotbl1 VALUES ('in standby, after promotion')");

	# Insert enough rows to trunc_aotbl to extend the file. pg_rewind should
	# truncate it back to the old size.
	master_psql(
		"INSERT INTO trunc_cotbl SELECT 'in master, after promotion: ' || g FROM generate_series(1, 10000) g"
	);

	# Truncate tail_aotbl. pg_rewind should copy back the truncated part
	# (We cannot use an actual TRUNCATE command here, as that creates a
	# whole new relfilenode)
	master_psql("DELETE FROM tail_cotbl WHERE id > 10");
	master_psql("VACUUM tail_cotbl");

	# Use immediate shutdown option. Hence, in this test we also test
	# unclean shutdown case for pg_rewind. The master will be stopped
	# with immediate mode which will cause unclean shutdown and leave
	# "in production" in the control file. At the beginning of
	# pg_rewind, a single-user mode postgres session should be run to
	# ensure clean shutdown on the target instance.
	RewindTest::run_pg_rewind($test_mode, stop_master_mode => 'immediate');

	check_query(
		'SELECT * FROM aotbl1',
		qq(in master
in master, before promotion
in standby, after promotion
),
		'table content');

	check_query(
		'SELECT * FROM trunc_aotbl',
		qq(in master
in master, before promotion
),
		'truncation');

	check_query(
		'SELECT count(*) FROM tail_aotbl',
		qq(10001
),
		'tail-copy');

	check_query(
		'SELECT * FROM cotbl1',
		qq(in master
in master, before promotion
in standby, after promotion
),
		'table content');

	check_query(
		'SELECT * FROM trunc_cotbl',
		qq(in master
in master, before promotion
),
		'truncation');

	check_query(
		'SELECT count(*) FROM tail_cotbl',
		qq(10001
),
		'tail-copy');
	
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
