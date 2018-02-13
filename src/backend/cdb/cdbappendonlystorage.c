/*-------------------------------------------------------------------------
 *
 * cdbappendonlystorage.c
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbappendonlystorage.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "storage/gp_compress.h"
#include "cdb/cdbappendonlystorage_int.h"
#include "cdb/cdbappendonlystorage.h"
#include "utils/guc.h"

int32 AppendOnlyStorage_GetUsableBlockSize(int32 configBlockSize)
{
	int32 result;

	if (configBlockSize > AOSmallContentHeader_MaxLength)
		result = AOSmallContentHeader_MaxLength;
	else
		result = configBlockSize;

	/*
	 * Round down to 32-bit boundary.
	 */
	result = (result / sizeof(uint32)) * sizeof(uint32);
	
	return result;
}
