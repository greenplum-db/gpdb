/*-------------------------------------------------------------------------
 *
 * transform.h
 * 	Query transformation routines
 *
 * Portions Copyright (c) 2011-2013, EMC Corporation
 * Author: Siva
 *-------------------------------------------------------------------------
 */

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "nodes/plannodes.h"
#include "nodes/relation.h"

extern Query *normalize_query(Query *query);

/* preprocess the query for the optimizer */
extern Query *preprocess_query_optimizer(PlannerGlobal *glob, Query *query, ParamListInfo boundParams);
#endif /* TRANSFORM_H */
