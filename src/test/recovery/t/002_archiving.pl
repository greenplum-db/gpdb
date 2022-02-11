# test for archiving with hot standby
use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;
use File::Copy;

# Initialize primary node, doing archives
my $node_primary = PostgreSQL::Test::Cluster->new('primary');
$node_primary->init(
	has_archiving    => 1,
	allows_streaming => 1);
my $backup_name = 'my_backup';
my $primary_connstr = $node_primary->connstr;

# Start it
$node_primary->start;

# Take backup for standby
$node_primary->backup($backup_name);

# Initialize standby node from backup, fetching WAL from archives
my $node_standby = PostgreSQL::Test::Cluster->new('standby');
# Note that this makes the standby store its contents on the archives
# of the primary.
$node_standby->init_from_backup($node_primary, $backup_name,
	has_restoring => 1);
$node_standby->append_conf('postgresql.conf',
	"wal_retrieve_retry_interval = '100ms'");
$node_standby->start;

# Create some content on primary
$node_primary->safe_psql('postgres',
	"CREATE TABLE tab_int AS SELECT generate_series(1,1000) AS a");
my $current_lsn =
  $node_primary->safe_psql('postgres', "SELECT pg_current_wal_lsn();");

# Force archiving of WAL file to make it present on primary
$node_primary->safe_psql('postgres', "SELECT pg_switch_wal()");

# Add some more content, it should not be present on standby
$node_primary->safe_psql('postgres',
	"INSERT INTO tab_int VALUES (generate_series(1001,2000))");

# Wait until necessary replay has been done on standby
my $caughtup_query =
  "SELECT '$current_lsn'::pg_lsn <= pg_last_wal_replay_lsn()";
$node_standby->poll_query_until('postgres', $caughtup_query)
  or die "Timed out while waiting for standby to catch up";

my $result =
  $node_standby->safe_psql('postgres', "SELECT count(*) FROM tab_int");
is($result, qq(1000), 'check content from archives');

$node_standby->append_conf('postgresql.conf', qq(primary_conninfo='$primary_connstr'));
$node_standby->restart;

###################### partial wal file tests ############################
# Test the following scenario:
# primary is alive but the standby is promoted. In this case, the last wal file
# on the old timeline in the mirror's pg_wal dir is renamed with the suffix ".partial"
# This partial file also gets archived. The original wal file only gets archived once
# the user runs pg_rewind.

# Consider the following example: Let's assume that 0000100004 is the current
# wal file on the primary

# start with a primary and standby pair
# add data to primary
# contents of pg_wal on primary
#         0000100001
#         .....
#         0000100003
#         0000100004 - current wal file on primary
#
# primary is alive but standby gets promoted
# contents of pg_wal on standby
#         0000100001
#         ....
#         0000100003
#         0000100004.partial (note that 0000100004 does not exist on the standby)
#         0000200004 - current wal file on standby
#
# Contents of the archive location
#         0000100003
#         0000100004
#
# stop primary with pg_ctl stop -m fast
# contents of pg_wal on primary
#         0000100004 on the primary gets flushed and gets archived
#         0000100004.done gets created on primary
# Contents of the archive location
#         0000100003
#         0000100004.partial
#         0000100004
# pg_rewind
#         copies from standby to primary
#         removes 0000100004 and 0000100004.done from primary's pg_wal dir

$node_primary->safe_psql('postgres',
	"CREATE TABLE test_partial_wal as SELECT generate_series(1,1000)");
my $latest_wal_filename_old_timeline =  $node_primary->safe_psql('postgres', "SELECT pg_walfile_name(pg_current_wal_lsn());");
my $latest_done_old_timeline = '/pg_wal/archive_status/' . $latest_wal_filename_old_timeline . '.done';
my $latest_wal_filepath_old_timeline = $node_primary->data_dir . '/pg_wal/' . $latest_wal_filename_old_timeline;
my $latest_archived_wal_old_timeline = $node_primary->archive_dir . '/' . $latest_wal_filename_old_timeline;

