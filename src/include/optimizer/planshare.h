/* 
 * planshare.h
 * 		Prototypes and data structures for plan sharing
 * Copyright (c) 2007-2008, Greenplum inc
 */

#ifndef _PLANSHARE_H_
#define _PLANSHARE_H_

#include "nodes/plannodes.h"

extern List *share_plan(PlannerInfo *root, Plan *common, int numpartners);
extern Cost cost_share_plan(Plan *common, PlannerInfo *root, int numpartners);

extern Plan *prepare_plan_for_sharing(PlannerInfo *root, Plan *common);
extern Plan *share_prepared_plan(PlannerInfo *root, Plan *common);

#endif /* _PLANSHARE_H_ */

