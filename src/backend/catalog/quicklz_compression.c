/*---------------------------------------------------------------------
 *
 * quicklz_compression.c
 *
 * The quicklz implementation is not provided due to licensing issues.
 * The following stub implementation is built if a proprietary
 * implementation is not provided.
 *
 * Note that other compression algorithms actually work better for new
 * installations anyway.
 *
 * Portions Copyright (c) 2011 EMC Corporation All Rights Reserved
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/catalog/quicklz_compression.c
 *
 *---------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/builtins.h"

Datum
quicklz_constructor(PG_FUNCTION_ARGS)
{
	elog(ERROR, "quicklz compression not supported");
	PG_RETURN_VOID();
}

Datum
quicklz_destructor(PG_FUNCTION_ARGS)
{
	elog(ERROR, "quicklz compression not supported");
	PG_RETURN_VOID();
}

Datum
quicklz_compress(PG_FUNCTION_ARGS)
{
	elog(ERROR, "quicklz compression not supported");
	PG_RETURN_VOID();
}

Datum
quicklz_decompress(PG_FUNCTION_ARGS)
{
	elog(ERROR, "quicklz compression not supported");
	PG_RETURN_VOID();
}

Datum
quicklz_validator(PG_FUNCTION_ARGS)
{
	elog(ERROR, "quicklz compression not supported");
	PG_RETURN_VOID();
}


