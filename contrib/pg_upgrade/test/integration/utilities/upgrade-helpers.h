
#ifndef PG_UPGRADE_INTEGRATION_TEST_UPGRADE_HELPERS
#define PG_UPGRADE_INTEGRATION_TEST_UPGRADE_HELPERS

#include "postgres_fe.h"
#include "pqexpbuffer.h"

void		performUpgrade(void);
void		performUpgradeCheck();

extern PQExpBufferData pg_upgrade_output;
extern int pg_upgrade_exit_status;

#endif							/* PG_UPGRADE_INTEGRATION_TEST_UPGRADE_HELPERS */
