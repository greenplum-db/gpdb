#include "bdd.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"

void
given(void (*arrangeFunction) (void))
{
	startGpdbFiveCluster();
	arrangeFunction();
	stopGpdbFiveCluster();
}

void
when(void (*actFunction) (void))
{
	actFunction();
}

void
then(void (*assertionFunction) (void))
{
	startGpdbSixCluster();
	assertionFunction();
	stopGpdbSixCluster();
}

void
and(void (*assertionFunction) (void))
{
	/* and has the same behavior as then */
	then(assertionFunction);
}

