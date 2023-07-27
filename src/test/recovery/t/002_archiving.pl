# test for archiving with hot standby
use strict;
use warnings;
use PostgresNode;
use TestLib;
use Test::More tests => 22;
use File::Copy;

# Initialize master node, doing archives
my $node_master = get_new_node('master');
$node_master->init(
	has_archiving    => 1,
	allows_streaming => 1);
my $backup_name = 'my_backup';
my $master_connstr = $node_master->connstr;

# Start it
$node_master->start;

# Take backup for standby
$node_master->backup($backup_name);

# Initialize standby node from backup, fetching WAL from archives
my $node_standby = get_new_node('standby');
$node_standby->init_from_backup($node_master, $backup_name,
	has_restoring => 1);
# GPDB: this GUC is not yet supported
# $node_standby->append_conf('postgresql.conf',
# 	"wal_retrieve_retry_interval = '100ms'");
$node_standby->start;

# Create some content on master
$node_master->safe_psql('postgres',
	"CREATE TABLE tab_int AS SELECT generate_series(1,1000) AS a");
my $current_lsn =
  $node_master->safe_psql('postgres', "SELECT pg_current_xlog_location();");

# Force archiving of WAL file to make it present on master
$node_master->safe_psql('postgres', "SELECT pg_switch_xlog()");

# Add some more content, it should not be present on standby
$node_master->safe_psql('postgres',
	"INSERT INTO tab_int VALUES (generate_series(1001,2000))");

# Wait until necessary replay has been done on standby
my $caughtup_query =
  "SELECT '$current_lsn'::pg_lsn <= pg_last_xlog_replay_location()";
$node_standby->poll_query_until('postgres', $caughtup_query)
  or die "Timed out while waiting for standby to catch up";

my $result =
  $node_standby->safe_psql('postgres', "SELECT count(*) FROM tab_int");
is($result, qq(1000), 'check content from archives');

$node_standby->append_conf('recovery.conf', qq(primary_conninfo='$master_connstr'));
$node_standby->restart;

###################### partial wal file tests ############################
# Test the following scenario:
# master is alive but the standby is promoted. In this case, the last wal file
# on the old timeline in the mirror's pg_xlog dir is renamed with the suffix ".partial"
# This partial file also gets archived. The original wal file only gets archived once
# the user runs pg_rewind.

# Consider the following example: Let's assume that 0000100004 is the current
# wal file on the master

# start with a master and standby pair
# add data to master
# contents of pg_xlog on master
#         0000100001
#         .....
#         0000100003
#         0000100004 - current wal file on master
#
# master is alive but standby gets promoted
# contents of pg_xlog on standby
#         0000100001
#         ....
#         0000100003
#         0000100004.partial (note that 0000100004 does not exist on the standby)
#         0000200004 - current wal file on standby
#
# Contents of the archive location
#         0000100003.tar.gz
#         0000100004.partial.tar.gz
#
# stop master with pg_ctl stop -m fast
# contents of pg_xlog on master
#         0000100004 on the master gets flushed and gets archived
#         0000100004.done gets created on master
# Contents of the archive location
#         0000100003.tar.gz
#         0000100004.partial.tar.gz
#         0000100004.tar.gz
# pg_rewind
#         copies from standby to master
#         removes 0000100004 and 0000100004.done from master's pg_xlog dir

$node_master->safe_psql('postgres',
	"CREATE TABLE test_partial_wal as SELECT generate_series(1,1000)");
my $latest_wal_filename_old_timeline =  $node_master->safe_psql('postgres', "SELECT pg_xlogfile_name(pg_current_xlog_location());");
my $latest_done_old_timeline = '/pg_xlog/archive_status/' . $latest_wal_filename_old_timeline . '.done';
my $latest_wal_filepath_old_timeline = $node_master->data_dir . '/pg_xlog/' . $latest_wal_filename_old_timeline;
my $latest_archived_wal_old_timeline = $node_master->archive_dir . '/' . $latest_wal_filename_old_timeline;

my $partial_wal_file_path = '/pg_xlog/' . $latest_wal_filename_old_timeline . '.partial';
my $partial_done_file_path = '/pg_xlog/archive_status/' . $latest_wal_filename_old_timeline . '.partial.done';
my $archived_partial_wal_file = $node_master->archive_dir . '/' . $latest_wal_filename_old_timeline . '.partial';

#assert that 0000100004 exists on master but it's not archived
ok(-f "$latest_wal_filepath_old_timeline", 'latest wal file from the old timeline exists on master');
ok(!-f "$latest_archived_wal_old_timeline", 'latest wal file from the old timeline is not archived yet');

#Only promote standby once the latest wal file from the master's current timeline has been streamed to the standby
my $master_current_xlog_loc = $node_master->safe_psql('postgres', "SELECT pg_current_xlog_location();");
my $query = "SELECT pg_last_xlog_receive_location() >= '$master_current_xlog_loc';";
$node_standby->poll_query_until('postgres', $query)
  or die "Timed out while waiting for standby to receive the latest wal file";

