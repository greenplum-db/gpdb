/*
 * plannerconfig.h
 *
 *  Created on: May 19, 2011
 *      Author: siva
 */

#ifndef PLANNERCONFIG_H_
#define PLANNERCONFIG_H_

/**
 * Planning configuration information
 */
typedef struct PlannerConfig
{
	bool		gp_enable_minmax_optimization;
	bool		gp_enable_multiphase_agg;
	bool		gp_enable_direct_dispatch;

	bool		gp_cte_sharing; /* Indicate whether sharing is to be disabled on any CTEs */

	bool		honor_order_by;

	bool		is_under_subplan; /* True for plan rooted at a subquery which is planned as a subplan */

	bool        force_singleQE; /* True means force gather base rel to singleQE  */
	bool        may_rescan; /* true means the subquery may be rescanned. */
} PlannerConfig;

extern PlannerConfig *DefaultPlannerConfig(void);
extern PlannerConfig *CopyPlannerConfig(const PlannerConfig *c1);

#endif /* PLANNERCONFIG_H_ */
