/*
 * Copyright (c) 2011 EMC Corporation All Rights Reserved
 *
 * This software is protected, without limitation, by copyright law
 * and international treaties. Use of this software and the intellectual
 * property contained therein is expressly limited to the terms and
 * conditions of the License Agreement under which it is provided by
 * or on behalf of EMC.
 */

/* ---------------------------------------------------------------------
 *
 * Interface to gp_ao_co_diagnostic functions.
 *
 * This file contains functions that are wrappers around their corresponding GP
 * internal functions located in the postgres backend executable. The GP
 * internal functions can not be called directly from outside
 *
 * Internal functions can not be called directly from outside the postrgres
 * backend executable without defining them in the catalog tables. The wrapper
 * functions defined in this file are compiled and linked into an library, which
 * can then be called as a user defined function. The wrapper functions will
 * call the actual internal functions.
 *
 * ---------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"

#include "cdb/cdbvars.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

extern Datum
gp_aoseg_history(PG_FUNCTION_ARGS);

extern Datum
gp_aocsseg(PG_FUNCTION_ARGS);

extern Datum
gp_aoseg(PG_FUNCTION_ARGS);

extern Datum
gp_aocsseg_history(PG_FUNCTION_ARGS);

extern Datum
gp_aoblkdir(PG_FUNCTION_ARGS);

extern Datum
gp_aovisimap(PG_FUNCTION_ARGS);

extern Datum
gp_aovisimap_entry(PG_FUNCTION_ARGS);

extern Datum
gp_aovisimap_hidden_info(PG_FUNCTION_ARGS);

extern Datum
gp_remove_ao_entry_from_cache(PG_FUNCTION_ARGS);

extern Datum
gp_get_ao_entry_from_cache(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_aoseg_history_wrapper);
PG_FUNCTION_INFO_V1(gp_aoseg_wrapper);
PG_FUNCTION_INFO_V1(gp_aocsseg_wrapper);
PG_FUNCTION_INFO_V1(gp_aocsseg_history_wrapper);
PG_FUNCTION_INFO_V1(gp_aoblkdir_wrapper);
PG_FUNCTION_INFO_V1(gp_aovisimap_wrapper);
PG_FUNCTION_INFO_V1(gp_aovisimap_entry_wrapper);
PG_FUNCTION_INFO_V1(gp_aovisimap_hidden_info_wrapper);
PG_FUNCTION_INFO_V1(gp_remove_ao_entry_from_cache);
PG_FUNCTION_INFO_V1(gp_get_ao_entry_from_cache);

extern Datum
gp_aoseg_history_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aoseg_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aocsseg_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aocsseg_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aocsseg_history_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aoblkdir_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aovisimap_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aovisimap_entry_wrapper(PG_FUNCTION_ARGS);
extern Datum
gp_aovisimap_hidden_info_wrapper(PG_FUNCTION_ARGS);

/* ---------------------------------------------------------------------
 * Interface to gp_aoseg_history_wrapper function.
 *
 * The gp_aoseg_history_wrapper function is a wrapper around the gp_aoseg_history
 * function. It can be invoked by creating a function via psql that references it.
 * For example,
 *
 * CREATE FUNCTION get_gp_aoseg_history(oid)
 *   RETURNS TABLE ( gp_tid tid
 *                 , gp_xmin integer
 *                 , gp_xmin_status text
 *                 , gp_xmin_commit_distrib_id text
 *                 , gp_xmax integer
 *                 , gp_xmax_status text
 *                 , gp_xmax_commit_distrib_id text
 *                 , gp_command_id integer
 *                 , gp_infomask text
 *                 , gp_update_tid tid
 *                 , gp_visibility text
 *                 , segno integer
 *                 , tupcount bigint
 *                 , eof bigint
 *                 , eof_uncompressed bigint
 *                 , state smallint
 *                 )
 *   AS '$libdir/gp_ao_co_diagnostics', 'gp_aoseg_history_wrapper' LANGUAGE C STRICT;
 *
 */
Datum
gp_aoseg_history_wrapper(PG_FUNCTION_ARGS)
{
  Datum returnValue = gp_aoseg_history(fcinfo);

  PG_RETURN_DATUM(returnValue);
}


/* ---------------------------------------------------------------------
 * Interface to gp_aocsseg_wrapper function.
 *
 * The gp_aocsseg_wrapper function is a wrapper around the gp_aocsseg function.
 * It can be invoked by creating a function via psql that references it.
 * For example,
 *
 * CREATE FUNCTION get_gp_aocsseg(regclass)
 *   RETURNS TABLE (segment_id integer
 *                 , gp_tid tid
 *                 , segno integer
 *                 , column_num smallint
 *                 , physical_segno integer
 *                 , tupcount bigint
 *                 , eof bigint
 *                 , eof_uncompressed bigint
 *                 , modcount bigint
 *                 , state smallint
 *                 )
 *   AS '$libdir/gp_ao_co_diagnostics', 'gp_aocsseg_wrapper'
 *   LANGUAGE C STRICT
 *   EXECUTE ON ALL SEGMENTS;
 *
 */
