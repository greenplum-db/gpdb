/*-------------------------------------------------------------------------
 *
 * var.h
 *	  prototypes for optimizer/util/var.c.
 *
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/optimizer/var.h,v 1.41 2009/04/19 19:46:33 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef VAR_H
#define VAR_H

#include "nodes/relation.h"

typedef enum
{
	PVC_REJECT_AGGREGATES,		/* throw error if Aggref found */
	PVC_INCLUDE_AGGREGATES,		/* include Aggrefs in output list */
	PVC_RECURSE_AGGREGATES		/* recurse into Aggref arguments */
} PVCAggregateBehavior;

typedef enum
{
	PVC_REJECT_PLACEHOLDERS,	/* throw error if PlaceHolderVar found */
	PVC_INCLUDE_PLACEHOLDERS,	/* include PlaceHolderVars in output list */
	PVC_RECURSE_PLACEHOLDERS	/* recurse into PlaceHolderVar arguments */
} PVCPlaceHolderBehavior;

typedef bool (*Cdb_walk_vars_callback_Aggref)(Aggref *aggref, void *context, int sublevelsup);
typedef bool (*Cdb_walk_vars_callback_Var)(Var *var, void *context, int sublevelsup);
typedef bool (*Cdb_walk_vars_callback_CurrentOf)(CurrentOfExpr *expr, void *context, int sublevelsup);
typedef bool (*Cdb_walk_vars_callback_placeholdervar)(PlaceHolderVar *expr, void *context, int sublevelsup);
bool        cdb_walk_vars(Node                         *node,
                          Cdb_walk_vars_callback_Var    callback_var,
                          Cdb_walk_vars_callback_Aggref callback_aggref,
                          Cdb_walk_vars_callback_CurrentOf callback_currentof,
						  Cdb_walk_vars_callback_placeholdervar callback_placeholdervar,
                          void                         *context,
                          int                           levelsup);

extern Relids pull_varnos(Node *node);

extern void pull_varattnos(Node *node, Bitmapset **varattnos);
extern bool contain_ctid_var_reference(Scan *scan);
extern bool contain_var_clause(Node *node);
extern bool contain_vars_of_level(Node *node, int levelsup);
extern bool contain_vars_of_level_or_above(Node *node, int levelsup);
extern int	locate_var_of_level(Node *node, int levelsup);
extern int	locate_var_of_relation(Node *node, int relid, int levelsup);
extern List *pull_var_clause(Node *node, PVCAggregateBehavior aggbehavior,
				PVCPlaceHolderBehavior phbehavior);
extern Node *flatten_join_alias_vars(PlannerInfo *root, Node *node);
bool contain_vars_of_level_or_above_cbPlaceHolderVar(PlaceHolderVar *placeholdervar, void *unused, int sublevelsup);

#endif   /* VAR_H */
