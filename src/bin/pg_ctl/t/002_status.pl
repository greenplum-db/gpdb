use strict;
use warnings;

use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

my $tempdir       = PostgreSQL::Test::Utils::tempdir;
my $tempdir_short = PostgreSQL::Test::Utils::tempdir_short;

command_exit_is([ 'pg_ctl', 'status', '-D', "$tempdir/nonexistent" ],
	4, 'pg_ctl status with nonexistent directory');

my $node = PostgreSQL::Test::Cluster->new('main');
$node->init;

command_exit_is([ 'pg_ctl', 'status', '-D', $node->data_dir ],
	3, 'pg_ctl status with server not running');

system_or_bail 'pg_ctl', '-l', "$tempdir/logfile", '-D',
  $node->data_dir, '-w', 'start', '-o', '-c gp_role=utility --gp_dbid=-1 --gp_contentid=-1';
command_exit_is([ 'pg_ctl', 'status', '-D', $node->data_dir ],
	0, 'pg_ctl status with server running');

system_or_bail 'pg_ctl', 'stop', '-D', $node->data_dir;

done_testing();
