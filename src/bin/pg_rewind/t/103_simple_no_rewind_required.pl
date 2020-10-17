use strict;
use warnings;
use TestLib;
use Test::More tests => 3;

use FindBin;
use lib $FindBin::RealBin;

use RewindTest;

sub run_test
{
	my $test_mode = shift;

	RewindTest::setup_cluster($test_mode);
	RewindTest::start_master();
	RewindTest::create_standby($test_mode);

	master_psql("CHECKPOINT");
	# ask to stop master before promoting standby, to make sure no wal
	# is written
	RewindTest::promote_standby(1);
	RewindTest::run_pg_rewind($test_mode);

	RewindTest::clean_rewind_test();
	return;
}

# Run the test in both modes
run_test('local');
run_test('remote');

exit(0);
