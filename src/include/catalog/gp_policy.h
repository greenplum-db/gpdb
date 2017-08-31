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
/*
 * Defines for gp_policy
 */
#define GpPolicyRelationName		"gp_distribution_policy"

#define GpPolicyRelationId  5002

CATALOG(gp_distribution_policy,5002) BKI_WITHOUT_OIDS
{
	Oid			localoid;
	int2		attrnums[1];
} FormData_gp_policy;

/* GPDB added foreign key definitions for gpcheckcat. */
FOREIGN_KEY(localoid REFERENCES pg_class(oid));

#define Natts_gp_policy			2
#define Anum_gp_policy_localoid	1
#define Anum_gp_policy_attrnums	2

/*
 * GpPolicyType represents a type of policy under which a relation's
 * tuples may be assigned to a component database.
 */
typedef enum GpPolicyType
{
	POLICYTYPE_PARTITIONED,		/* Tuples partitioned onto segment database. */
	POLICYTYPE_ENTRY			/* Tuples stored on entry database. */
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
	GpPolicyType ptype;

	/* These fields apply to POLICYTYPE_PARTITIONED. */
	int			nattrs;
	AttrNumber	attrs[1];		/* the first of nattrs attribute numbers.  */
} GpPolicy;

#define SizeOfGpPolicy(nattrs)	(offsetof(GpPolicy, attrs) + sizeof(AttrNumber) * (nattrs))

/*
 * GpPolicyCopy -- Return a copy of a GpPolicy object.
 *
 * The copy is palloc'ed in the specified context.
 */
GpPolicy *GpPolicyCopy(MemoryContext mcxt, const GpPolicy *src);

/* GpPolicyEqual
 *
 * A field-by-field comparison just to facilitate comparing IntoClause
 * (which embeds this) in equalFuncs.c
 */
bool GpPolicyEqual(const GpPolicy *lft, const GpPolicy *rgt);

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
GpPolicy *GpPolicyFetch(MemoryContext mcxt, Oid tbloid);

/*
 * GpPolicyStore: sets the GpPolicy for a table.
 */
void GpPolicyStore(Oid tbloid, const GpPolicy *policy);

void GpPolicyReplace(Oid tbloid, const GpPolicy *policy);

void GpPolicyRemove(Oid tbloid);

bool GpPolicyIsRandomly(GpPolicy *policy);

extern GpPolicy *createRandomDistribution(void);

#endif /*_GP_POLICY_H_*/
