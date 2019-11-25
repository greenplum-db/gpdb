#include "libpq-fe.h"

PGconn *connectToFive(void);
PGconn *connectToFiveOnDatabase(char *database_name);
void resetGpdbFiveDataDirectories(void);

PGconn *connectToSix(void);
PGconn *connectToSixOnDatabase(char *database_name);
void resetGpdbSixDataDirectories(void);
