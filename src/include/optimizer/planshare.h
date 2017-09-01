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

extern List *share_plan(PlannerInfo *root, Plan *common, int numpartners);
extern Cost cost_share_plan(Plan *common, PlannerInfo *root, int numpartners);

extern Plan *prepare_plan_for_sharing(PlannerInfo *root, Plan *common);
extern Plan *share_prepared_plan(PlannerInfo *root, Plan *common);

#endif /* _PLANSHARE_H_ */

