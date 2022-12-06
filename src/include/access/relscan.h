/*-------------------------------------------------------------------------
 *
 * relscan.h
 *	  POSTGRES relation scan descriptor definitions.
 *
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/relscan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef RELSCAN_H
#define RELSCAN_H

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/itup.h"
#include "access/tupdesc.h"

#include "access/formatter.h"

typedef struct HeapScanDescData
{
	/* scan parameters */
	Relation	rs_rd;			/* heap relation descriptor */
	Snapshot	rs_snapshot;	/* snapshot to see */
	int			rs_nkeys;		/* number of scan keys */
	ScanKey		rs_key;			/* array of scan key descriptors */
	bool		rs_bitmapscan;	/* true if this is really a bitmap scan */
	bool		rs_pageatatime; /* verify visibility page-at-a-time? */
	bool		rs_allow_strat; /* allow or disallow use of access strategy */
	bool		rs_allow_sync;	/* allow or disallow use of syncscan */
	bool		rs_temp_snap;	/* unregister snapshot at scan end? */

	/* state set up at initscan time */
	BlockNumber rs_nblocks;		/* number of blocks to scan */
	BlockNumber rs_startblock;	/* block # to start at */
	BufferAccessStrategy rs_strategy;	/* access strategy for reads */
	bool		rs_syncscan;	/* report location to syncscan logic? */

	/* scan current state */
	bool		rs_inited;		/* false = scan not init'd yet */
	HeapTupleData rs_ctup;		/* current tuple in scan, if any */
	BlockNumber rs_cblock;		/* current block # in scan, if any */
	Buffer		rs_cbuf;		/* current buffer in scan, if any */
	/* NB: if rs_cbuf is not InvalidBuffer, we hold a pin on that buffer */

	/* these fields only used in page-at-a-time mode and for bitmap scans */
	int			rs_cindex;		/* current tuple's index in vistuples */
	int			rs_ntuples;		/* number of visible tuples on page */
	OffsetNumber rs_vistuples[MaxHeapTuplesPerPage];	/* their offsets */
}	HeapScanDescData;

/*
 * We use the same IndexScanDescData structure for both amgettuple-based
 * and amgetbitmap-based index scans.  Some fields are only relevant in
 * amgettuple-based scans.
 */
typedef struct IndexScanDescData
{
	/* scan parameters */
	Relation	heapRelation;	/* heap relation descriptor, or NULL */
	Relation	indexRelation;	/* index relation descriptor */
	Snapshot	xs_snapshot;	/* snapshot to see */
	int			numberOfKeys;	/* number of index qualifier conditions */
	int			numberOfOrderBys;		/* number of ordering operators */
	ScanKey		keyData;		/* array of index qualifier descriptors */
	ScanKey		orderByData;	/* array of ordering op descriptors */
	bool		xs_want_itup;	/* caller requests index tuples */

	/* signaling to index AM about killing index tuples */
	bool		kill_prior_tuple;		/* last-returned tuple is dead */
	bool		ignore_killed_tuples;	/* do not return killed entries */
	bool		xactStartedInRecovery;	/* prevents killing/seeing killed
										 * tuples */

	/* index access method's private state */
	void	   *opaque;			/* access-method-specific info */

	/* in an index-only scan, this is valid after a successful amgettuple */
	IndexTuple	xs_itup;		/* index tuple returned by AM */
	TupleDesc	xs_itupdesc;	/* rowtype descriptor of xs_itup */

	/* xs_ctup/xs_cbuf/xs_recheck are valid after a successful index_getnext */
	HeapTupleData xs_ctup;		/* current heap tuple, if any */
	Buffer		xs_cbuf;		/* current heap buffer in scan, if any */
	/* NB: if xs_cbuf is not InvalidBuffer, we hold a pin on that buffer */
	bool		xs_recheck;		/* T means scan keys must be rechecked */

	/* state data for traversing HOT chains in index_getnext */
	bool		xs_continue_hot;	/* T if must keep walking HOT chain */
}	IndexScanDescData;

/*
 * used for scan of external relations with the file protocol
 */
typedef struct FileScanDescData
{
	/* scan parameters */
	Relation	fs_rd;			/* target relation descriptor */
	struct URL_FILE *fs_file;	/* the file pointer to our URI */
	char	   *fs_uri;			/* the URI string */
	bool		fs_noop;		/* no op. this segdb has no file to scan */
	uint32      fs_scancounter;	/* copied from struct ExternalScan in plan */
	
	/* current file parse state */
	struct CopyStateData *fs_pstate;

	Form_pg_attribute *attr;
	AttrNumber	num_phys_attrs;
	Datum	   *values;
	bool	   *nulls;
	FmgrInfo   *in_functions;
	Oid		   *typioparams;
	Oid			in_func_oid;
	
	/* current file scan state */
	bool		fs_inited;		/* false = scan not init'd yet */
	TupleDesc	fs_tupDesc;
	HeapTupleData fs_ctup;		/* current tuple in scan, if any */

	/* custom data formatter */
	FmgrInfo   *fs_custom_formatter_func; /* function to convert to custom format */
	List	   *fs_custom_formatter_params; /* list of defelems that hold user's format parameters */
	FormatterData *fs_formatter;

	/* external partition */
	bool		fs_hasConstraints;
	List		**fs_constraintExprs;	
}	FileScanDescData;

typedef FileScanDescData *FileScanDesc;

/* Struct for heap-or-index scans of system tables */
typedef struct SysScanDescData
{
	Relation	heap_rel;		/* catalog being scanned */
	Relation	irel;			/* NULL if doing heap scan */
	HeapScanDesc scan;			/* only valid in heap-scan case */
	IndexScanDesc iscan;		/* only valid in index-scan case */
	Snapshot	snapshot;		/* snapshot to unregister at end of scan */
}	SysScanDescData;

#endif   /* RELSCAN_H */
