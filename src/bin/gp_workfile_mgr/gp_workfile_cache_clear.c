/*
 * Copyright (c) 2012 EMC Corporation All Rights Reserved
 *
 * This software is protected, without limitation, by copyright law
 * and international treaties. Use of this software and the intellectual
 * property contained therein is expressly limited to the terms and
 * conditions of the License Agreement under which it is provided by
 * or on behalf of EMC.
 */

#include "postgres.h"
#include "utils/workfile_mgr.h"

Datum gp_workfile_mgr_reset_segspace(PG_FUNCTION_ARGS);

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(gp_workfile_mgr_reset_segspace);

/*
 * gp_workfile_mgr_reset_segspace
 *    Function to reset the used segspace on a segment
 *    This directly manipulates the segspace counter and
 *    should be used for testing purposes only
 *  Returns the size before the reset
 */
Datum
gp_workfile_mgr_reset_segspace(PG_FUNCTION_ARGS)
{
	int64 size = WorkfileSegspace_GetSize();
	WorkfileSegspace_Commit(0, size);
	return Int64GetDatum(size);
}
