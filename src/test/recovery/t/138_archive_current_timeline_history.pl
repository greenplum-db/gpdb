# Demonstrate that setting up and starting WAL archiving on an
# already-promoted node will result in the archival of its current
# timeline history file.  The way we do this is by doing the
# following:
#
# 1. Create a primary node that is on a timeline greater than 1.
# 2. Take a basebackup of that primary node with the -Xnone flag to
#    suggest that the WAL and timeline history files will be coming
#    from a separate location (e.g. a WAL archive).
# 3. Validate specific scenarios that would fail or give unexpected
#    warnings if the timeline history file was not retrievable.
#    Specifically, we test the different recovery_target_timeline
#    values and cascade streaming replication.

use strict;
use warnings;
use PostgresNode;
use TestLib;
use Test::More tests => 15;

$ENV{PGDATABASE} = 'postgres';

# Initialize primary node
my $node_primary = get_new_node('primary');
$node_primary->init(allows_streaming => 1);
$node_primary->start;

# Take a backup
my $backup_name = 'my_backup_1';
$node_primary->backup($backup_name);

# Create a standby that will be promoted onto timeline 2
my $node_primary_tli2 = get_new_node('primary_tli2');
$node_primary_tli2->init_from_backup($node_primary, $backup_name,
	has_streaming => 1);
$node_primary_tli2->start;

# Stop and remove the primary; it's not needed anymore
$node_primary->teardown_node;

# Promote the standby which will switch it to timeline 2
$node_primary_tli2->promote;

# Enable archiving on the promoted node. The timeline 2 history file
# will be pushed to the archive. We set this up ourselves instead of
# using $node_primary_tli2->enable_archiving so that the
# archive_command will fail if it tries to archive the same file
# again.
my $node_primary_tli2_archive_dir = $node_primary_tli2->archive_dir;
$node_primary_tli2->append_conf('postgresql.conf', qq{
archive_mode = 'on'
archive_command = 'cp -n "%p" "$node_primary_tli2_archive_dir/%f"'
});
$node_primary_tli2->restart;

# Wait until the timeline 2 history file has been archived. The file
# is marked ready after recovery has completed and will be archived
# when the archiver process does its first loop but poll to make this
# check deterministic.
my $primary_archive = $node_primary_tli2->archive_dir;
$node_primary_tli2->poll_query_until('postgres',
	"SELECT size IS NOT NULL FROM pg_stat_file('pg_xlog/archive_status/00000002.history.done', true);")
  or die "Timed out while waiting for 00000002.history to be archived";
ok(-f "$primary_archive/00000002.history", 'timeline 2 history file has been successfully archived');

# Check to see if this restart will not attempt to archive the
# timeline 2 history file again.
$node_primary_tli2->restart;
my $node_primary_tli2_log_contents = slurp_file($node_primary_tli2->logfile);
ok( $node_primary_tli2_log_contents !~ qr/archive command failed with exit code 1/,
	"00000002.history was not archived twice")
  or die "00000002.history was being archived again and archiving errored out";

# Take backup of node_primary_tli2 and use -Xnone so that pg_xlog will
# be empty and restore will retrieve the necessary WAL and timeline
# history file(s) from the archive.
$backup_name = 'my_backup_2';
$node_primary_tli2->backup($backup_name, backup_options => ['-Xnone']);

# Create simple WAL that will be archived and restored
$node_primary_tli2->safe_psql('postgres', "CREATE TABLE tab_int AS SELECT 8 AS a;");

# Create a restore point to later use as the recovery_target_name
my $recovery_name = "my_target";
my $restore_point_lsn = $node_primary_tli2->safe_psql('postgres',
	"SELECT pg_create_restore_point('$recovery_name');");

# Find the next WAL segment to be archived
my $walfile_to_be_archived = $node_primary_tli2->safe_psql('postgres',
	"SELECT pg_xlogfile_name(pg_current_xlog_location());");

# Make the WAL segment eligible for archival
$node_primary_tli2->safe_psql('postgres', 'SELECT pg_switch_xlog();');

# Wait until the WAL segment has been archived
my $archive_wait_query =
  "SELECT '$walfile_to_be_archived' <= last_archived_wal FROM pg_stat_archiver;";
$node_primary_tli2->poll_query_until('postgres', $archive_wait_query)
  or die "Timed out while waiting for WAL segment to be archived";
$node_primary_tli2->teardown_node;

# Scenario 1: Initialize a new standby node from the backup using
# recovery_target_timeline explicitly set to '2'. This node will start
# off on timeline 2 according to the control file and will finish
# recovery onto the same timeline. When explicitly setting the
# timeline id, startup will fail if the timeline history file is not
# retrievable from the archive but will not fail if we use 'current'
# or 'latest'.
my $node_standby_explicit = get_new_node('standby_explicit');
$node_standby_explicit->init_from_backup($node_primary_tli2, $backup_name,
	has_restoring => 1, standby => 0);
