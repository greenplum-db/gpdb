/*-------------------------------------------------------------------------
 *
 * planshare.h
 * 		Prototypes and data structures for plan sharing
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/optimizer/planshare.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef _PLANSHARE_H_
#define _PLANSHARE_H_

#include "nodes/plannodes.h"

extern Plan *prepare_plan_for_sharing(PlannerInfo *root, Plan *common);
extern Plan *share_prepared_plan(PlannerInfo *root, Plan *common);

#endif /* _PLANSHARE_H_ */

