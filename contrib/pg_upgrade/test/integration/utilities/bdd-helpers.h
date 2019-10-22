#ifndef PG_UPGRADE_BDD_HELPERS_H
#define PG_UPGRADE_BDD_HELPERS_H

#include "bdd-library/bdd.h"

bdd_step_function withinGpdbFiveCluster(bdd_step_function step);
bdd_step_function withinGpdbSixCluster(bdd_step_function step);

#endif /* PG_UPGRADE_BDD_HELPERS_H */
