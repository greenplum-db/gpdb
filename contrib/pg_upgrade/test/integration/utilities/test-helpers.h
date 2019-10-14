#include "libpq-fe.h"

PGconn *connectToFive(void);
void resetGpdbFiveDataDirectories(void);
void initializePgUpgradStatus(void);

PGconn *connectToSix(void);
void resetGpdbSixDataDirectories(void);
void resetPgUpgradeStatus(void);
