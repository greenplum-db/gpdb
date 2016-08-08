#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"

#include "storage/lmgr.h"
#include "storage/lock.h"
#include "storage/proc.h"
#include "utils/resowner.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(lockrelease);

Datum lockrelease(PG_FUNCTION_ARGS);

static bool
stillHolding(Oid relid, LOCKMODE lmode)
{
	LockData   *lockData;
	PROCLOCK   *proclock;
	LOCK	   *lock;
	PGPROC	   *proc;
	int			i;

	lockData = GetLockStatusData();

	for (i = 0; i < lockData->nelements; i++)
	{
		proclock = &(lockData->proclocks[i]);
		lock = &(lockData->locks[i]);
		proc = &(lockData->procs[i]);

		if (proc->pid != MyProcPid)
			continue;

		if (lock->tag.locktag_type != LOCKTAG_RELATION)
			continue;

		if (lock->tag.locktag_field1 != MyDatabaseId)
			continue;

		if (lock->tag.locktag_field2 != relid)
			continue;

		if (!(proclock->holdMask & LOCKBIT_ON(lmode)))
			continue;

		return true;
	}

	return false;
}

Datum
lockrelease(PG_FUNCTION_ARGS)
{
	Oid			relid = PG_GETARG_OID(0);
	LOCKMODE	lmode = PG_GETARG_INT32(1);
	ResourceOwner saveResourceOwner;


	saveResourceOwner = CurrentResourceOwner;
	CurrentResourceOwner = TopTransactionResourceOwner;
	while (stillHolding(relid, lmode))
	{
		LOCKTAG		tag;

		SET_LOCKTAG_RELATION(tag, MyDatabaseId, relid);
		if (!LockRelease(&tag, lmode, false))
		{
			break;
		}
	}
	CurrentResourceOwner = saveResourceOwner;

	PG_RETURN_NULL();
}
