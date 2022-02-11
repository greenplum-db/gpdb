use strict;
use warnings;
use PostgreSQL::Test::Utils;
use Test::More;

program_help_ok('pg_waldump');
program_version_ok('pg_waldump');
program_options_handling_ok('pg_waldump');

done_testing();
