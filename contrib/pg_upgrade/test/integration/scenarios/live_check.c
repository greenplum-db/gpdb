#include "cmockery_gp.h"

#include "bdd-library/bdd.h"

#include "live_check.h"
#include "utilities/gpdb5-cluster.h"
#include "utilities/test-upgrade-helpers.h"

static void
assert_string_does_not_contain(char *unexpected_fragment, char *actual)
{
	char *found = strstr(actual, unexpected_fragment);
	
	if (found != NULL)
		fail_msg("expected to not find \"%s\" in \"%s\", but did find \"%s\".\n\n", 
			unexpected_fragment, 
			actual, 
			unexpected_fragment);
	else
		assert_true(found == NULL);
}

static void 
theGpdbFiveServerIsStillRunning(void)
{
	startGpdbFiveCluster();
}

static void
theAdministratorRunsChecks(void)
{
	performUpgradeCheck();
}

static void
noWarningMessagesShouldBeOutputThatTheServerIsStillRunning(void)
{
	assert_string_does_not_contain("*failure*", upgradeCheckOutput());
	assert_true(upgradeReturnedSuccess());
}

void
test_a_live_check_with_the_old_server_still_up_does_not_throw_error_message(void **state)
{
	given(theGpdbFiveServerIsStillRunning);
	when(theAdministratorRunsChecks);
	then(noWarningMessagesShouldBeOutputThatTheServerIsStillRunning);
}