my $partial_wal_file_path = '/pg_wal/' . $latest_wal_filename_old_timeline . '.partial';
my $partial_done_file_path = '/pg_wal/archive_status/' . $latest_wal_filename_old_timeline . '.partial.done';
my $archived_partial_wal_file = $node_primary->archive_dir . '/' . $latest_wal_filename_old_timeline . '.partial';

#assert that 0000100004 exists on primary but it's not archived
ok(-f "$latest_wal_filepath_old_timeline", 'latest wal file from the old timeline exists on primary');
ok(!-f "$latest_archived_wal_old_timeline", 'latest wal file from the old timeline is not archived yet');

#Only promote standby once the latest wal file from the primary's current timeline has been streamed to the standby
my $primary_current_wal_loc = $node_primary->safe_psql('postgres', "SELECT pg_current_wal_lsn();");
my $query = "SELECT pg_last_wal_receive_lsn() >= '$primary_current_wal_loc'::pg_lsn;";
$node_standby->poll_query_until('postgres', $query)
  or die "Timed out while waiting for standby to receive the latest wal file";

$node_standby->promote;
# Force a checkpoint after the promotion. pg_rewind looks at the control
# file to determine what timeline the server is on, and that isn't updated
# immediately at promotion, but only at the next checkpoint. When running
# pg_rewind in remote mode, it's possible that we complete the test steps
# after promotion so quickly that when pg_rewind runs, the standby has not
# performed a checkpoint after promotion yet.
$node_standby->safe_psql('postgres', "checkpoint");# wait for the partial file to get archived

$node_standby->safe_psql('postgres',
	"INSERT INTO test_partial_wal SELECT generate_series(1,1000)");
# Once we promote the standby, it will be on a new timeline and we want to assert
# that the latest file from the old timeline is archived properly
post_standby_promotion_tests();

$node_primary->stop;
$node_standby->safe_psql('postgres',
	"INSERT INTO test_partial_wal SELECT generate_series(1,1000)");

post_primary_stop_tests();

my $tmp_check      = PostgreSQL::Test::Utils::tempdir;
my $primary_datadir = $node_primary->data_dir;
# Keep a temporary postgresql.conf for primary node or it would be
# overwritten during the rewind.
copy("$primary_datadir/postgresql.conf",
	 "$tmp_check/primary-postgresql.conf.tmp");

command_ok(['pg_rewind',
			"--debug",
			"--source-server",
            'port='. $node_standby->port . ' dbname=postgres',
			'--target-pgdata=' . $node_primary->data_dir],
		   'pg_rewind');

post_pg_rewind_tests();

# Now move back postgresql.conf with old settings
move("$tmp_check/primary-postgresql.conf.tmp",
	 "$primary_datadir/postgresql.conf");

# Start the primary
$node_primary->start;
$node_primary->safe_psql('postgres',
	"INSERT INTO test_partial_wal SELECT generate_series(1,1000)");

sub wait_until_file_exists
{
	my ($filepath, $filedesc) = @_;
	my $query = "SELECT size IS NOT NULL FROM pg_stat_file('$filepath')";
	# we aren't querying primary because we stop the primary node for some of the
	# scenarios
	$node_standby->poll_query_until('postgres', $query)
	  or die "Timed out while waiting for $filedesc $filepath";
}

sub post_standby_promotion_tests
{
	#assert that 0000100004 exists on primary
	wait_until_file_exists($latest_wal_filepath_old_timeline, "latest wal file from the old timeline to exist on primary");
	#assert that 0000100004.partial exists on standby
	wait_until_file_exists($node_standby->data_dir . $partial_wal_file_path, "partial wal file from the old timeline to exist on standby");
	#assert that 0000100004.partial.done exists on standby
	wait_until_file_exists($node_standby->data_dir . $partial_done_file_path, "partial done file from the old timeline to exist on standby");
	#assert that 0000100004.partial got archived
	wait_until_file_exists($archived_partial_wal_file, "latest partial wal file from the old timeline to be archived");

	#assert that 0000100004.partial doesn't exist on primary
	ok(!-f $node_primary->data_dir . $partial_wal_file_path, 'partial wal file from the old timeline should not exist on primary');
	#assert that 0000100004.partial.done doesn't exist on primary
	ok(!-f $node_primary->data_dir . $partial_done_file_path, 'partial done file from the old timeline should not exist on primary');
	#assert that 0000100004.done doesn't exist on primary
	ok(!-f $node_primary->data_dir . $latest_done_old_timeline, 'done file from the old timeline should not exist on primary');
	#assert that 0000100004 hasn't been archived
	ok(!-f $latest_archived_wal_old_timeline, 'wal file from the old timeline should not be archived');
	#assert that 0000100004 doesn't exist on standby
	ok(!-f $node_standby->data_dir . '/pg_wal/' . $latest_wal_filename_old_timeline, 'latest wal file from the old timeline should not exist on the standby');

	check_history_files();
}

