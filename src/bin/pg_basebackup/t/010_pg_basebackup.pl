use strict;
use warnings;
use Cwd;
use TestLib;
use File::Compare;
use File::Path qw(rmtree);
use Test::More tests => 48;

program_help_ok('pg_basebackup');
program_version_ok('pg_basebackup');
program_options_handling_ok('pg_basebackup');

my $tempdir = tempdir;
start_test_server $tempdir;

command_fails(['pg_basebackup', '--target-gp-dbid', '123'],
	'pg_basebackup needs target directory specified');
command_fails(
	[ 'pg_basebackup', '-D', "$tempdir/backup", '--target-gp-dbid', '123' ],
	'pg_basebackup fails because of hba');

# Some Windows ANSI code pages may reject this filename, in which case we
# quietly proceed without this bit of test coverage.
if (open BADCHARS, ">>$tempdir/pgdata/FOO\xe0\xe0\xe0BAR")
{
	print BADCHARS "test backup of file with non-UTF8 name\n";
	close BADCHARS;
}

configure_hba_for_replication "$tempdir/pgdata";
system_or_bail 'pg_ctl', '-D', "$tempdir/pgdata", 'reload';

command_fails(['pg_basebackup', '-D', "$tempdir/backup"],
	'pg_basebackup fails without specifiying the target greenplum db id');


#
# GPDB: The minimum value of max_wal_senders is 2 in GPDB
# instead of 0 in Postgres.
#
# This test is disabled because it is difficult to
# set up an environment that consumes all of the slots
# without setting up mirrors.
#
# open CONF, ">>$tempdir/pgdata/postgresql.conf";
# print CONF "max_wal_senders = 0\n";
# close CONF;
# restart_test_server;

# command_fails(
# 	[ 'pg_basebackup', '-D', "$tempdir/backup", '--target-gp-dbid', '123' ],
# 	'pg_basebackup fails because of WAL configuration');

open CONF, ">>$tempdir/pgdata/postgresql.conf";
print CONF "max_wal_senders = 2\n";
print CONF "wal_level = archive\n";
close CONF;
restart_test_server;

command_ok([ 'pg_basebackup', '-D', "$tempdir/backup", '--target-gp-dbid', '123'],
	'pg_basebackup runs');
ok(-f "$tempdir/backup/PG_VERSION", 'backup was created');

command_ok(
	[   'pg_basebackup', '-D', "$tempdir/backup2", '--xlogdir',
		"$tempdir/xlog2", '--target-gp-dbid', '123' ],
	'separate xlog directory');
ok(-f "$tempdir/backup2/PG_VERSION", 'backup was created');
ok(-d "$tempdir/xlog2/",             'xlog directory was created');

command_ok([ 'pg_basebackup', '-D', "$tempdir/tarbackup", '-Ft',
			 '--target-gp-dbid', '123' ],
	'tar format');
ok(-f "$tempdir/tarbackup/base.tar", 'backup tar was created');

########################## Test that the headers are zeroed out in both the primary and mirror WAL files
my $compare_tempdir = "$tempdir/checksum_test";

# Ensure that when pg_basebackup is run that the last WAL segment file
# containing the XLOG_BACKUP_END and XLOG_SWITCH records match on both
# the primary and mirror segment. We want to ensure that all pages after
# the XLOG_SWITCH record are all zeroed out. Previously, the primary
# segment's WAL segment file would have interleaved page headers instead
# of all zeros. Although the WAL segment files from the primary and
# mirror segments were logically the same, they were different physically
# and would lead to checksum mismatches for external tools that checked
# for that.

#Insert data and then run pg_basebackup
psql 'postgres',  'CREATE TABLE zero_header_test as SELECT generate_series(1,1000);';
command_ok([ 'pg_basebackup', '-D', $compare_tempdir, '--target-gp-dbid', '123' , '-X', 'stream'],
	'pg_basebackup wal file comparison test');
ok( -f "$compare_tempdir/PG_VERSION", 'pg_basebackup ran successfully');

my $current_wal_file = psql 'postgres', "SELECT pg_xlogfile_name(pg_current_xlog_location());";
my $primary_wal_file_path = "$tempdir/pgdata/pg_xlog/$current_wal_file";
my $mirror_wal_file_path = "$compare_tempdir/pg_xlog/$current_wal_file";

## Test that primary and mirror WAL file is the same
ok(compare($primary_wal_file_path, $mirror_wal_file_path) eq 0, "wal file comparison");

