/*-------------------------------------------------------------------------
 *
 * spi_priv.h
 *				Server Programming Interface private declarations
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/spi_priv.h,v 1.25 2006/03/05 15:58:56 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef SPI_PRIV_H
#define SPI_PRIV_H

#include "executor/spi.h"


#define _SPI_PLAN_MAGIC		569278163

typedef struct
{
	/* current results */
	uint64		processed;		/* by Executor */
	Oid			lastoid;
	SPITupleTable *tuptable;

	MemoryContext procCxt;		/* procedure context */
	MemoryContext execCxt;		/* executor context */
	MemoryContext savedcxt;		/* context of SPI_connect's caller */
	SubTransactionId connectSubid;		/* ID of connecting subtransaction */
} _SPI_connection;

typedef struct _SPI_plan
{
	int			magic;			/* should equal _SPI_PLAN_MAGIC */
	bool		saved;			/* saved or unsaved plan? */
	/* Context containing _SPI_plan itself as well as subsidiary data */
	MemoryContext plancxt;
	int			cursor_options; /* Cursor options used for planning */
	/* Original query string (used for error reporting) */
	const char *query;
	/* List of List of querytrees; one sublist per original parsetree */
	List	   *qtlist;
	/* List of PlannedStmt* --- length == # of querytrees, but flat list */
	List	   *ptlist;
	/* Argument types, if a prepared plan */
	int			nargs;			/* number of plan arguments */
	Oid		   *argtypes;		/* Argument types (NULL if nargs is 0) */

	unsigned long	use_count;
} _SPI_plan;


#define _SPI_CPLAN_CURCXT	0
#define _SPI_CPLAN_PROCXT	1
#define _SPI_CPLAN_TOPCXT	2

#endif   /* SPI_PRIV_H */
