/*-------------------------------------------------------------------------
 *
 * cdbdef.h
 *	Definitions for use anywhere
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbdef.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDEF_H
#define CDBDEF_H


/*
 * CdbSwap
 *		Exchange the contents of two variables.
 */
#define CdbSwap(_type, _x, _y)	\
	do							\
	{							\
		_type _t = (_x);		\
		_x = (_y);				\
		_y = (_t);				\
	} while (0)


/*
 * CdbVisitOpt
 *      Some tree walkers use these codes to direct the traversal.
 */
typedef enum 
{
    CdbVisit_Walk = 1,          /* proceed in normal sequence */    
    CdbVisit_Skip,              /* no more calls for current node or its kids */
    CdbVisit_Stop,              /* break out of traversal, no more callbacks */
    CdbVisit_Failure,           /* break out of traversal, no more callbacks */
    CdbVisit_Success            /* break out of traversal, no more callbacks */
} CdbVisitOpt;


#endif   /* CDBDEF_H */