## Test that all the bytes after the last written record in the WAL file are zeroed out
my $total_bytes_cmd = 'pg_controldata ' . $compare_tempdir .  ' | grep "Bytes per WAL segment:" |  awk \'{print $5}\'';
my $total_allocated_bytes = `$total_bytes_cmd`;

my $current_lsn_cmd = 'pg_xlogdump -f ' . $primary_wal_file_path . ' | grep "xlog switch" | awk \'{print $10}\' | sed "s/,//"';
my $current_lsn = `$current_lsn_cmd`;
chomp($current_lsn);
my $current_byte_offset = psql 'postgres', "SELECT file_offset FROM pg_xlogfile_name_offset('$current_lsn');";

#Get offset of last written record
open my $fh, '<:raw', $primary_wal_file_path;
#Since pg_xlogfile_name_offset does not account for the xlog switch record, we need to add it ourselves
my $xlog_switch_record_len = 32;
seek $fh, $current_byte_offset + $xlog_switch_record_len, 0;
my $bytes_read = "";
my $len_bytes_to_validate = $total_allocated_bytes - $current_byte_offset;
read($fh, $bytes_read, $len_bytes_to_validate);
close $fh;
ok($bytes_read =~ /\A\x00*+\z/, 'make sure wal segment is zeroed');

# The following tests test symlinks. Windows doesn't have symlinks, so
# skip on Windows.
SKIP: {
	# Create a temporary directory in the system location and symlink it
	# to our physical temp location.  That way we can use shorter names
	# for the tablespace directories, which hopefully won't run afoul of
	# the 99 character length limit.
	skip "symlinks not supported on Windows", 9 if ($windows_os);

	my $shorter_tempdir = tempdir_short . "/tempdir";
	symlink "$tempdir", $shorter_tempdir;

	mkdir "$tempdir/tblspc1";
	psql 'postgres', "CREATE TABLESPACE tblspc1 LOCATION '$shorter_tempdir/tblspc1';";
	psql 'postgres', "CREATE TABLE test1 (a int) TABLESPACE tblspc1;";
	command_ok([ 'pg_basebackup', '-D', "$tempdir/tarbackup2", '-Ft',
			   '--target-gp-dbid', '123'],
		'tar format with tablespaces');
	ok(-f "$tempdir/tarbackup2/base.tar", 'backup tar was created');
	my @tblspc_tars = glob "$tempdir/tarbackup2/[0-9]*.tar";
	is(scalar(@tblspc_tars), 1, 'one tablespace tar was created');

	command_fails(
		[ 'pg_basebackup', '-D', "$tempdir/backup1", '-Fp',
		  '--target-gp-dbid', '1'
		],
		'plain format with tablespaces fails without tablespace mapping and target-gp-dbid as the test server dbid');

	command_ok(
		[   'pg_basebackup',    '-D',
			"$tempdir/backup1", '-Fp',
			'--target-gp-dbid', '1',
			"-T$shorter_tempdir/tblspc1=$tempdir/tbackup/tblspc1" ],
		'plain format with tablespaces succeeds with tablespace mapping');
		ok(-d "$tempdir/tbackup/tblspc1/1", 'tablespace was relocated');
	opendir(my $dh, "$tempdir/pgdata/pg_tblspc") or die;
	ok( (   grep
			{
				-l "$tempdir/backup1/pg_tblspc/$_"
				  and readlink "$tempdir/backup1/pg_tblspc/$_" eq
				  "$tempdir/tbackup/tblspc1/1"
			  } readdir($dh)),
		"tablespace symlink was updated");
	closedir $dh;

	mkdir "$tempdir/tbl=spc2";
	psql 'postgres', "DROP TABLE test1;";
	psql 'postgres', "DROP TABLESPACE tblspc1;";
	psql 'postgres', "CREATE TABLESPACE tblspc2 LOCATION '$shorter_tempdir/tbl=spc2';";
	command_ok(
		[   'pg_basebackup',    '-D',
			"$tempdir/backup3", '-Fp',
			'--target-gp-dbid', '123',
			"-T$shorter_tempdir/tbl\\=spc2=$tempdir/tbackup/tbl\\=spc2" ],
		'mapping tablespace with = sign in path');
	ok(-d "$tempdir/tbackup/tbl=spc2", 'tablespace with = sign was relocated');

	psql 'postgres', "DROP TABLESPACE tblspc2;";


	my $twenty_characters = '11111111112222222222';
	my $longer_tempdir = "$tempdir/some_long_directory_path_$twenty_characters$twenty_characters$twenty_characters$twenty_characters$twenty_characters";
	my $some_backup_dir = "$tempdir/backup_dir";
	my $some_other_backup_dir = "$tempdir/other_backup_dir";

	mkdir "$longer_tempdir";
	mkdir "$some_backup_dir";
	psql 'postgres', "CREATE TABLESPACE too_long_tablespace LOCATION '$longer_tempdir';";
	command_warns_like([
		'pg_basebackup',
		'-D', "$some_backup_dir",
		'--target-gp-dbid', '99'],
				 qr/WARNING:  symbolic link ".*" target is too long and will not be added to the backup/,
					   'basebackup with a tablespace that has a very long location should warn target is too long.');

	mkdir "$some_other_backup_dir";
	command_warns_like([
		'pg_basebackup',
		'-D', "$some_other_backup_dir",
		'--target-gp-dbid', '99'],
				 qr/The symbolic link with target ".*" is too long. Symlink targets with length greater than 100 characters would be truncated./,
					   'basebackup with a tablespace that has a very long location should warn link not added to the backup.');

	command_fails_like([
		'ls', "$some_other_backup_dir/pg_tblspc/*"],
				 qr/No such file/,
				 'tablespace directory should be empty');
}

