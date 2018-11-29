/*-------------------------------------------------------------------------
 *
 * gp_policy.h
 *	  definitions for the gp_distribution_policy catalog table
 *
 * Portions Copyright (c) 2005-2011, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/catalog/gp_policy.h
 *
 * NOTES
 *
 *-------------------------------------------------------------------------
 */

#ifndef _GP_POLICY_H_
#define _GP_POLICY_H_

#include "access/attnum.h"
#include "catalog/genbki.h"
#include "nodes/pg_list.h"

/*
 * Defines for gp_policy
 */
#define GpPolicyRelationId  5002

CATALOG(gp_distribution_policy,5002) BKI_WITHOUT_OIDS
{
	Oid			localoid;
	int16		attrnums[1];
	char		policytype; /* distribution policy type */
	int32		numsegments;
} FormData_gp_policy;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(localoid REFERENCES pg_class(oid));

#define Natts_gp_policy		4
#define Anum_gp_policy_localoid	1
#define Anum_gp_policy_attrnums	2
#define Anum_gp_policy_type	3
#define Anum_gp_policy_numsegments	4

/*
 * Symbolic values for Anum_gp_policy_type column
 */
#define SYM_POLICYTYPE_PARTITIONED 'p'
#define SYM_POLICYTYPE_REPLICATED 'r'

/*
 * A magic number, setting GpPolicy.numsegments to this value will cause a
 * failed assertion at runtime, which allows developers to debug with gdb.
 */
#define __GP_POLICY_EVIL_NUMSEGMENTS		(666)

/*
 * All the segments.  getgpsegmentCount() should not be used directly as it
 * will return 0 in utility mode, but a valid numsegments should always be
 * greater than 0.
 */
#define GP_POLICY_ALL_NUMSEGMENTS			Max(1, getgpsegmentCount())

/*
 * Minimal set of segments.
 */
#define GP_POLICY_MINIMAL_NUMSEGMENTS		1

/*
 * A random set of segments, the value is different on each call.
 */
#define GP_POLICY_RANDOM_NUMSEGMENTS		\
	(GP_POLICY_MINIMAL_NUMSEGMENTS + \
	 random() % (GP_POLICY_ALL_NUMSEGMENTS - GP_POLICY_MINIMAL_NUMSEGMENTS + 1))

/*
 * Default set of segments, the value is controlled by the variable
 * gp_create_table_default_numsegments.
 */
#define GP_POLICY_DEFAULT_NUMSEGMENTS		\
( gp_create_table_default_numsegments == GP_DEFAULT_NUMSEGMENTS_FULL    ? GP_POLICY_ALL_NUMSEGMENTS \
: gp_create_table_default_numsegments == GP_DEFAULT_NUMSEGMENTS_RANDOM  ? GP_POLICY_RANDOM_NUMSEGMENTS \
: gp_create_table_default_numsegments == GP_DEFAULT_NUMSEGMENTS_MINIMAL ? GP_POLICY_MINIMAL_NUMSEGMENTS \
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
 * The segments suitable for Entry locus, which include both master and all
 * the segments.
 *
 * FIXME: in fact numsegments only describe a range of segments from 0 to
 * `numsegments-1`, master is not described by it at all.  So far this does
 * not matter.
 */
#define GP_POLICY_ENTRY_NUMSEGMENTS			GP_POLICY_ALL_NUMSEGMENTS

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
	NodeTag         type;
	GpPolicyType ptype;
	int			numsegments;

	/* These fields apply to POLICYTYPE_PARTITIONED. */
	int			nattrs;
	AttrNumber	*attrs;		/* pointer to the first of nattrs attribute numbers.  */
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
extern GpPolicy *createHashPartitionedPolicy(List *keys, int numsegments);

extern bool IsReplicatedTable(Oid relid);

#endif /*_GP_POLICY_H_*/
