use strict;
use warnings;
use TestLib;
use Test::More tests => 11;

program_help_ok('droplang');
program_version_ok('droplang');
program_options_handling_ok('droplang');

my $tempdir = tempdir;
start_test_server $tempdir;

# GPDB: drop gp_toolkit before trying to drop plpgsql in tests; otherwise they
# fail because of the dependency.
psql 'postgres', 'DROP SCHEMA gp_toolkit CASCADE';

issues_sql_like(
	[ 'droplang', 'plpgsql', 'postgres' ],
	qr/statement: DROP EXTENSION "plpgsql"/,
	'SQL DROP EXTENSION run');

command_fails(
	[ 'droplang', 'nonexistent', 'postgres' ],
	'fails with nonexistent language');