sub post_primary_stop_tests
{
	#assert that 0000100004 still exists on primary
	wait_until_file_exists($latest_wal_filepath_old_timeline, "latest wal file from the old timeline to exist on primary");
	#assert that 0000100004.done exists on primary
	wait_until_file_exists($node_primary->data_dir . $latest_done_old_timeline, "done file from the old timeline to exist on primary");
	#assert that 0000100004 is archived
	wait_until_file_exists($latest_archived_wal_old_timeline, "latest wal file from the old timeline to be archived");
}

sub post_pg_rewind_tests
{
	#assert that 0000100004.partial exists on primary
	wait_until_file_exists($node_primary->data_dir . $partial_wal_file_path, "latest partial wal file from the old timeline to exist on primary");
	#assert that 0000100004.partial.done exists on primary
	wait_until_file_exists($node_primary->data_dir . $partial_done_file_path, "latest partial done file from the old timeline to exist on primary");

	#assert that 0000100004 is still archived
	wait_until_file_exists($latest_archived_wal_old_timeline, "latest wal file from the old timeline to be archived");
	#partial wal file is still archived
	wait_until_file_exists($archived_partial_wal_file, "latest partial wal file from the old timeline to be archived");

	#assert that 0000100004 does not exist on primary
	ok(!-f "$latest_wal_filepath_old_timeline", 'latest wal file from the old timeline should not exist on standby');
	#assert that 0000100004.done does not exist on primary
	ok(!-f $node_primary->data_dir . $latest_done_old_timeline, 'latest done file from the old timeline should not exist on primary');

}

sub check_history_files
{
	# Check the presence of temporary files specifically generated during
	# archive recovery.  To ensure the presence of the temporary history
	# file, switch to a timeline large enough to allow a standby to recover
	# a history file from an archive.  As this requires at least two timeline
	# switches, promote the existing standby first.  Then create a second
	# standby based on the primary, using its archives.  Finally, the second
	# standby is promoted.

	# Wait until the history file has been stored on the archives of the
	# primary once the promotion of the standby completes.  This ensures that
	# the second standby created below will be able to restore this file,
	# creating a RECOVERYHISTORY.
	my $primary_archive = $node_primary->archive_dir;
	wait_until_file_exists("$primary_archive/00000002.history", "history file to be archived");

	my $node_standby2 = PostgreSQL::Test::Cluster->new('standby2');
	$node_standby2->init_from_backup($node_primary, $backup_name,
		has_streaming => 1, has_restoring => 1);
	$node_standby2->start;

	my $log_location = -s $node_standby2->logfile;

	# Now promote standby2, and check that temporary files specifically
	# generated during archive recovery are removed by the end of recovery.
	$node_standby2->promote;

	# Check the logs of the standby to see that the commands have failed.
	my $log_contents       = slurp_file($node_standby2->logfile, $log_location);
	my $node_standby2_data = $node_standby2->data_dir;

	like(
		$log_contents,
		qr/restored log file "00000002.history" from archive/s,
		"00000002.history retrieved from the archives");
	ok( !-f "$node_standby2_data/pg_wal/RECOVERYHISTORY",
		"RECOVERYHISTORY removed after promotion");
	ok( !-f "$node_standby2_data/pg_wal/RECOVERYXLOG",
		"RECOVERYXLOG removed after promotion");
}

done_testing();