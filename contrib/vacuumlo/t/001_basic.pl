use strict;
use warnings;

use PostgreSQL::Test::Utils;
use Test::More;

program_help_ok('vacuumlo');
program_version_ok('vacuumlo');
program_options_handling_ok('vacuumlo');

done_testing();
