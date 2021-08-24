use strict;
use warnings;
use Config;
use TestLib;
use Test::More tests => 18;

my $tempdir = TestLib::tempdir;
my $tempdir_short = TestLib::tempdir_short;

program_help_ok('pg_ctl');
program_version_ok('pg_ctl');
program_options_handling_ok('pg_ctl');

command_ok([ 'pg_ctl', 'initdb', '-D', "$tempdir/data" ], 'pg_ctl initdb');
command_ok(
	[ $ENV{PG_REGRESS}, '--config-auth',
		"$tempdir/data" ],
	'configure authentication');
open CONF, ">>$tempdir/data/postgresql.conf";
print CONF "fsync = off\n";
if (! $windows_os)
{
	print CONF "listen_addresses = ''\n";
	print CONF "unix_socket_directories = '$tempdir_short'\n";
}
else
{
	print CONF "listen_addresses = '127.0.0.1'\n";
}
close CONF;
command_ok([ 'pg_ctl', 'start', '-D', "$tempdir/data", '-w', '-o', '-c gp_role=utility --gp_dbid=-1 --gp_contentid=-1'],
	'pg_ctl start -w');
# sleep here is because Windows builds can't check postmaster.pid exactly,
# so they may mistake a pre-existing postmaster.pid for one created by the
# postmaster they start.  Waiting more than the 2 seconds slop time allowed
# by test_postmaster_connection prevents that mistake.
sleep 3 if ($windows_os);
command_fails([ 'pg_ctl', 'start', '-D', "$tempdir/data", '-w' ],
	'second pg_ctl start -w fails');
command_ok([ 'pg_ctl', 'stop', '-D', "$tempdir/data", '-w', '-m', 'fast' ],
	'pg_ctl stop -w');
command_fails([ 'pg_ctl', 'stop', '-D', "$tempdir/data", '-w', '-m', 'fast' ],
	'second pg_ctl stop fails');

command_ok([ 'pg_ctl', 'restart', '-D', "$tempdir/data", '-w', '-m', 'fast' ],
	'pg_ctl restart with server not running');
command_ok([ 'pg_ctl', 'restart', '-D', "$tempdir/data", '-w', '-m', 'fast' ],
	'pg_ctl restart with server running');

system_or_bail 'pg_ctl', 'stop', '-D', "$tempdir/data", '-m', 'fast';

# gpdb specific: verify that --wrapper and --wrapper-args work as expected
if (not $windows_os)
{
	my $keypair = 'TESTKEY=hello';
	my $file;

	# launch the server with the env command as a wrapper
	command_ok([ 'pg_ctl', 'start', '-D', "$tempdir/data", '-w',
			'--wrapper=env', "--wrapper-args=$keypair",
			'-o', '-c gp_role=utility --gp_dbid=-1 --gp_contentid=-1' ],
		'pg_ctl start --wrapper');

	# read the pid
	open($file, '<', "$tempdir/data/postmaster.pid");
	my $pid = 0 + <$file>;
	close($file);

	# verify that the envvar is successfully set
	command_ok([ 'grep', '-z', $keypair, "/proc/$pid/environ" ],
		'verify wrapper effect');

	system_or_bail 'pg_ctl', 'stop', '-D', "$tempdir/data", '-m', 'fast';
}
