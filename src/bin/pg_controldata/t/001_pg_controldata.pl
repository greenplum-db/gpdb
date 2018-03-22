use strict;
use warnings;
use PostgresNode;
use TestLib;
use Test::More tests => 13;

program_help_ok('pg_controldata');
program_version_ok('pg_controldata');
program_options_handling_ok('pg_controldata');
command_fails(['pg_controldata'], 'pg_controldata without arguments fails');
command_fails([ 'pg_controldata', 'nonexistent' ],
	'pg_controldata with nonexistent directory fails');

my $node = get_new_demo_node('main');

# GPDB_84_MERGE_FIXME: Seems there already exist demo cluster in
# GPDB, should we init cluster here or just use demo cluster?
#$node->init;

command_like([ 'pg_controldata', $node->name ],
	qr/checkpoint/, 'pg_controldata produces output');
