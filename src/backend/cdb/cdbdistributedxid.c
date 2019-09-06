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
#include "utils/builtins.h"
#include "cdb/cdbtm.h"

Datum		gp_distributed_xid(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_distributed_xid);
Datum
gp_distributed_xid(PG_FUNCTION_ARGS pg_attribute_unused())
{
	DistributedTransactionId xid = getDistributedTransactionId();

	PG_RETURN_XID(xid);

}
