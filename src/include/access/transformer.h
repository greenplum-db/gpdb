/*-------------------------------------------------------------------------
 *
 * transformer.h
 *	  External table data transformer routines.
 *
 * The main purpose of the transformer is to resolve abortion for ETL tasks.
 *
 * A transformer is used to transform a readable external table's unmatched data
 * source into the external table definition data. And handle any possible errors
 * during the transform phase through single row error handler.
 *
 * A simple example is:
 * A CSV file contains 3 columns' data. But the external table is defined 
 * as (a int, b json).
 * Without the transformer, read data from this external table will raise
 * an error.
 *
 * Although an alternative way to do this is to define an external with the
 * same table definition of the data source. And then use SELECT statement
 * to transform data into another table.
 * Like (INSERT INTO t AS SELECT expression(c1), expression(c2) from ext).
 *
 * For ETL tasks, it's not a good solution. Since once the expression raises
 * error, the whole task will be aborted.
 * So for ETL tasks, it can use the transformer to transform data, and errors
 * should be handled by the single row error handler.
 *
 * Portions Copyright (c) 2020-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	    src/include/access/transformer.h
 *
 *-------------------------------------------------------------------------
 */

#include "commands/copy.h"
#include "executor/spi.h"
#include "nodes/pg_list.h"

/*
 * TransformerInfo
 *
 * Usd to transform external table's source data into extertal table defination.
 * 
 * fromoid - retrieve from "transform_from" option. Used to specify the data
 * source's table defination. fromrel is the relation object for this Oid.
 * expression - retrieve from "transform_expression" option. Used to sepcify the
 * transform expressions.
 * arguments - retrieve from "transform_arguments" option. Used to sepcify the args
 * used in expression. The args are integer value reference the fromoid relation's
 * column index.
 * handleerror - handle all error codes in single row error handle, not limit
 * to ERRCODE_DATA_EXCEPTION. User need to aware the risk.
 */
typedef struct TransformerInfo
{
	/* parse from transform_* options */
	Oid			fromoid;
	char	   *expression;
	List	   *arguments;
	bool		handleerror;

	/* for copy and formatter */
	Relation	fromrel;		/* the from template table that used to get data source columns defination */
	TupleDesc	tupledesc;		/* the tuple descriptor gets from fromrel that match the data source columns */
	Datum	   *values;			/* values retrieve from copy and formatter to build data source tuple */
	bool	   *nulls;			/* nulls retrieve from copy and formatter to build data source tuple */
	FmgrInfo   *in_functions; 	/* input functions for formatter to build data source tuple */
	Oid		   *typioparams; 	/* type io params for formatter to build data source tuple */
	Bitmapset  *attrsidx;		/* att index set for the data source template table that builds the data source tuple */

	/* Used for SPI */
	Oid		   *argtypes;		/* arg types to build the SPI plan */
	char	   *query;			/* real query for the SPI plan, should consider sql escape */
	SPIPlanPtr	spiplan;		/* saved SPI plan which transform the data source tuple */
	Datum	   *paramvalues;	/* values to put into the plain, gets from the data source tuple */
	char	   *paramnulls;		/* nulls to put into the plain, gets from the data source tuple */
	int			nexttupleidx;	/* if the plan returns set of trmsformed tuples, mark the returned tuple index. */
} TransformerInfo;

extern TransformerInfo *initTransformerInfo(List *options);
extern void freeTransformerInfo(TransformerInfo *transformer);

extern HeapTuple retrieveTransformedTuple(TransformerInfo *transformer);
extern HeapTuple executeTupleTransform(TransformerInfo *transformer, HeapTuple tuple,
									   CopyState cstate, bool *errskipped /* out */);

