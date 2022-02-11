use strict;
use warnings;
use PostgreSQL::Test::Utils;
use Test::More;

program_help_ok('pg_checksums');
program_version_ok('pg_checksums');
program_options_handling_ok('pg_checksums');

done_testing();