$node_standby_explicit->append_conf('postgresql.conf', qq{
archive_mode = 'off'
});
$node_standby_explicit->append_conf('recovery.conf', qq{
recovery_target_timeline = '2'
recovery_target_action = 'pause'
recovery_target_name = 'my_target'
primary_conninfo = ''
});
$node_standby_explicit->start;
standby_sanity_check($node_standby_explicit, $restore_point_lsn);
$node_standby_explicit->teardown_node;

# Scenario 2: Initialize a new standby node from the backup using
# recovery_target_timeline set to 'current' (which is the default
# value when not specifying recovery_target_timeline and can only be
# used by not setting recovery_target_timeline).  As such, we don't
# set recovery_target_timeline in the below recovery.conf so that it
# will use the default 'current' value.  This node will start off on
# timeline 2 according to the control file and will finish recovery on
# the same timeline.  If the timeline history file was not retrievable
# from the archive, the standby would just log a warning and proceed
# normally which is not desirable.
my $node_standby_current = get_new_node('standby_current');
$node_standby_current->init_from_backup($node_primary_tli2, $backup_name,
	has_restoring => 1, standby => 0);
$node_standby_current->append_conf('postgresql.conf', qq{
archive_mode = 'off'
});
$node_standby_current->append_conf('recovery.conf', qq{
recovery_target_action = 'pause'
recovery_target_name = 'my_target'
primary_conninfo = ''
});
$node_standby_current->start;
standby_sanity_check($node_standby_current, $restore_point_lsn);
$node_standby_current->teardown_node;

# Scenario 3: Initialize a new standby node from the backup using
# recovery_target_timeline set to 'latest'. This node will start off
# on timeline 2 according to the control file and will finish recovery
# onto the same timeline.  If the timeline history file was not
# retrievable from the archive, the standby would just log a warning
# and proceed normally which is not desirable.
my $node_standby_latest = get_new_node('standby_latest');
$node_standby_latest->init_from_backup($node_primary_tli2, $backup_name,
	has_restoring => 1, standby => 0);
$node_standby_latest->append_conf('postgresql.conf', qq{
archive_mode = 'off'
});
$node_standby_latest->append_conf('recovery.conf', qq{
recovery_target_timeline = 'latest'
recovery_target_action = 'pause'
recovery_target_name = 'my_target'
primary_conninfo = ''
});
$node_standby_latest->start;
standby_sanity_check($node_standby_latest, $restore_point_lsn);

# Scenario 4: Set up a cascade standby node to validate that there's
# no issues since the WAL receiver will request all necessary timeline
# history files from the standby node's WAL sender.  If the timeline
# history file was not retrievable from the standby node, the cascade
# standby node would continuously loop trying to re-request the
# timeline history file and always fail.
my $node_cascade = get_new_node('cascade');
$node_cascade->init_from_backup($node_primary_tli2, $backup_name,
	standby => 1);
$node_cascade->enable_streaming($node_standby_latest);
$node_cascade->start;

# Wait for the replication to catch up
$node_standby_latest->wait_for_catchup($node_cascade, 'replay', $node_standby_latest->lsn('replay'));

# Sanity check that the cascade standby node came up and is queryable
my $result_cascade =
  $node_cascade->safe_psql('postgres', "SELECT * FROM tab_int;");
is($result_cascade, qq(8), 'check that the node received the streamed WAL data');

$node_cascade->teardown_node;
$node_standby_latest->teardown_node;

sub standby_sanity_check
{
	my ($node_standby, $restore_point_lsn) = @_;

	# Sanity check that the timeline history file was retrieved
	my $node_standby_log_contents = slurp_file($node_standby->logfile);
	ok( $node_standby_log_contents =~ qr/restored log file "00000002.history" from archive/,
		"00000002.history retrieved from the archives");
	ok (-f $node_standby->data_dir . "/pg_xlog/00000002.history",
		"00000002.history exists in the standby's pg_xlog directory");

	# Wait until necessary replay has been done on standby
	my $caughtup_query =
	  "SELECT '$restore_point_lsn'::pg_lsn <= pg_last_xlog_replay_location()";
	$node_standby->poll_query_until('postgres', $caughtup_query)
	  or die "Timed out while waiting for standby to catch up";

	# Check that the current timeline from the control file is 2
	my ($stdout, $stderr) = run_command([ 'pg_controldata', $node_standby->data_dir ]);
	my @control_data = split("\n", $stdout);
	my $standby_timeline_id = undef;
	foreach (@control_data)
	{
		if ($_ =~ /^Latest checkpoint's TimeLineID:\s*(\d+)$/mg)
		{
			$standby_timeline_id = $1;
			last;
		}
	}
	die "No timeline id in control file found\n"
		unless defined($standby_timeline_id);
	ok($standby_timeline_id == 2, "check that the node is on timeline 2");

	# Sanity check that the node is queryable
	my $result_standby =
	  $node_standby->safe_psql('postgres', "SELECT * FROM tab_int;");
	is($result_standby, qq(8), 'check that the node did archive recovery');

	return;
}
