/*
 * pxf_fdw.h
 *		  Foreign-data wrapper for PXF (Platform Extension Framework)
 *
 * IDENTIFICATION
 *		  contrib/pxf_fdw/pxf_fdw.h
 */

#include "postgres.h"

#include "access/formatter.h"
#include "commands/copy.h"
#include "nodes/pg_list.h"

#ifndef PXF_FDW_H
#define PXF_FDW_H

#define PXF_FDW_DEFAULT_PROTOCOL "http"
#define PXF_FDW_DEFAULT_HOST     "localhost"
#define PXF_FDW_DEFAULT_PORT     5888

#define GpdbWritableFormatName   "GPDBWritable"
#define TextFormatName           "TEXT"

/*
 * Structure to store the PXF options */
typedef struct PxfOptions
{
	/* PXF service options */
	int			pxf_port;		/* port number for the PXF Service */
	char	   *pxf_host;		/* hostname for the PXF Service */
	char	   *pxf_protocol;	/* protocol for the PXF Service (i.e HTTP or
								 * HTTPS) */

	/* Server doesn't come from options, it is the actual SERVER name */
	char	   *server;			/* the name of the external server */

	/* Defined at options, but it is not visible to FDWs */
	char		exec_location;	/* execute on MASTER, ANY or ALL SEGMENTS,
								 * Greenplum MPP specific */

	/* Single Row Error Handling */
	int			reject_limit;
	bool		is_reject_limit_rows;
	bool		log_errors;

	/* FDW options */
	char	   *protocol;		/* PXF protocol */
	char	   *resource;		/* PXF resource */
	char	   *format;			/* PXF resource format */
	const char *wire_format;	/* undocumented serialization format between
								 * C-client and Java */
	char	   *profile;		/* protocol[:format] */

	List	   *copy_options;	/* merged options for COPY */
	List	   *options;		/* merged options, excluding COPY, protocol,
								 * resource, format, wire_format, pxf_port,
								 * pxf_host, and pxf_protocol */
}			PxfOptions;

/*
 * FDW-specific information for ForeignScanState.fdw_state.
 */
typedef struct PxfFdwExecutionState
{
	CopyState	cstate;			/* state of reading from PXF */
	PxfOptions *options;		/* FDW options */
	ProjectionInfo *proj_info;	/* Projection information */
	List	   *quals;			/* Qualifiers for the query */

	/*
	 * custom data formatter
	 */
	struct custom
	{
		FmgrInfo   *fs_custom_formatter_func;	/* function to convert to
												 * custom format */
		List	   *fs_custom_formatter_params; /* list of defelems that hold
												 * user's format parameters */
		FormatterData *fs_formatter;

		Relation	fs_rd;		/* target relation descriptor */
		FmgrInfo   *in_functions;
		Oid		   *typioparams;

		TupleDesc	fs_tupDesc;

		bool		raw_buf_done;

	}			custom;
}			PxfFdwExecutionState;

/* Functions prototypes for pxf_option.c file */
PxfOptions *PxfGetOptions(Oid foreigntableid);

#endif							/* _PXF_FDW_H_ */
