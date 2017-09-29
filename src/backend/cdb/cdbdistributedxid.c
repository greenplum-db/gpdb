/*-------------------------------------------------------------------------
 *
 * cdbdistributedxid.c
 *		Function to return maximum distributed transaction id.
 *
 * IDENTIFICATION
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "access/heapam.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbtm.h"

Datum		gp_distributed_xid(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_distributed_xid);
Datum
gp_distributed_xid(PG_FUNCTION_ARGS __attribute__((unused)))
{
	DistributedTransactionId xid = getDistributedTransactionId();

	PG_RETURN_XID(xid);

}
