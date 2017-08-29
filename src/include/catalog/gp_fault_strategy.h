/*-------------------------------------------------------------------------
 *
 * gp_fault_strategy.h
 *    FTS fault strategy configuration
 *
 * Copyright (c) 2009-2011, Greenplum Inc.
 *
 *-------------------------------------------------------------------------
 */
#ifndef _GP_FAULT_STRATEGY_H_
#define _GP_FAULT_STRATEGY_H_

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
#define GpFaultStrategyFileRepMirrorred	'f'


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

#endif /* _GP_FAULT_STRATEGY_ */
