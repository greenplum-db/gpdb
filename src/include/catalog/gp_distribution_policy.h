/*-------------------------------------------------------------------------
 *
 * gp_distribution_policy.h
 *	  definitions for the gp_distribution_policy catalog table
 *
 * Portions Copyright (c) 2005-2011, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_distribution_policy.h
 *
 * NOTES
 *
 *-------------------------------------------------------------------------
 */

#ifndef GP_DISTRIBUTION_POLICY_H
#define GP_DISTRIBUTION_POLICY_H

#include "access/attnum.h"
#include "access/tupdesc.h"
#include "catalog/genbki.h"
#include "catalog/gp_distribution_policy_d.h"
#include "nodes/pg_list.h"

/*
 * Defines for gp_distribution_policy
 */
CATALOG(gp_distribution_policy,7142,GpPolicyRelationId)
{
	Oid			localoid;
	char		policytype; /* distribution policy type */
	int32		numsegments;
#ifdef CATALOG_VARLEN			/* variable-length fields start here */
	int2vector	distkey;		/* column numbers of distribution key cols */
	oidvector	distclass;		/* opclass identifiers */
#endif
} FormData_gp_distribution_policy;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(localoid REFERENCES pg_class(oid));

/* ----------------
 *		Form_gp_distribution_policy corresponds to a pointer to a tuple with
 *		the format of gp_distribution_policy relation.
 * ----------------
 */
typedef FormData_gp_distribution_policy *Form_gp_distribution_policy;

/*
 * Symbolic values for Anum_gp_distribution_policy_type column
 */
#define SYM_POLICYTYPE_PARTITIONED 'p'
#define SYM_POLICYTYPE_REPLICATED 'r'

/*
 * Default set of segments, the value is controlled by the variable
 * gp_create_table_default_numsegments.
 */
#define GP_POLICY_DEFAULT_NUMSEGMENTS()		\
( gp_create_table_default_numsegments == GP_DEFAULT_NUMSEGMENTS_FULL    ? getgpsegmentCount() \
: gp_create_table_default_numsegments == GP_DEFAULT_NUMSEGMENTS_RANDOM  ? (1 + random() % getgpsegmentCount()) \
: gp_create_table_default_numsegments == GP_DEFAULT_NUMSEGMENTS_MINIMAL ? 1 \
: gp_create_table_default_numsegments )

/*
 * The the default numsegments policies when creating a table.
 *
 * - FULL: all the segments;
 * - RANDOM: pick a random set of segments each time;
 * - MINIMAL: the minimal set of segments;
 */
enum
{
	GP_DEFAULT_NUMSEGMENTS_FULL    = -1,
	GP_DEFAULT_NUMSEGMENTS_RANDOM  = -2,
	GP_DEFAULT_NUMSEGMENTS_MINIMAL = -3,
};

/*
 * GpPolicyType represents a type of policy under which a relation's
 * tuples may be assigned to a component database.
 */
typedef enum GpPolicyType
{
	POLICYTYPE_PARTITIONED,		/* Tuples partitioned onto segment database. */
	POLICYTYPE_ENTRY,			/* Tuples stored on entry database. */
	POLICYTYPE_REPLICATED		/* Tuples stored a copy on all segment database. */
} GpPolicyType;

/*
 * GpPolicy represents a Greenplum DB data distribution policy. The ptype field
 * is always significant.  Other fields may be specific to a particular
 * type.
 *
 * A GpPolicy is typically palloc'd with space for nattrs integer
 * attribute numbers (attrs) in addition to sizeof(GpPolicy).
 */
typedef struct GpPolicy
{
	NodeTag		type;
	GpPolicyType ptype;
	int			numsegments;

	/* These fields apply to POLICYTYPE_PARTITIONED. */
	int			nattrs;
	AttrNumber *attrs;		/* array of attribute numbers  */
	Oid		   *opclasses;	/* and their opclasses */
} GpPolicy;

/*
 * Global Variables
 */
extern int	gp_create_table_default_numsegments;

/*
 * GpPolicyCopy -- Return a copy of a GpPolicy object.
 *
 * The copy is palloc'ed in the specified context.
 */
extern GpPolicy *GpPolicyCopy(const GpPolicy *src);

/* GpPolicyEqual
 *
 * A field-by-field comparison just to facilitate comparing IntoClause
 * (which embeds this) in equalFuncs.c
 */
extern bool GpPolicyEqual(const GpPolicy *lft, const GpPolicy *rgt);

extern bool GpPolicyEqualByName(const TupleDesc ltd, const GpPolicy *lpol,
								const TupleDesc rtd, const GpPolicy *rpol);

/*
 * GpPolicyFetch
 *
 * Looks up a given Oid in the gp_distribution_policy table.
 * If found, returns an GpPolicy object (palloc'd in the specified
 * context) containing the info from the gp_distribution_policy row.
 * Else returns NULL.
 *
 * The caller is responsible for passing in a valid relation oid.  This
 * function does not check and assigns a policy of type POLICYTYPE_ENTRY
 * for any oid not found in gp_distribution_policy.
 */
extern GpPolicy *GpPolicyFetch(Oid tbloid);

/*
 * GpPolicyStore: sets the GpPolicy for a table.
 */
extern void GpPolicyStore(Oid tbloid, const GpPolicy *policy);

extern void GpPolicyReplace(Oid tbloid, const GpPolicy *policy);

extern void GpPolicyRemove(Oid tbloid);

extern bool GpPolicyIsRandomPartitioned(const GpPolicy *policy);
extern bool GpPolicyIsHashPartitioned(const GpPolicy *policy);
extern bool GpPolicyIsPartitioned(const GpPolicy *policy);
extern bool GpPolicyIsReplicated(const GpPolicy *policy);
extern bool GpPolicyIsEntry(const GpPolicy *policy);

extern GpPolicy *makeGpPolicy(GpPolicyType ptype, int nattrs, int numsegments);
extern GpPolicy *createReplicatedGpPolicy(int numsegments);
extern GpPolicy *createRandomPartitionedPolicy(int numsegments);
extern GpPolicy *createHashPartitionedPolicy(List *keys, List *opclasses, int numsegments);

extern bool IsReplicatedTable(Oid relid);

#endif			/* GP_DISTRIBUTION_POLICY_H */
