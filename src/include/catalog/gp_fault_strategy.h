/*-------------------------------------------------------------------------
 *
 * gp_fault_strategy.h
 *    FTS fault strategy configuration
 *
 * Portions Copyright (c) 2009-2011, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_fault_strategy.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef _GP_FAULT_STRATEGY_H_
#define _GP_FAULT_STRATEGY_H_

#include "catalog/genbki.h"

/*
 * Defines for gp_fault_strategy table.
 */
#define GpFaultStrategy		"gp_fault_strategy"

/* ----------------
 *		gp_fault_strategy definition.  cpp turns this into
 *		typedef struct FormData_gp_fault_strategy
 * ----------------
 */
#define GpFaultStrategyRelationId	5039

#define GpFaultStrategyMirrorLess		'n'
#ifdef USE_SEGWALREP
#define GpFaultStrategyWalRepMirrored	'w'
#else
#define GpFaultStrategyFileRepMirrored	'f'
#endif

CATALOG(gp_fault_strategy,5039) BKI_SHARED_RELATION BKI_WITHOUT_OIDS
{
	char	fault_strategy;	
} FormData_gp_fault_strategy;

/* no foreign keys */

/* ----------------
 *		Form_gp_fault_strategy corresponds to a pointer to a tuple with
 *		the format of gp_fault_strategy relation.
 * ----------------
 */
typedef FormData_gp_fault_strategy *Form_gp_fault_strategy;


/* ----------------
 *		compiler constants for gp_fault_strategy
 * ----------------
 */
#define Natts_gp_fault_strategy					1
#define Anum_gp_fault_strategy_fault_strategy	1

extern char get_gp_fault_strategy(void);
#ifdef USE_SEGWALREP
extern void update_gp_fault_strategy(char fault_strategy);
#endif

#endif /* _GP_FAULT_STRATEGY_ */
