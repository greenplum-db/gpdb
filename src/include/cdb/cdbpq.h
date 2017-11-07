#ifndef _CDBPQ_H_
#define _CDBPQ_H_

#include "postgres_ext.h"
#include "libpq-int.h"

/* Interface for dispatching stmts from QD to qExecs.
 *
 * See cdbtm.h for valid flag values.
 */
extern int PQsendGpQuery_shared(PGconn       *conn,
								 char         *query,
								 int          query_len,
								 pqbool         nonblock);

#endif
