use strict;
use warnings;
use PostgreSQL::Test::Utils;
use Test::More;

use FindBin;
use lib $FindBin::RealBin;

use RewindTest;

sub run_test
{
	my $test_mode = shift;

	RewindTest::setup_cluster($test_mode);
	RewindTest::start_primary();

	# Create a test table and insert a row in primary.
	primary_psql("CREATE TABLE tbl1 (d text)");
	primary_psql("INSERT INTO tbl1 VALUES ('in primary')");

	# This test table will be used to test truncation, i.e. the table
	# is extended in the old primary after promotion
	primary_psql("CREATE TABLE trunc_tbl (d text)");
	primary_psql("INSERT INTO trunc_tbl VALUES ('in primary')");

	# This test table will be used to test the "copy-tail" case, i.e. the
	# table is truncated in the old primary after promotion
	primary_psql("CREATE TABLE tail_tbl (id integer, d text)");
	primary_psql("INSERT INTO tail_tbl VALUES (0, 'in primary')");

	primary_psql("CHECKPOINT");

	RewindTest::create_standby($test_mode);

	# Insert additional data on primary that will be replicated to standby
	primary_psql("INSERT INTO tbl1 values ('in primary, before promotion')");
	primary_psql(
		"INSERT INTO trunc_tbl values ('in primary, before promotion')");
	primary_psql(
		"INSERT INTO tail_tbl SELECT g, 'in primary, before promotion: ' || g FROM generate_series(1, 10000) g"
	);

	primary_psql('CHECKPOINT');

	RewindTest::promote_standby();

	# Insert a row in the old primary. This causes the primary and standby
	# to have "diverged", it's no longer possible to just apply the
	# standy's logs over primary directory - you need to rewind.
	primary_psql("INSERT INTO tbl1 VALUES ('in primary, after promotion')");

	# Also insert a new row in the standby, which won't be present in the
	# old primary.
	standby_psql("INSERT INTO tbl1 VALUES ('in standby, after promotion')");

	# Insert enough rows to trunc_tbl to extend the file. pg_rewind should
	# truncate it back to the old size.
	primary_psql(
		"INSERT INTO trunc_tbl SELECT 'in primary, after promotion: ' || g FROM generate_series(1, 10000) g"
	);

	# Truncate tail_tbl. pg_rewind should copy back the truncated part
	# (We cannot use an actual TRUNCATE command here, as that creates a
	# whole new relfilenode)
	primary_psql("DELETE FROM tail_tbl WHERE id > 10");
	primary_psql("VACUUM tail_tbl");

	RewindTest::run_pg_rewind($test_mode);

	check_query(
		'SELECT * FROM tbl1',
		qq(in primary
in primary, before promotion
in standby, after promotion
),
		'table content');

	check_query(
		'SELECT * FROM trunc_tbl',
		qq(in primary
in primary, before promotion
),
		'truncation');

	check_query(
		'SELECT count(*) FROM tail_tbl',
		qq(10001
),
		'tail-copy');

	# Permissions on PGDATA should be default
  SKIP:
	{
		skip "unix-style permissions not supported on Windows", 1
		  if ($windows_os);

		ok(check_mode_recursive($node_primary->data_dir(), 0700, 0600),
			'check PGDATA permissions');
	}

	RewindTest::clean_rewind_test();
	return;
}

# Run the test in both modes
run_test('local');
run_test('remote');

done_testing();
