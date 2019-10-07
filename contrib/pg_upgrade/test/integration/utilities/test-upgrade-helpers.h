
#ifndef PG_UPGRADE_INTEGRATION_TEST_UPGRADE_HELPERS
#define PG_UPGRADE_INTEGRATION_TEST_UPGRADE_HELPERS

#include "postgres_fe.h"

void		performUpgrade(void);
void        performUpgradeWithTablespaces(char *mappingFilePath);
void		performUpgradeCheck(void);
void		initializePgUpgradeStatus(void);
void		resetPgUpgradeStatus(void);

char	   *upgradeCheckOutput(void);
int			upgradeCheckStatus(void);

#endif							/* PG_UPGRADE_INTEGRATION_TEST_UPGRADE_HELPERS */
