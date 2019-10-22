#include "bdd-helpers.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"

static bdd_step_function current_step;

static void
within_gpdb_five_function(void)
{
	startGpdbFiveCluster();
	current_step();
	stopGpdbFiveCluster();
}

static void
within_gpdb_six_function(void)
{
	startGpdbSixCluster();
	current_step();
	stopGpdbSixCluster();
}

bdd_step_function
withinGpdbFiveCluster(bdd_step_function step)
{
	current_step = step;

	return within_gpdb_five_function;
}

bdd_step_function
withinGpdbSixCluster(bdd_step_function step)
{
	current_step = step;

	return within_gpdb_six_function;
}


