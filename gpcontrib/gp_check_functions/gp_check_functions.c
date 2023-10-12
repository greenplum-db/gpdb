/*-------------------------------------------------------------------------
 *
 * gp_check_functions.c
 *	  GPDB helper functions for checking various system fact/status.
 *
 *
 * Copyright (c) 2022-Present VMware Software, Inc.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "catalog/catalog.h"
#include "utils/builtins.h"

Datum get_tablespace_version_directory_name(PG_FUNCTION_ARGS);

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(get_tablespace_version_directory_name);

/*
 * get the GPDB-specific directory name for user tablespace
 */
Datum
get_tablespace_version_directory_name(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(CStringGetTextDatum(GP_TABLESPACE_VERSION_DIRECTORY));
}

