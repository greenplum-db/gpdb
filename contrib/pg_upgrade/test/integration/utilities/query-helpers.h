#include "libpq-fe.h"

PGconn *connectTo(int port);
PGconn *connectToFive(void);
PGconn *connectToSix(void);
PGresult *executeQuery(PGconn *connection, char *const query);
