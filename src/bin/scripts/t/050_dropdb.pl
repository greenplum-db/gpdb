use strict;
use warnings;

use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

program_help_ok('dropdb');
program_version_ok('dropdb');
program_options_handling_ok('dropdb');

my $node = PostgreSQL::Test::Cluster->new('main');
$node->init;
$node->start;

$node->safe_psql('postgres', 'CREATE DATABASE foobar1');
$node->issues_sql_like(
	[ 'dropdb', 'foobar1' ],
	qr/statement: DROP DATABASE foobar1/,
	'SQL DROP DATABASE run');

$node->command_fails([ 'dropdb', 'nonexistent' ],
	'fails with nonexistent database');

done_testing();
