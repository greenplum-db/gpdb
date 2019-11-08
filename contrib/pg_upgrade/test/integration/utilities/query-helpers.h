#include "libpq-fe.h"

PGconn *connectTo(int port);
PGresult *executeQuery(PGconn *connection, const char *query);
void executeQueryClearResult(PGconn *connection, const char *query);
