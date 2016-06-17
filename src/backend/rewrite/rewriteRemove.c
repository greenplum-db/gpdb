/*-------------------------------------------------------------------------
 *
 * rewriteRemove.c
 *	  routines for removing rewrite rules
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/rewrite/rewriteRemove.c,v 1.70 2008/01/01 19:45:51 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "catalog/catquery.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_rewrite.h"
#include "miscadmin.h"
#include "rewrite/rewriteRemove.h"
#include "rewrite/rewriteSupport.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"


/*
 * RemoveRewriteRule
 *
 * Delete a rule given its name.
 */
void
RemoveRewriteRule(Oid owningRel, const char *ruleName, DropBehavior behavior,
				  bool missing_ok)
{
	HeapTuple	tuple;
	Oid			eventRelationOid;
	ObjectAddress object;
	cqContext	*pcqCtx;

	/*
	 * Find the tuple for the target rule.
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_rewrite "
				" WHERE ev_class = :1 "
				" AND rulename = :2 "
				" FOR UPDATE ",
				ObjectIdGetDatum(owningRel),
				CStringGetDatum((char *) ruleName)));

	tuple = caql_getnext(pcqCtx);

	/*
	 * complain if no rule with such name exists
	 */
	if (!HeapTupleIsValid(tuple))
	{
		if (!missing_ok)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("rule \"%s\" for relation \"%s\" does not exist",
							ruleName, get_rel_name(owningRel))));
		else
			ereport(NOTICE,
					(errmsg("rule \"%s\" for relation \"%s\" does not exist, skipping",
							ruleName, get_rel_name(owningRel))));

		caql_endscan(pcqCtx);
		return;
	}

	/*
	 * Verify user has appropriate permissions.
	 */
	eventRelationOid = ((Form_pg_rewrite) GETSTRUCT(tuple))->ev_class;
	Assert(eventRelationOid == owningRel);
	if (!pg_class_ownercheck(eventRelationOid, GetUserId()))
		aclcheck_error(ACLCHECK_NOT_OWNER, ACL_KIND_CLASS,
					   get_rel_name(eventRelationOid));

	/*
	 * Do the deletion
	 */
	object.classId = RewriteRelationId;
	object.objectId = HeapTupleGetOid(tuple);
	object.objectSubId = 0;

	caql_endscan(pcqCtx);

	performDeletion(&object, behavior);
}


/*
 * Guts of rule deletion.
 */
void
RemoveRewriteRuleById(Oid ruleOid)
{
	cqContext  *pcqCtx;
	Relation	event_relation;
	HeapTuple	tuple;
	Oid			eventRelationOid;
	bool		hasMoreRules;

	/*
	 * Find the tuple for the target rule.
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_rewrite "
				" WHERE oid = :1 "
				" FOR UPDATE ",
				ObjectIdGetDatum(ruleOid)));

	tuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "could not find tuple for rule %u", ruleOid);

	/*
	 * We had better grab AccessExclusiveLock so that we know no other rule
	 * additions/deletions are going on for this relation.	Else we cannot set
	 * relhasrules correctly.  Besides, we don't want to be changing the
	 * ruleset while queries are executing on the rel.
	 */
	eventRelationOid = ((Form_pg_rewrite) GETSTRUCT(tuple))->ev_class;
	event_relation = heap_open(eventRelationOid, AccessExclusiveLock);

	hasMoreRules = event_relation->rd_rules != NULL &&
		event_relation->rd_rules->numLocks > 1;

	/*
	 * Now delete the pg_rewrite tuple for the rule
	 */
	caql_delete_current(pcqCtx);

	caql_endscan(pcqCtx);


	/*
	 * Set pg_class 'relhasrules' field correctly for event relation.
	 *
	 * Important side effect: an SI notice is broadcast to force all backends
	 * (including me!) to update relcache entries with the new rule set.
	 * Therefore, must do this even if relhasrules is still true!
	 */
	SetRelationRuleStatus(eventRelationOid, hasMoreRules, false);

	/* Close rel, but keep lock till commit... */
	heap_close(event_relation, NoLock);
}