Datum
gp_aocsseg_wrapper(PG_FUNCTION_ARGS)
{
  Datum returnValue = gp_aocsseg(fcinfo);

  PG_RETURN_DATUM(returnValue);
}

/* ---------------------------------------------------------------------
 * Interface to gp_aoseg_wrapper function.
 *
 * The gp_wrapper function is a wrapper around the gp_aoseg function.
 * It can be invoked by creating a function via psql that references it.
 * For example,
 *
 * CREATE FUNCTION get_gp_aoseg(regclass)
 *   RETURNS TABLE (segment_id integer
 *                 , segno integer
 *                 , eof bigint
 *                 , tupcount bigint
 *                 , varblockcount bigint
 *                 , eof_uncompressed bigint
 *                 , modcount bigint
 *                 , formatversion smallint
 *                 , state smallint
 *                 )
 *   AS '$libdir/gp_ao_co_diagnostics', 'gp_aoseg_wrapper'
 *   LANGUAGE C STRICT
 *   EXECUTE ON ALL SEGMENTS;
 *
 */
Datum
gp_aoseg_wrapper(PG_FUNCTION_ARGS)
{
  Datum returnValue = gp_aoseg(fcinfo);

  PG_RETURN_DATUM(returnValue);
}

/* ---------------------------------------------------------------------
 * Interface to gp_aocsseg_history_wrapper function.
 *
 * The gp_aocsseg_history_wrapper function is a wrapper around the gp_aocsseg_history
 * function. It can be invoked by creating a function via psql that references it.
 * For example,
 *
 * CREATE FUNCTION get_gp_aocsseg_history(oid)
 *   RETURNS TABLE ( gp_tid tid
 *                 , gp_xmin integer
 *                 , gp_xmin_status text
 *                 , gp_xmin_distrib_id text
 *                 , gp_xmax integer
 *                 , gp_xmax_status text
 *                 , gp_xmax_distrib_id text
 *                 , gp_command_id integer
 *                 , gp_infomask text
 *                 , gp_update_tid tid
 *                 , gp_visibility text
 *                 , segno integer
 *                 , column_num smallint
 *                 , physical_segno integer
 *                 , tupcount bigint
 *                 , eof bigint
 *                 , eof_uncompressed bigint
 *                 , state smallint
 *                 )
 *   AS '$libdir/gp_ao_co_diagnostics', 'gp_aocsseg_history_wrapper' LANGUAGE C STRICT;
 *
 */
Datum
gp_aocsseg_history_wrapper(PG_FUNCTION_ARGS)
{
  Datum returnValue = gp_aocsseg_history(fcinfo);

  PG_RETURN_DATUM(returnValue);
}

/*
 * Interface to gp_aoblkdir_wrapper function.
 *
 * CREATE FUNCTION gp_aoblkdir_wrapper(regclass) RETURNS TABLE
 * (segno integer, columngroup_no integer, first_row_no bigint, file_offset bigint, row_count bigint)
 * AS '$libdir/gp_ao_co_diagnostics.so', 'gp_aoblkdir_wrapper' LANGUAGE C STRICT;
 */
Datum
gp_aoblkdir_wrapper(PG_FUNCTION_ARGS)
{
	Datum returnValue = gp_aoblkdir(fcinfo);

	PG_RETURN_DATUM(returnValue);
}

/* 
 * Interface to gp_aovisimap_wrapper function.
 *
 * CREATE FUNCTION gp_aovisimap(regclass) RETURNS TABLE 
 * (tid tid, segno integer, row_num bigint) 
 * AS '$libdir/gp_ao_co_diagnostics.so', 'gp_aovisimap_wrapper' LANGUAGE C STRICT;
 */
Datum
gp_aovisimap_wrapper(PG_FUNCTION_ARGS)
{
	Datum returnValue = gp_aovisimap(fcinfo);

	PG_RETURN_DATUM(returnValue);
}

/* 
 * Interface to gp_aovisimap_hidden_info_wrapper function.
 *
 * CREATE FUNCTION gp_aovisimap_hidden_info(regclass) RETURNS TABLE 
 * (segno integer, hidden_tupcount bigint, total_tupcount bigint) 
 * AS '$libdir/gp_ao_co_diagnostics.so', 'gp_aovisimap_hidden_info_wrapper' LANGUAGE C STRICT;
 */
Datum
gp_aovisimap_hidden_info_wrapper(PG_FUNCTION_ARGS)
{
	Datum returnValue = gp_aovisimap_hidden_info(fcinfo);

	PG_RETURN_DATUM(returnValue);
}

/* 
 * Interface to gp_aovisimap_entry_wrapper function.
 *
 * CREATE FUNCTION gp_aovisimap_entry(regclass) RETURNS TABLE 
 * (segno integer, hidden_tupcount bigint, total_tupcount bigint) 
 * AS '$libdir/gp_ao_co_diagnostics.so', 'gp_aovisimap_entry_wrapper' LANGUAGE C STRICT;
 */
Datum
gp_aovisimap_entry_wrapper(PG_FUNCTION_ARGS)
{
	Datum returnValue = gp_aovisimap_entry(fcinfo);

	PG_RETURN_DATUM(returnValue);
}