command_fails(
	[ 'pg_basebackup', '-D', "$tempdir/backup_foo", '-Fp', "-T=/foo" ],
	'-T with empty old directory fails');
command_fails(
	[ 'pg_basebackup', '-D', "$tempdir/backup_foo", '-Fp', "-T/foo=" ],
	'-T with empty new directory fails');
command_fails(
	[   'pg_basebackup', '-D', "$tempdir/backup_foo", '-Fp',
		"-T/foo=/bar=/baz" ],
	'-T with multiple = fails');
command_fails(
	[ 'pg_basebackup', '-D', "$tempdir/backup_foo", '-Fp', "-Tfoo=/bar" ],
	'-T with old directory not absolute fails');
command_fails(
	[ 'pg_basebackup', '-D', "$tempdir/backup_foo", '-Fp', "-T/foo=bar" ],
	'-T with new directory not absolute fails');
command_fails(
	[ 'pg_basebackup', '-D', "$tempdir/backup_foo", '-Fp', "-Tfoo" ],
	'-T with invalid format fails');

#
# GPDB: Exclude some files with the --exclude-from option
#

my $exclude_tempdir = "$tempdir/backup_exclude";
my $excludelist = "$tempdir/exclude.list";

mkdir "$exclude_tempdir";
mkdir "$tempdir/pgdata/exclude";

open EXCLUDELIST, ">$excludelist";

# Put a large amount of non-exist patterns in the exclude-from file,
# the pattern matching is efficient enough to handle them.
for my $i (1..1000000) {
	print EXCLUDELIST "./exclude/non_exist.$i\n";
}

# Create some files to exclude
for my $i (1..1000) {
	print EXCLUDELIST "./exclude/$i\n";

	open FILE, ">$tempdir/pgdata/exclude/$i";
	close FILE;
}

# Below file should not be excluded
open FILE, ">$tempdir/pgdata/exclude/keep";
close FILE;

close EXCLUDELIST;

command_ok(
	[	'pg_basebackup',
		'-D', "$exclude_tempdir",
		'--target-gp-dbid', '123',
		'--exclude-from', "$excludelist" ],
	'pg_basebackup runs with exclude-from file');
ok(! -f "$exclude_tempdir/exclude/0", 'excluded files were not created');
ok(-f "$exclude_tempdir/exclude/keep", 'other files were created');

# GPDB: Exclude gpbackup default directory
my $gpbackup_test_dir = "$tempdir/gpbackup_test_dir";
mkdir "$tempdir/pgdata/backups";
TestLib::append_to_file("$tempdir/pgdata/backups/random_backup_file", "some random backup data");

command_ok([ 'pg_basebackup', '-D', $gpbackup_test_dir, '--target-gp-dbid', '123' ],
	'pg_basebackup does not copy over \'backups/\' directory created by gpbackup');
ok(! -d "$gpbackup_test_dir/backups", 'gpbackup default backup directory should be excluded');
rmtree($gpbackup_test_dir);
