use strict;
use warnings;

use PostgreSQL::Test::Utils;
use Test::More;

#########################################
# Basic checks

program_help_ok('oid2name');
program_version_ok('oid2name');
program_options_handling_ok('oid2name');

done_testing();
