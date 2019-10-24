#include "libpq-fe.h"

PGconn *connectTo(int port);
PGresult *executeQuery(PGconn *connection, char *const query);
void executeQueryClearResult(PGconn *connection, char *const query);
