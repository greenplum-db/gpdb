
#include "postgres.h"
#include "storage/shmem.h"
#include "storage/s_lock.h"

slock_t    *ShmemLock;			/* spinlock for shared memory and LWLock
								 * allocation */

void InitShmemAccess(void *seghdr)
{
    elog(ERROR, (errmsg("NOT IMPLEMENTED")));
}

void InitShmemAllocation(void)
{
    elog(ERROR, (errmsg("NOT IMPLEMENTED")));
}

void *ShmemAlloc(Size size)
{
    void * result = malloc(size);
    return result;
}

bool ShmemAddrIsValid(const void *addr)
{
    return false;
}

void InitShmemIndex(void)
{
    elog(ERROR, (errmsg("NOT IMPLEMENTED")));
}

HTAB *ShmemInitHash(const char *name, long init_size, long max_size, HASHCTL *infoP, int hash_flags)
{
    elog(ERROR, (errmsg("NOT IMPLEMENTED")));
    pg_unreachable();
}

void *ShmemInitStruct(const char *name, Size size, bool *foundPtr)
{
    elog(ERROR, (errmsg("NOT IMPLEMENTED")));
    pg_unreachable();
}

Size add_size(Size s1, Size s2)
{
    return s1 + s2;
}

Size mul_size(Size s1, Size s2)
{
    if (s1 == 0 || s2 == 0)
        return 0;

    return s1 * s2;
}