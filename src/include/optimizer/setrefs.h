/*-------------------------------------------------------------------------
*
* planner.h
*	  prototypes for setrefs.c.
*
*
* src/include/optimizer/setrefs.h
*
*-------------------------------------------------------------------------
*/

#ifndef SETREFS_H
#define SETREFS_H
#include "nodes/plannodes.h"
#include "optimizer/optimizer.h"

void set_upper_references(PlannerInfo *root, Plan *plan, int rtoffset);
#endif /* SETREFS_H */