use strict;
use warnings;
use TestLib;
use Test::More tests => 2;

my $tempdir = TestLib::tempdir;
my $tempdir_short = TestLib::tempdir_short;

standard_initdb "$tempdir/data";

command_exit_is([ 'pg_ctl', 'status', '-D', "$tempdir/data" ],
	3, 'pg_ctl status with server not running');

system_or_bail 'pg_ctl', '-s', '-l', "$tempdir/logfile", '-D',
  "$tempdir/data", '-w', 'start', '-o', '-c gp_role=utility --gp_dbid=-1 --gp_contentid=-1';

command_exit_is([ 'pg_ctl', 'status', '-D', "$tempdir/data" ],
	0, 'pg_ctl status with server running');

system_or_bail 'pg_ctl', 'stop', '-D', "$tempdir/data", '-m', 'fast';