$node_standby->promote;
# wait for the partial file to get archived
my $archiver_query = "select count(*) = 1 from pg_stat_archiver where last_archived_wal >= '$latest_wal_filename_old_timeline.partial';";
$node_standby->poll_query_until('postgres', $archiver_query)
  or die "Timed out while waiting for the partial wal file to be archived by the standby";

$node_standby->safe_psql('postgres',
	"INSERT INTO test_partial_wal SELECT generate_series(1,1000)");
# Once we promote the standby, it will be on a new timeline and we want to assert
# that the latest file from the old timeline is archived properly
post_standby_promotion_tests();

$node_master->stop;
$node_standby->safe_psql('postgres',
	"INSERT INTO test_partial_wal SELECT generate_series(1,1000)");

post_master_stop_tests();

my $master_datadir = $node_master->data_dir;
# Keep a temporary postgresql.conf for master node or it would be
# overwritten during the rewind.
copy("$master_datadir/postgresql.conf",
	 "$tmp_check/master-postgresql.conf.tmp");

local $ENV{PGOPTIONS} = '-c gp_session_role=utility';
command_ok(['pg_rewind',
			"--debug",
			"--source-server",
            'port='. $node_standby->port . ' dbname=postgres',
			'--target-pgdata=' . $node_master->data_dir],
		   'pg_rewind');

post_pg_rewind_tests();

# Now move back postgresql.conf with old settings
move("$tmp_check/master-postgresql.conf.tmp",
	 "$master_datadir/postgresql.conf");

# Start the master
$node_master->start;
$node_master->safe_psql('postgres',
	"INSERT INTO test_partial_wal SELECT generate_series(1,1000)");

sub post_standby_promotion_tests
{
	#assert that 0000100004 exists on master
	ok(-f "$latest_wal_filepath_old_timeline", 'latest wal file from the old timeline exists on master');
	#assert that 0000100004.partial exists on standby
	ok(-f $node_standby->data_dir . $partial_wal_file_path, 'partial wal file from the old timeline exists on standby');
	#assert that 0000100004.partial.done exists on standby
	ok(-f $node_standby->data_dir . $partial_done_file_path, 'partial done file from the old timeline exists on standby');
	#assert that 0000100004.partial got archived
	ok(-f "$archived_partial_wal_file", 'latest partial wal file from the old timeline has been archived');

	#assert that 0000100004.partial doesn't exist on master
	ok(!-f $node_master->data_dir . $partial_wal_file_path, 'partial wal file from the old timeline should not exist on master');
	#assert that 0000100004.partial.done doesn't exist on master
	ok(!-f $node_master->data_dir . $partial_done_file_path, 'partial done file from the old timeline should not exist on master');
	#assert that 0000100004.done doesn't exist on master
	ok(!-f $node_master->data_dir . $latest_done_old_timeline, 'done file from the old timeline should not exist on master');
	#assert that 0000100004 hasn't been archived
	ok(!-f $latest_archived_wal_old_timeline, 'wal file from the old timeline should not be archived');
	#assert that 0000100004 doesn't exist on standby
	ok(!-f $node_standby->data_dir . '/pg_xlog/' . $latest_wal_filename_old_timeline, 'latest wal file from the old timeline should not exist on the standby');
}

sub post_master_stop_tests
{
	#assert that 0000100004 still exists on master
	ok(-f "$latest_wal_filepath_old_timeline", 'latest wal file from the old timeline exists on master');
	#assert that 0000100004.done exists on master
	ok(-f $node_master->data_dir . $latest_done_old_timeline, 'done file from the old timeline should exist on master');
	#assert that 0000100004 is archived
	ok(-f "$latest_archived_wal_old_timeline", 'latest wal file from the old timeline should be archived');
}

sub post_pg_rewind_tests
{
	#assert that 0000100004.partial exists on master
	ok(-f $node_master->data_dir . $partial_wal_file_path, 'latest partial wal file from the old timeline exists on master');
	#assert that 0000100004.partial.done exists on master
	ok(-f $node_master->data_dir . $partial_done_file_path, 'latest partial done file from the old timeline exists on master');

	#assert that 0000100004 does not exist on master
	ok(!-f "$latest_wal_filepath_old_timeline", 'latest wal file from the old timeline exists should not exist on standby');
	#assert that 0000100004.done does not exist on master
	ok(!-f $node_master->data_dir . $latest_done_old_timeline, 'latest done file from the old timeline should not exist on master');

	#assert that 0000100004 is still archived
	ok(-f "$latest_archived_wal_old_timeline", 'latest wal file from the old timeline should still be archived');
	#partial wal file is still archived
	ok(-f "$archived_partial_wal_file", 'latest partial wal file from the old timeline should still be archived');
}
