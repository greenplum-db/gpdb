/*-------------------------------------------------------------------------
 *
 * formatter.h
 *	  Declarations for External Table Formatter functions
 *
 * Portions Copyright (c) 2010, EMC corporation
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/formatter.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef FORMATTER_H
#define FORMATTER_H

#include "access/htup.h"
#include "access/tupdesc.h"
#include "lib/stringinfo.h"
#include "mb/pg_wchar.h"
#include "nodes/nodes.h"
#include "nodes/value.h"
#include "utils/rel.h"

typedef enum FmtNotification
{
	FMT_NONE,
	FMT_NEED_MORE_DATA

} FmtNotification;

/*
 * FormatterData is the node type that is passed as fmgr "context" info
 * when a function is called by the External Table Formatter manager.
 */

typedef struct FormatterData
{
	NodeTag			type;                 /* see T_FormatterData */
	
	/* metadata */
	Relation    	fmt_relation;
	TupleDesc   	fmt_tupDesc;
	List		   *fmt_args;
	FmtNotification fmt_notification;
	HeapTuple		fmt_tuple; /* hack! pass back the tuple through here for now... */

	/* formatting */
	StringInfoData	fmt_databuf;
	bool			fmt_saw_eof;
	FmgrInfo   	   *fmt_conv_funcs; /* in_functions (RET) or out_functions (WET) */
	Oid            *fmt_typioparams;
	MemoryContext	fmt_perrow_ctx;
	void		   *fmt_user_ctx;
	
	/* sreh */
	int				fmt_badrow_num;
	int				fmt_badrow_len;
	char		   *fmt_badrow_data;
	int             fmt_bytesread;
	
	/* encoding */
	bool			fmt_needs_transcoding;
	FmgrInfo*		fmt_conversion_proc;
	int				fmt_external_encoding;
		
} FormatterData;

#define CALLED_AS_FORMATTER(fcinfo) \
	((fcinfo->context != NULL && IsA((fcinfo)->context, FormatterData)))

#define FORMATTER_GET_TUPDESC(fcinfo)         (((FormatterData*) fcinfo->context)->fmt_tupDesc)
#define FORMATTER_GET_RELATION(fcinfo)        (((FormatterData*) fcinfo->context)->fmt_relation)
#define FORMATTER_GET_DATABUF(fcinfo)         (((FormatterData*) fcinfo->context)->fmt_databuf.data)
#define FORMATTER_GET_DATALEN(fcinfo)         (((FormatterData*) fcinfo->context)->fmt_databuf.len)
#define FORMATTER_GET_DATACURSOR(fcinfo)      (((FormatterData*) fcinfo->context)->fmt_databuf.cursor)
#define FORMATTER_GET_SAW_EOF(fcinfo)	      (((FormatterData*) fcinfo->context)->fmt_saw_eof)
#define FORMATTER_GET_USER_CTX(fcinfo)        (((FormatterData*) fcinfo->context)->fmt_user_ctx)
#define FORMATTER_GET_PER_ROW_MEM_CTX(fcinfo) (((FormatterData*) fcinfo->context)->fmt_perrow_ctx)
#define FORMATTER_GET_CONVERSION_FUNCS(fcinfo) (((FormatterData*) fcinfo->context)->fmt_conv_funcs)
#define FORMATTER_GET_TYPIOPARAMS(fcinfo)     (((FormatterData*) fcinfo->context)->fmt_typioparams)
#define FORMATTER_GET_ARG_LIST(fcinfo)	      (((FormatterData*) fcinfo->context)->fmt_args)
#define FORMATTER_GET_NUM_ARGS(fcinfo)	      (list_length(FORMATTER_GET_ARG_LIST(fcinfo)))
#define FORMATTER_GET_NTH_ARG_KEY(fcinfo, n)  (((DefElem *)(list_nth(FORMATTER_GET_ARG_LIST(fcinfo),(n - 1))))->defname)
#define FORMATTER_GET_NTH_ARG_VAL(fcinfo, n)  (((Value *)((DefElem *)(list_nth(FORMATTER_GET_ARG_LIST(fcinfo),(n - 1))))->arg)->val.str)
#define FORMATTER_GET_EXTENCODING(fcinfo)     (((FormatterData*) fcinfo->context)->fmt_external_encoding)

#define FORMATTER_SET_USER_CTX(fcinfo, p) \
	(((FormatterData*) fcinfo->context)->fmt_user_ctx = p)

#define FORMATTER_SET_BAD_ROW_NUM(fcinfo, n) \
	(((FormatterData*) fcinfo->context)->fmt_badrow_num = n)

#define FORMATTER_SET_BAD_ROW_DATA(fcinfo, p, n) \
	do { \
		((FormatterData*) fcinfo->context)->fmt_badrow_len = n; \
		((FormatterData*) fcinfo->context)->fmt_badrow_data = p; \
	} while (0)
		
#define FORMATTER_SET_DATACURSOR(fcinfo, n) \
	((((FormatterData*) fcinfo->context)->fmt_databuf.cursor) = n)

#define FORMATTER_SET_TUPLE(fcinfo, t) \
	(((FormatterData*) fcinfo->context)->fmt_tuple = t)

#define FORMATTER_SET_BYTE_NUMBER(fcinfo, n) \
	((((FormatterData*) fcinfo->context)->fmt_bytesread) += n)

#define FORMATTER_RETURN_TUPLE(tuple) \
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple))
	
#define FORMATTER_RETURN_NOTIFICATION(fcinfo, n) \
	do { \
		(((FormatterData*) fcinfo->context)->fmt_notification = n); \
		PG_RETURN_NULL(); \
	} while (0)

#define FORMATTER_ENCODE_STRING(fcinfo, p, n, cvt, is_import) \
	do { \
		if (((FormatterData*) fcinfo->context)->fmt_needs_transcoding) \
		{\
			if(is_import) \
				cvt = pg_custom_to_server(p, \
										n, \
										((FormatterData*) fcinfo->context)->fmt_external_encoding, \
										((FormatterData*) fcinfo->context)->fmt_conversion_proc); \
			else \
				cvt = pg_server_to_custom(p, \
										n, \
										((FormatterData*) fcinfo->context)->fmt_external_encoding, \
										((FormatterData*) fcinfo->context)->fmt_conversion_proc); \
		}\
	} while (0)


#endif /* FORMATTER_H */
