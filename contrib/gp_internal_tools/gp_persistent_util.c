#include "postgres.h"
#include "miscadmin.h"
#include "fmgr.h"
#include "funcapi.h"
#include "catalog/gp_persistent.h"
#include "cdb/cdbpersistentfilesysobj.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

Datum gp_persistent_freelist_rebuild(PG_FUNCTION_ARGS);

Datum
gp_persistent_freelist_rebuild(PG_FUNCTION_ARGS)
{
	/* Must be super user */
	if (!superuser())
		elog(ERROR, "permission denied");
	Oid pt_oid = PG_GETARG_OID(0);
	uint64 numFree = 0;
	switch(pt_oid)
	{
		case GpPersistentRelationNodeRelationId:
			numFree = PersistentFileSysObj_RebuildFreeList(PersistentFsObjType_RelationFile);
			break;
		case GpPersistentDatabaseNodeRelationId:
			numFree = PersistentFileSysObj_RebuildFreeList(PersistentFsObjType_DatabaseDir);
			break;
		case GpPersistentFilespaceNodeRelationId:
			numFree = PersistentFileSysObj_RebuildFreeList(PersistentFsObjType_FilespaceDir);
			break;
		case GpPersistentTablespaceNodeRelationId:
			numFree = PersistentFileSysObj_RebuildFreeList(PersistentFsObjType_TablespaceDir);
			break;
		default:
			elog(ERROR, "invalid persistent table OID specified");
			break;
	}
	PG_RETURN_INT64(numFree);
}
PG_FUNCTION_INFO_V1(gp_persistent_freelist_rebuild);

