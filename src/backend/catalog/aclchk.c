/*-------------------------------------------------------------------------
 *
 * aclchk.c
 *	  Routines to check access control permissions.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/catalog/aclchk.c,v 1.143.2.1 2008/03/24 19:12:58 tgl Exp $
 *
 * NOTES
 *	  See acl.h.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "catalog/heap.h"
#include "access/sysattr.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/catquery.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/gp_persistent.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_filespace.h"
#include "catalog/pg_type.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "cdb/cdbpartition.h"
#include "commands/dbcommands.h"
#include "commands/tablecmds.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/prep.h"
#include "parser/parse_func.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdisp_query.h"


static void ExecGrant_Relation(InternalGrant *grantStmt);
static void ExecGrant_Database(InternalGrant *grantStmt);
static void ExecGrant_Function(InternalGrant *grantStmt);
static void ExecGrant_Language(InternalGrant *grantStmt);
static void ExecGrant_Namespace(InternalGrant *grantStmt);
static void ExecGrant_Tablespace(InternalGrant *grantStmt);
static void ExecGrant_ExtProtocol(InternalGrant *grantstmt);

static List *objectNamesToOids(GrantObjectType objtype, List *objnames);
static AclMode string_to_privilege(const char *privname);
static const char *privilege_to_string(AclMode privilege);
static AclMode restrict_and_check_grant(bool is_grant, AclMode avail_goptions,
						 bool all_privs, AclMode privileges,
						 Oid objectId, Oid grantorId,
						 AclObjectKind objkind, char *objname);
static AclMode pg_aclmask(AclObjectKind objkind, Oid table_oid, Oid roleid,
		   AclMode mask, AclMaskHow how);


#ifdef ACLDEBUG
static void
dumpacl(Acl *acl)
{
	int			i;
	AclItem    *aip;

	elog(DEBUG2, "acl size = %d, # acls = %d",
		 ACL_SIZE(acl), ACL_NUM(acl));
	aip = ACL_DAT(acl);
	for (i = 0; i < ACL_NUM(acl); ++i)
		elog(DEBUG2, "	acl[%d]: %s", i,
			 DatumGetCString(DirectFunctionCall1(aclitemout,
												 PointerGetDatum(aip + i))));
}
#endif   /* ACLDEBUG */


/*
 * If is_grant is true, adds the given privileges for the list of
 * grantees to the existing old_acl.  If is_grant is false, the
 * privileges for the given grantees are removed from old_acl.
 *
 * NB: the original old_acl is pfree'd.
 */
static Acl *
merge_acl_with_grant(Acl *old_acl, bool is_grant,
					 bool grant_option, DropBehavior behavior,
					 List *grantees, AclMode privileges,
					 Oid grantorId, Oid ownerId, char *objName)
{
	unsigned	modechg;
	ListCell   *j;
	Acl		   *new_acl;

	modechg = is_grant ? ACL_MODECHG_ADD : ACL_MODECHG_DEL;

#ifdef ACLDEBUG
	dumpacl(old_acl);
#endif
	new_acl = old_acl;

	foreach(j, grantees)
	{
		AclItem aclitem;
		Acl		   *newer_acl;

		aclitem.	ai_grantee = lfirst_oid(j);

		/*
		 * Grant options can only be granted to individual roles, not PUBLIC.
		 * The reason is that if a user would re-grant a privilege that he
		 * held through PUBLIC, and later the user is removed, the situation
		 * is impossible to clean up.
		 */
		if (is_grant && grant_option && aclitem.ai_grantee == ACL_ID_PUBLIC)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_GRANT_OPERATION),
					 errmsg("grant options can only be granted to roles")));

		aclitem.	ai_grantor = grantorId;

		/*
		 * The asymmetry in the conditions here comes from the spec.  In
		 * GRANT, the grant_option flag signals WITH GRANT OPTION, which means
		 * to grant both the basic privilege and its grant option. But in
		 * REVOKE, plain revoke revokes both the basic privilege and its grant
		 * option, while REVOKE GRANT OPTION revokes only the option.
		 */
		ACLITEM_SET_PRIVS_GOPTIONS(aclitem,
					(is_grant || !grant_option) ? privileges : ACL_NO_RIGHTS,
				   (!is_grant || grant_option) ? privileges : ACL_NO_RIGHTS);

		newer_acl = aclupdate(new_acl, &aclitem, modechg, ownerId, behavior, objName);

		/* avoid memory leak when there are many grantees */
		pfree(new_acl);
		new_acl = newer_acl;

#ifdef ACLDEBUG
		dumpacl(new_acl);
#endif
	}

	return new_acl;
}

/*
 * Restrict the privileges to what we can actually grant, and emit
 * the standards-mandated warning and error messages.
 */
static AclMode
restrict_and_check_grant(bool is_grant, AclMode avail_goptions, bool all_privs,
						 AclMode privileges, Oid objectId, Oid grantorId,
						 AclObjectKind objkind, char *objname)
{
	AclMode		this_privileges;
	AclMode		whole_mask;

	switch (objkind)
	{
		case ACL_KIND_CLASS:
			whole_mask = ACL_ALL_RIGHTS_RELATION;
			break;
		case ACL_KIND_SEQUENCE:
			whole_mask = ACL_ALL_RIGHTS_SEQUENCE;
			break;
		case ACL_KIND_DATABASE:
			whole_mask = ACL_ALL_RIGHTS_DATABASE;
			break;
		case ACL_KIND_PROC:
			whole_mask = ACL_ALL_RIGHTS_FUNCTION;
			break;
		case ACL_KIND_LANGUAGE:
			whole_mask = ACL_ALL_RIGHTS_LANGUAGE;
			break;
		case ACL_KIND_NAMESPACE:
			whole_mask = ACL_ALL_RIGHTS_NAMESPACE;
			break;
		case ACL_KIND_TABLESPACE:
			whole_mask = ACL_ALL_RIGHTS_TABLESPACE;
			break;
		case ACL_KIND_EXTPROTOCOL:
			whole_mask = ACL_ALL_RIGHTS_EXTPROTOCOL;
			break;
		default:
			elog(ERROR, "unrecognized object kind: %d", objkind);
			/* not reached, but keep compiler quiet */
			return ACL_NO_RIGHTS;
	}

	/*
	 * If we found no grant options, consider whether to issue a hard error.
	 * Per spec, having any privilege at all on the object will get you by
	 * here.
	 */
	if (avail_goptions == ACL_NO_RIGHTS)
	{
		if (pg_aclmask(objkind, objectId, grantorId,
					   whole_mask | ACL_GRANT_OPTION_FOR(whole_mask),
					   ACLMASK_ANY) == ACL_NO_RIGHTS)
			aclcheck_error(ACLCHECK_NO_PRIV, objkind, objname);
	}

	/*
	 * Restrict the operation to what we can actually grant or revoke, and
	 * issue a warning if appropriate.	(For REVOKE this isn't quite what the
	 * spec says to do: the spec seems to want a warning only if no privilege
	 * bits actually change in the ACL. In practice that behavior seems much
	 * too noisy, as well as inconsistent with the GRANT case.)
	 */
	this_privileges = privileges & ACL_OPTION_TO_PRIVS(avail_goptions);
	
	 /*
	 * GPDB: don't do this if we're an execute node. Let the QD handle the
	 * WARNING.
	 */
	if (Gp_role == GP_ROLE_EXECUTE)
		return this_privileges;

	if (is_grant)
	{
		if (this_privileges == 0)
			ereport(WARNING,
					(errcode(ERRCODE_WARNING_PRIVILEGE_NOT_GRANTED),
				  errmsg("no privileges were granted for \"%s\"", objname)));
		else if (!all_privs && this_privileges != privileges)
			ereport(WARNING,
					(errcode(ERRCODE_WARNING_PRIVILEGE_NOT_GRANTED),
			 errmsg("not all privileges were granted for \"%s\"", objname)));
	}
	else
	{
		if (this_privileges == 0)
			ereport(WARNING,
					(errcode(ERRCODE_WARNING_PRIVILEGE_NOT_REVOKED),
			  errmsg("no privileges could be revoked for \"%s\"", objname)));
		else if (!all_privs && this_privileges != privileges)
			ereport(WARNING,
					(errcode(ERRCODE_WARNING_PRIVILEGE_NOT_REVOKED),
					 errmsg("not all privileges could be revoked for \"%s\"", objname)));
	}

	return this_privileges;
}

/*
 * Called to execute the utility commands GRANT and REVOKE
 */
void
ExecuteGrantStmt(GrantStmt *stmt)
{
	InternalGrant istmt;
	ListCell   *cell;
	const char *errormsg;
	AclMode		all_privileges;
	List	   *objs = NIL;
	bool		added_objs = false;

	/*
	 * Turn the regular GrantStmt into the InternalGrant form.
	 */
	istmt.is_grant = stmt->is_grant;
	istmt.objtype = stmt->objtype;
	istmt.objects = objectNamesToOids(stmt->objtype, stmt->objects);

	/* If this is a GRANT/REVOKE on a table, expand partition references */
	if (istmt.objtype == ACL_OBJECT_RELATION)
	{
		foreach(cell, istmt.objects)
		{
			Oid relid = lfirst_oid(cell);
			Relation rel = heap_open(relid, AccessShareLock);
			bool add_self = true;

			if (Gp_role == GP_ROLE_DISPATCH)
			{
				List *a;
				if (rel_is_partitioned(relid))
				{
					PartitionNode *pn = RelationBuildPartitionDesc(rel, false);

					a = all_partition_relids(pn);
					if (a)
						added_objs = true;

					objs = list_concat(objs, a);
				}
				else if (rel_is_child_partition(relid))
				{
					/* get my children */
					a = find_all_inheritors(relid);
					if (a)
						added_objs = true;

					objs = list_concat(objs, a);
	
					/* find_all_inheritors() adds me, don't do it twice */
					add_self = false;
				}
			}

			heap_close(rel, NoLock);

			if (add_self)
				objs = lappend_oid(objs, relid);
		}
		istmt.objects = objs;
	}

	/* If we're dispatching, put the objects back in into the parse tree */
	if (Gp_role == GP_ROLE_DISPATCH && added_objs)
	{
		List *n = NIL;

		foreach(cell, istmt.objects)
		{
			Oid rid = lfirst_oid(cell);
			RangeVar *rv;
			char *nspname = get_namespace_name(get_rel_namespace(rid));
			char *relname = get_rel_name(rid);

			rv = makeRangeVar(nspname, relname, -1);
			n = lappend(n, rv);
		}

		stmt->objects = n;
	}

	if (stmt->cooked_privs)
	{
		istmt.all_privs = false;
		istmt.privileges = 0;
		istmt.grantees = NIL;
		istmt.grant_option = stmt->grant_option;
		istmt.behavior = stmt->behavior;
		istmt.cooked_privs = stmt->cooked_privs;
	}
	else
	{
		/* all_privs to be filled below */
		/* privileges to be filled below */
		istmt.grantees = NIL;
		/* filled below */
		istmt.grant_option = stmt->grant_option;
		istmt.behavior = stmt->behavior;
	
	
		/*
		 * Convert the PrivGrantee list into an Oid list.  Note that at this point
		 * we insert an ACL_ID_PUBLIC into the list if an empty role name is
		 * detected (which is what the grammar uses if PUBLIC is found), so
		 * downstream there shouldn't be any additional work needed to support
		 * this case.
		 */
		foreach(cell, stmt->grantees)
		{
			PrivGrantee *grantee = (PrivGrantee *) lfirst(cell);
	
			if (grantee->rolname == NULL)
				istmt.grantees = lappend_oid(istmt.grantees, ACL_ID_PUBLIC);
			else
				istmt.grantees =
					lappend_oid(istmt.grantees,
								get_roleid_checked(grantee->rolname));
		}
	
		/*
		 * Convert stmt->privileges, a textual list, into an AclMode bitmask.
		 */
		switch (stmt->objtype)
		{
				/*
				 * Because this might be a sequence, we test both relation and
				 * sequence bits, and later do a more limited test when we know
				 * the object type.
				 */
			case ACL_OBJECT_RELATION:
				all_privileges = ACL_ALL_RIGHTS_RELATION | ACL_ALL_RIGHTS_SEQUENCE;
				errormsg = gettext_noop("invalid privilege type %s for relation");
				break;
			case ACL_OBJECT_SEQUENCE:
				all_privileges = ACL_ALL_RIGHTS_SEQUENCE;
				errormsg = gettext_noop("invalid privilege type %s for sequence");
				break;
			case ACL_OBJECT_DATABASE:
				all_privileges = ACL_ALL_RIGHTS_DATABASE;
				errormsg = gettext_noop("invalid privilege type %s for database");
				break;
			case ACL_OBJECT_FUNCTION:
				all_privileges = ACL_ALL_RIGHTS_FUNCTION;
				errormsg = gettext_noop("invalid privilege type %s for function");
				break;
			case ACL_OBJECT_LANGUAGE:
				all_privileges = ACL_ALL_RIGHTS_LANGUAGE;
				errormsg = gettext_noop("invalid privilege type %s for language");
				break;
			case ACL_OBJECT_NAMESPACE:
				all_privileges = ACL_ALL_RIGHTS_NAMESPACE;
				errormsg = gettext_noop("invalid privilege type %s for schema");
				break;
			case ACL_OBJECT_TABLESPACE:
				all_privileges = ACL_ALL_RIGHTS_TABLESPACE;
				errormsg = gettext_noop("invalid privilege type %s for tablespace");
				break;
			case ACL_OBJECT_EXTPROTOCOL:
				all_privileges = ACL_ALL_RIGHTS_EXTPROTOCOL;
				errormsg = gettext_noop("invalid privilege type %s for external protocol");
				break;
			default:
				/* keep compiler quiet */
				all_privileges = ACL_NO_RIGHTS;
				errormsg = NULL;
				elog(ERROR, "unrecognized GrantStmt.objtype: %d",
					 (int) stmt->objtype);
		}
	
		if (stmt->privileges == NIL)
		{
			istmt.all_privs = true;
	
			/*
			 * will be turned into ACL_ALL_RIGHTS_* by the internal routines
			 * depending on the object type
			 */
			istmt.privileges = ACL_NO_RIGHTS;
		}
		else
		{
			istmt.all_privs = false;
			istmt.privileges = ACL_NO_RIGHTS;
	
			foreach(cell, stmt->privileges)
			{
				char	   *privname = strVal(lfirst(cell));
				AclMode		priv = string_to_privilege(privname);
	
				if (priv & ~((AclMode) all_privileges))
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_GRANT_OPERATION),
							 errmsg(errormsg, privilege_to_string(priv))));
	
				istmt.privileges |= priv;
			}
		}
	
		istmt.cooked_privs = NIL;
	}

	ExecGrantStmt_oids(&istmt);
	
		
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *)stmt, "ExecuteGrantStmt");
	}
	
}

/*
 * ExecGrantStmt_oids
 *
 * "Internal" entrypoint for granting and revoking privileges.
 */
void
ExecGrantStmt_oids(InternalGrant *istmt)
{
	switch (istmt->objtype)
	{
		case ACL_OBJECT_RELATION:
		case ACL_OBJECT_SEQUENCE:
			ExecGrant_Relation(istmt);
			break;
		case ACL_OBJECT_DATABASE:
			ExecGrant_Database(istmt);
			break;
		case ACL_OBJECT_FUNCTION:
			ExecGrant_Function(istmt);
			break;
		case ACL_OBJECT_LANGUAGE:
			ExecGrant_Language(istmt);
			break;
		case ACL_OBJECT_NAMESPACE:
			ExecGrant_Namespace(istmt);
			break;
		case ACL_OBJECT_TABLESPACE:
			ExecGrant_Tablespace(istmt);
			break;
		case ACL_OBJECT_EXTPROTOCOL:
			ExecGrant_ExtProtocol(istmt);
			break;
		default:
			elog(ERROR, "unrecognized GrantStmt.objtype: %d",
				 (int) istmt->objtype);
	}
}

/*
 * objectNamesToOids
 *
 * Turn a list of object names of a given type into an Oid list.
 */
static List *
objectNamesToOids(GrantObjectType objtype, List *objnames)
{
	List	   *objects = NIL;
	ListCell   *cell;

	Assert(objnames != NIL);

	switch (objtype)
	{
		case ACL_OBJECT_RELATION:
		case ACL_OBJECT_SEQUENCE:
			foreach(cell, objnames)
			{
				RangeVar   *relvar = (RangeVar *) lfirst(cell);
				Oid			relOid;

				relOid = RangeVarGetRelid(relvar, false);
				objects = lappend_oid(objects, relOid);
			}
			break;
		case ACL_OBJECT_DATABASE:
			foreach(cell, objnames)
			{
				char	   *dbname = strVal(lfirst(cell));
				Oid			dbid;

				dbid = get_database_oid(dbname);
				if (!OidIsValid(dbid))
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_DATABASE),
							 errmsg("database \"%s\" does not exist",
									dbname)));
				objects = lappend_oid(objects, dbid);
			}
			break;
		case ACL_OBJECT_FUNCTION:
			foreach(cell, objnames)
			{
				FuncWithArgs *func = (FuncWithArgs *) lfirst(cell);
				Oid			funcid;

				funcid = LookupFuncNameTypeNames(func->funcname,
												 func->funcargs, false);
				objects = lappend_oid(objects, funcid);
			}
			break;
		case ACL_OBJECT_LANGUAGE:
			foreach(cell, objnames)
			{
				int			fetchCount = 0;
				Oid			objId;
				char	   *langname = strVal(lfirst(cell));

				objId = caql_getoid_plus(
						NULL,
						&fetchCount,
						NULL,
						cql("SELECT oid FROM pg_language "
							" WHERE lanname = :1 ",
							CStringGetDatum(langname)));

				if (0 == fetchCount)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_OBJECT),
							 errmsg("language \"%s\" does not exist",
									langname)));

				objects = lappend_oid(objects, objId);
			}
			break;
		case ACL_OBJECT_NAMESPACE:
			foreach(cell, objnames)
			{
				int			fetchCount = 0;
				Oid			objId;
				char	   *nspname = strVal(lfirst(cell));

				objId = caql_getoid_plus(
						NULL,
						&fetchCount,
						NULL,
						cql("SELECT oid FROM pg_namespace "
							" WHERE nspname = :1 ",
							CStringGetDatum(nspname)));

				if (0 == fetchCount)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_SCHEMA),
							 errmsg("schema \"%s\" does not exist",
									nspname)));

				objects = lappend_oid(objects, objId);
			}
			break;
		case ACL_OBJECT_TABLESPACE:
			foreach(cell, objnames)
			{
				char	   *spcname = strVal(lfirst(cell));
				int			fetchCount = 0;
				Oid			objId;

				objId = caql_getoid_plus(
						NULL,
						&fetchCount,
						NULL,
						cql("SELECT oid FROM pg_tablespace "
							" WHERE spcname = :1",
							CStringGetDatum(spcname)));

				if (0 == fetchCount)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_OBJECT),
					   errmsg("tablespace \"%s\" does not exist", spcname)));

				objects = lappend_oid(objects, objId);
			}
			break;
		case ACL_OBJECT_EXTPROTOCOL:
			foreach(cell, objnames)
			{
				char	   *ptcname = strVal(lfirst(cell));
				Oid			ptcid = LookupExtProtocolOid(ptcname, false);

				objects = lappend_oid(objects, ptcid);
			}
			break;			
		default:
			elog(ERROR, "unrecognized GrantStmt.objtype: %d",
				 (int) objtype);
	}

	return objects;
}

/*
 *	This processes both sequences and non-sequences.
 */
static void
ExecGrant_Relation(InternalGrant *istmt)
{
	Relation	relation;
	ListCell   *cell;

	relation = heap_open(RelationRelationId, RowExclusiveLock);

	foreach(cell, istmt->objects)
	{
		Oid			relOid = lfirst_oid(cell);
		Datum		aclDatum;
		Form_pg_class pg_class_tuple;
		bool		isNull;
		AclMode		avail_goptions;
		AclMode		this_privileges;
		Acl			*old_acl;
		Acl			*new_acl;
		Oid			grantorId;
		Oid			ownerId	 = InvalidOid;
		HeapTuple	tuple;
		HeapTuple	newtuple;
		Datum		values[Natts_pg_class];
		bool		nulls[Natts_pg_class];
		bool		replaces[Natts_pg_class];
		int			nnewmembers;
		Oid		   *newmembers;
		int			noldmembers = 0;
		Oid		   *oldmembers;
		bool		bTemp;
		cqContext	cqc;
		cqContext  *pcqCtx;

		bTemp = false;

		pcqCtx = caql_beginscan(
				caql_addrel(cqclr(&cqc), relation),
				cql("SELECT * FROM pg_class "
					" WHERE oid = :1 "
					" FOR UPDATE ",
					ObjectIdGetDatum(relOid)));
		tuple = caql_getnext(pcqCtx);
		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "cache lookup failed for relation %u", relOid);
		pg_class_tuple = (Form_pg_class) GETSTRUCT(tuple);

		/* Not sensible to grant on an index */
		if (pg_class_tuple->relkind == RELKIND_INDEX)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is an index",
							NameStr(pg_class_tuple->relname))));

		/* Composite types aren't tables either */
		if (pg_class_tuple->relkind == RELKIND_COMPOSITE_TYPE)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is a composite type",
							NameStr(pg_class_tuple->relname))));

		/* Used GRANT SEQUENCE on a non-sequence? */
		if (istmt->objtype == ACL_OBJECT_SEQUENCE &&
			pg_class_tuple->relkind != RELKIND_SEQUENCE)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is not a sequence",
							NameStr(pg_class_tuple->relname))));

		/* pre-cooked privileges -- probably from ADD PARTITION */
		if (istmt->cooked_privs)
		{
			ListCell *lc;
			AclItem *aip;
			int size = ACL_N_SIZE(list_length(istmt->cooked_privs));

			new_acl = (Acl *) palloc0(size);
			SET_VARSIZE(new_acl, size);
			new_acl->ndim = 1;
			new_acl->dataoffset = 0;	/* we never put in any nulls */
			new_acl->elemtype = ACLITEMOID;
			ARR_LBOUND(new_acl)[0] = 1;
			ARR_DIMS(new_acl)[0] = list_length(istmt->cooked_privs);
			aip = ACL_DAT(new_acl);

			foreach(lc, istmt->cooked_privs)
			{
				char *aclstr = strVal(lfirst(lc));
				AclItem *newai;

				newai = DatumGetPointer(DirectFunctionCall1(aclitemin,
											CStringGetDatum(aclstr)));

				aip->ai_grantee = newai->ai_grantee;
				aip->ai_grantor = newai->ai_grantor;
				aip->ai_privs = newai->ai_privs;

				aip++;
			}
		}
		else
		{

			/* 
			 * Adjust the default permissions based on whether it is a 
			 * sequence
			 */
			if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
			{
				if (pg_class_tuple->relkind == RELKIND_SEQUENCE)
					this_privileges = ACL_ALL_RIGHTS_SEQUENCE;
				else
					this_privileges = ACL_ALL_RIGHTS_RELATION;
			}
			else
				this_privileges = istmt->privileges;
	
			/*
			 * The GRANT TABLE syntax can be used for sequences and
			 * non-sequences, so we have to look at the relkind to determine
			 * the supported permissions.  The OR of table and sequence
			 * permissions were already checked.
			 */
			if (istmt->objtype == ACL_OBJECT_RELATION)
			{
				if (pg_class_tuple->relkind == RELKIND_SEQUENCE)
				{
					/*
					 * For backward compatibility, throw just a warning for
					 * invalid sequence permissions when using the non-sequence
					 * GRANT syntax is used.
					 */
					if (this_privileges & ~((AclMode) ACL_ALL_RIGHTS_SEQUENCE))
					{
						/*
						 * Mention the object name because the user
						 * needs to know which operations
						 * succeeded. This is required because WARNING
						 * allows the command to continue.
						 */
						ereport(WARNING,
								(errcode(ERRCODE_INVALID_GRANT_OPERATION),
								 errmsg("sequence \"%s\" only supports USAGE, SELECT, and UPDATE",
										NameStr(pg_class_tuple->relname))));
						this_privileges &= (AclMode) ACL_ALL_RIGHTS_SEQUENCE;
					}
				}
				else
				{
					if (this_privileges & ~((AclMode) ACL_ALL_RIGHTS_RELATION))
					{
						/*
						 * USAGE is the only permission supported by
						 * sequences but not by non-sequences.  Don't
						 * mention the object name because we didn't
						 * in the combined TABLE | SEQUENCE check.
						 */
						ereport(ERROR,
								(errcode(ERRCODE_INVALID_GRANT_OPERATION),
							  errmsg("invalid privilege type USAGE for table")));
					}
				}
			}
	
			/*
			 * Get owner ID and working copy of existing ACL. If
			 * there's no ACL, substitute the proper default.
			 */
			ownerId = pg_class_tuple->relowner;
			aclDatum = caql_getattr(pcqCtx, Anum_pg_class_relacl,
									&isNull);
			if (isNull)
				old_acl = acldefault(pg_class_tuple->relkind == RELKIND_SEQUENCE ?
									 ACL_OBJECT_SEQUENCE : ACL_OBJECT_RELATION,
									 ownerId);
			else
				old_acl = DatumGetAclPCopy(aclDatum);
	
			/* Determine ID to do the grant as, and available grant options */
			select_best_grantor(GetUserId(), this_privileges,
								old_acl, ownerId,
								&grantorId, &avail_goptions);

			/*
			 * Restrict the privileges to what we can actually grant,
			 * and emit the standards-mandated warning and error
			 * messages.
			 */
			this_privileges =
				restrict_and_check_grant(istmt->is_grant, avail_goptions,
										 istmt->all_privs, this_privileges,
										 relOid, grantorId,
									  pg_class_tuple->relkind == RELKIND_SEQUENCE
										 ? ACL_KIND_SEQUENCE : ACL_KIND_CLASS,
										 NameStr(pg_class_tuple->relname));
	
			/*
			 * Generate new ACL.
			 *
			 * We need the members of both old and new ACLs so we can
			 * correct the shared dependency information.
			 */
			noldmembers = aclmembers(old_acl, &oldmembers);
	
			new_acl = merge_acl_with_grant(old_acl, istmt->is_grant,
										   istmt->grant_option, istmt->behavior,
										   istmt->grantees, this_privileges,
										   grantorId, ownerId, NameStr(pg_class_tuple->relname));
		}

		nnewmembers = aclmembers(new_acl, &newmembers);

		/* finished building new ACL value, now insert it */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		MemSet(replaces, false, sizeof(replaces));

		replaces[Anum_pg_class_relacl - 1] = true;
		values[Anum_pg_class_relacl - 1] = PointerGetDatum(new_acl);

		newtuple = caql_modify_current(pcqCtx, values, nulls, replaces);

		caql_update_current(pcqCtx, newtuple);
		/* and Update indexes (implicit) */

		/* MPP-7572: Don't track metadata if table in any
		 * temporary namespace
		 */
		bTemp = isAnyTempNamespace(pg_class_tuple->relnamespace);

		/* MPP-6929: metadata tracking */
		if (!bTemp && 
			(Gp_role == GP_ROLE_DISPATCH)
			&& (
				(pg_class_tuple->relkind == RELKIND_INDEX) ||
				(pg_class_tuple->relkind == RELKIND_RELATION) ||
				(pg_class_tuple->relkind == RELKIND_SEQUENCE) ||
				(pg_class_tuple->relkind == RELKIND_VIEW)))
			MetaTrackUpdObject(RelationRelationId,
							   relOid,
							   GetUserId(), /* not grantorId, */
							   "PRIVILEGE", 
							   (istmt->is_grant) ? "GRANT" : "REVOKE"
					);


		if (!istmt->cooked_privs)
		{
			/* Update the shared dependency ACL info */
			updateAclDependencies(RelationRelationId, relOid,
								  ownerId, istmt->is_grant,
								  noldmembers, oldmembers,
								  nnewmembers, newmembers);
		}

		caql_endscan(pcqCtx);

		pfree(new_acl);

		/* prevent error when processing duplicate objects */
		CommandCounterIncrement();
	}

	heap_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Database(InternalGrant *istmt)
{
	Relation	relation;
	ListCell   *cell;

	if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
		istmt->privileges = ACL_ALL_RIGHTS_DATABASE;

	relation = heap_open(DatabaseRelationId, RowExclusiveLock);

	foreach(cell, istmt->objects)
	{
		Oid			datId = lfirst_oid(cell);
		Form_pg_database pg_database_tuple;
		Datum		aclDatum;
		bool		isNull;
		AclMode		avail_goptions;
		AclMode		this_privileges;
		Acl		   *old_acl;
		Acl		   *new_acl;
		Oid			grantorId;
		Oid			ownerId;
		HeapTuple	newtuple;
		Datum		values[Natts_pg_database];
		bool		nulls[Natts_pg_database];
		bool		replaces[Natts_pg_database];
		int			noldmembers;
		int			nnewmembers;
		Oid		   *oldmembers;
		Oid		   *newmembers;
		HeapTuple	tuple;
		cqContext	cqc;
		cqContext  *pcqCtx;

		pcqCtx = caql_beginscan(
				caql_addrel(cqclr(&cqc), relation),
				cql("SELECT * FROM pg_database "
					" WHERE oid = :1 "
					" FOR UPDATE ",
					ObjectIdGetDatum(datId)));

		tuple = caql_getnext(pcqCtx);

		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "cache lookup failed for database %u", datId);

		pg_database_tuple = (Form_pg_database) GETSTRUCT(tuple);

		/*
		 * Get owner ID and working copy of existing ACL. If there's no ACL,
		 * substitute the proper default.
		 */
		ownerId = pg_database_tuple->datdba;
		aclDatum = heap_getattr(tuple, Anum_pg_database_datacl,
								RelationGetDescr(relation), &isNull);
		if (isNull)
			old_acl = acldefault(ACL_OBJECT_DATABASE, ownerId);
		else
			old_acl = DatumGetAclPCopy(aclDatum);

		/* Determine ID to do the grant as, and available grant options */
		select_best_grantor(GetUserId(), istmt->privileges,
							old_acl, ownerId,
							&grantorId, &avail_goptions);

		/*
		 * Restrict the privileges to what we can actually grant, and emit the
		 * standards-mandated warning and error messages.
		 */
		this_privileges =
			restrict_and_check_grant(istmt->is_grant, avail_goptions,
									 istmt->all_privs, istmt->privileges,
									 datId, grantorId, ACL_KIND_DATABASE,
									 NameStr(pg_database_tuple->datname));

		/*
		 * Generate new ACL.
		 *
		 * We need the members of both old and new ACLs so we can correct the
		 * shared dependency information.
		 */
		noldmembers = aclmembers(old_acl, &oldmembers);

		new_acl = merge_acl_with_grant(old_acl, istmt->is_grant,
									   istmt->grant_option, istmt->behavior,
									   istmt->grantees, this_privileges,
									   grantorId, ownerId, NameStr(pg_database_tuple->datname));

		nnewmembers = aclmembers(new_acl, &newmembers);

		/* finished building new ACL value, now insert it */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		MemSet(replaces, false, sizeof(replaces));

		replaces[Anum_pg_database_datacl - 1] = true;
		values[Anum_pg_database_datacl - 1] = PointerGetDatum(new_acl);

		newtuple = caql_modify_current(pcqCtx, values, nulls, replaces);

		caql_update_current(pcqCtx, newtuple);
		/* and Update indexes (implicit) */

		/* MPP-6929: metadata tracking */
		if (Gp_role == GP_ROLE_DISPATCH)
			MetaTrackUpdObject(DatabaseRelationId,
							   datId,
							   GetUserId(), /* not grantorId, */
							   "PRIVILEGE", 
							   (istmt->is_grant) ? "GRANT" : "REVOKE"
					);

		/* Update the shared dependency ACL info */
		updateAclDependencies(DatabaseRelationId, HeapTupleGetOid(tuple),
							  ownerId, istmt->is_grant,
							  noldmembers, oldmembers,
							  nnewmembers, newmembers);

		caql_endscan(pcqCtx);

		pfree(new_acl);

		/* prevent error when processing duplicate objects */
		CommandCounterIncrement();
	}

	heap_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Function(InternalGrant *istmt)
{
	Relation	relation;
	ListCell   *cell;

	if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
		istmt->privileges = ACL_ALL_RIGHTS_FUNCTION;

	relation = heap_open(ProcedureRelationId, RowExclusiveLock);

	foreach(cell, istmt->objects)
	{
		Oid			funcId = lfirst_oid(cell);
		Form_pg_proc pg_proc_tuple;
		Datum		aclDatum;
		bool		isNull;
		AclMode		avail_goptions;
		AclMode		this_privileges;
		Acl		   *old_acl;
		Acl		   *new_acl;
		Oid			grantorId;
		Oid			ownerId;
		HeapTuple	tuple;
		HeapTuple	newtuple;
		Datum		values[Natts_pg_proc];
		bool		nulls[Natts_pg_proc];
		bool		replaces[Natts_pg_proc];
		int			noldmembers;
		int			nnewmembers;
		Oid		   *oldmembers;
		Oid		   *newmembers;
		cqContext	cqc;
		cqContext  *pcqCtx;

		pcqCtx = caql_beginscan(
				caql_addrel(cqclr(&cqc), relation),
				cql("SELECT * FROM pg_proc "
					" WHERE oid = :1 "
					" FOR UPDATE ",
					ObjectIdGetDatum(funcId)));

		tuple = caql_getnext(pcqCtx);

		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "cache lookup failed for function %u", funcId);

		pg_proc_tuple = (Form_pg_proc) GETSTRUCT(tuple);

		/*
		 * Get owner ID and working copy of existing ACL. If there's no ACL,
		 * substitute the proper default.
		 */
		ownerId = pg_proc_tuple->proowner;
		aclDatum = caql_getattr(pcqCtx, Anum_pg_proc_proacl,
								&isNull);
		if (isNull)
			old_acl = acldefault(ACL_OBJECT_FUNCTION, ownerId);
		else
			old_acl = DatumGetAclPCopy(aclDatum);

		/* Determine ID to do the grant as, and available grant options */
		select_best_grantor(GetUserId(), istmt->privileges,
							old_acl, ownerId,
							&grantorId, &avail_goptions);

		/*
		 * Restrict the privileges to what we can actually grant, and emit the
		 * standards-mandated warning and error messages.
		 */
		this_privileges =
			restrict_and_check_grant(istmt->is_grant, avail_goptions,
									 istmt->all_privs, istmt->privileges,
									 funcId, grantorId, ACL_KIND_PROC,
									 NameStr(pg_proc_tuple->proname));

		/*
		 * Generate new ACL.
		 *
		 * We need the members of both old and new ACLs so we can correct the
		 * shared dependency information.
		 */
		noldmembers = aclmembers(old_acl, &oldmembers);

		new_acl = merge_acl_with_grant(old_acl, istmt->is_grant,
									   istmt->grant_option, istmt->behavior,
									   istmt->grantees, this_privileges,
									   grantorId, ownerId, NameStr(pg_proc_tuple->proname));

		nnewmembers = aclmembers(new_acl, &newmembers);

		/* finished building new ACL value, now insert it */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		MemSet(replaces, false, sizeof(replaces));

		replaces[Anum_pg_proc_proacl - 1] = true;
		values[Anum_pg_proc_proacl - 1] = PointerGetDatum(new_acl);

		newtuple = caql_modify_current(pcqCtx, values, nulls, replaces);

		caql_update_current(pcqCtx, newtuple);
		/* and Update indexes (implicit) */

		/* Update the shared dependency ACL info */
		updateAclDependencies(ProcedureRelationId, funcId,
							  ownerId, istmt->is_grant,
							  noldmembers, oldmembers,
							  nnewmembers, newmembers);

		caql_endscan(pcqCtx);

		pfree(new_acl);

		/* prevent error when processing duplicate objects */
		CommandCounterIncrement();
	}

	heap_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Language(InternalGrant *istmt)
{
	Relation	relation;
	ListCell   *cell;

	if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
		istmt->privileges = ACL_ALL_RIGHTS_LANGUAGE;

	relation = heap_open(LanguageRelationId, RowExclusiveLock);

	foreach(cell, istmt->objects)
	{
		Oid			langId = lfirst_oid(cell);
		Form_pg_language pg_language_tuple;
		Datum		aclDatum;
		bool		isNull;
		AclMode		avail_goptions;
		AclMode		this_privileges;
		Acl		   *old_acl;
		Acl		   *new_acl;
		Oid			grantorId;
		Oid			ownerId;
		HeapTuple	tuple;
		HeapTuple	newtuple;
		Datum		values[Natts_pg_language];
		bool		nulls[Natts_pg_language];
		bool		replaces[Natts_pg_language];
		int			noldmembers;
		int			nnewmembers;
		Oid		   *oldmembers;
		Oid		   *newmembers;

		tuple = SearchSysCache(LANGOID,
							   ObjectIdGetDatum(langId),
							   0, 0, 0);
		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "cache lookup failed for language %u", langId);

		pg_language_tuple = (Form_pg_language) GETSTRUCT(tuple);

		if (!pg_language_tuple->lanpltrusted)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("language \"%s\" is not trusted",
							NameStr(pg_language_tuple->lanname)),
				   errhint("Only superusers can use untrusted languages.")));

		/*
		 * Get owner ID and working copy of existing ACL. If there's no ACL,
		 * substitute the proper default.
		 */
		ownerId = pg_language_tuple->lanowner;
		aclDatum = SysCacheGetAttr(LANGNAME, tuple, Anum_pg_language_lanacl,
								   &isNull);
		if (isNull)
			old_acl = acldefault(ACL_OBJECT_LANGUAGE, ownerId);
		else
			old_acl = DatumGetAclPCopy(aclDatum);

		/* Determine ID to do the grant as, and available grant options */
		select_best_grantor(GetUserId(), istmt->privileges,
							old_acl, ownerId,
							&grantorId, &avail_goptions);

		/*
		 * Restrict the privileges to what we can actually grant, and emit the
		 * standards-mandated warning and error messages.
		 */
		this_privileges =
			restrict_and_check_grant(istmt->is_grant, avail_goptions,
									 istmt->all_privs, istmt->privileges,
									 langId, grantorId, ACL_KIND_LANGUAGE,
									 NameStr(pg_language_tuple->lanname));

		/*
		 * Generate new ACL.
		 *
		 * We need the members of both old and new ACLs so we can correct the
		 * shared dependency information.
		 */
		noldmembers = aclmembers(old_acl, &oldmembers);

		new_acl = merge_acl_with_grant(old_acl, istmt->is_grant,
									   istmt->grant_option, istmt->behavior,
									   istmt->grantees, this_privileges,
									   grantorId, ownerId, NameStr(pg_language_tuple->lanname));

		nnewmembers = aclmembers(new_acl, &newmembers);

		/* finished building new ACL value, now insert it */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		MemSet(replaces, false, sizeof(replaces));

		replaces[Anum_pg_language_lanacl - 1] = true;
		values[Anum_pg_language_lanacl - 1] = PointerGetDatum(new_acl);

		newtuple = heap_modify_tuple(tuple, RelationGetDescr(relation), values,
									 nulls, replaces);

		simple_heap_update(relation, &newtuple->t_self, newtuple);

		/* keep the catalog indexes up to date */
		CatalogUpdateIndexes(relation, newtuple);

		/* Update the shared dependency ACL info */
		updateAclDependencies(LanguageRelationId, HeapTupleGetOid(tuple),
							  ownerId, istmt->is_grant,
							  noldmembers, oldmembers,
							  nnewmembers, newmembers);

		ReleaseSysCache(tuple);

		pfree(new_acl);

		/* prevent error when processing duplicate objects */
		CommandCounterIncrement();
	}

	heap_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Namespace(InternalGrant *istmt)
{
	Relation	relation;
	ListCell   *cell;

	if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
		istmt->privileges = ACL_ALL_RIGHTS_NAMESPACE;

	relation = heap_open(NamespaceRelationId, RowExclusiveLock);

	foreach(cell, istmt->objects)
	{
		Oid			nspid = lfirst_oid(cell);
		Form_pg_namespace pg_namespace_tuple;
		Datum		aclDatum;
		bool		isNull;
		AclMode		avail_goptions;
		AclMode		this_privileges;
		Acl		   *old_acl;
		Acl		   *new_acl;
		Oid			grantorId;
		Oid			ownerId;
		HeapTuple	tuple;
		HeapTuple	newtuple;
		Datum		values[Natts_pg_namespace];
		bool		nulls[Natts_pg_namespace];
		bool		replaces[Natts_pg_namespace];
		int			noldmembers;
		int			nnewmembers;
		Oid		   *oldmembers;
		Oid		   *newmembers;
		cqContext	cqc;
		cqContext  *pcqCtx;

		pcqCtx = caql_beginscan(
				caql_addrel(cqclr(&cqc), relation),
				cql("SELECT * FROM pg_namespace "
					" WHERE oid = :1 "
					" FOR UPDATE ",
					ObjectIdGetDatum(nspid)));

		tuple = caql_getnext(pcqCtx);

		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "cache lookup failed for namespace %u", nspid);

		pg_namespace_tuple = (Form_pg_namespace) GETSTRUCT(tuple);

		/*
		 * Get owner ID and working copy of existing ACL. If there's no ACL,
		 * substitute the proper default.
		 */
		ownerId = pg_namespace_tuple->nspowner;
		aclDatum = caql_getattr(pcqCtx,
								Anum_pg_namespace_nspacl,
								&isNull);
		if (isNull)
			old_acl = acldefault(ACL_OBJECT_NAMESPACE, ownerId);
		else
			old_acl = DatumGetAclPCopy(aclDatum);

		/* Determine ID to do the grant as, and available grant options */
		select_best_grantor(GetUserId(), istmt->privileges,
							old_acl, ownerId,
							&grantorId, &avail_goptions);

		/*
		 * Restrict the privileges to what we can actually grant, and emit the
		 * standards-mandated warning and error messages.
		 */
		this_privileges =
			restrict_and_check_grant(istmt->is_grant, avail_goptions,
									 istmt->all_privs, istmt->privileges,
									 nspid, grantorId, ACL_KIND_NAMESPACE,
									 NameStr(pg_namespace_tuple->nspname));

		/*
		 * Generate new ACL.
		 *
		 * We need the members of both old and new ACLs so we can correct the
		 * shared dependency information.
		 */
		noldmembers = aclmembers(old_acl, &oldmembers);

		new_acl = merge_acl_with_grant(old_acl, istmt->is_grant,
									   istmt->grant_option, istmt->behavior,
									   istmt->grantees, this_privileges,
									   grantorId, ownerId, NameStr(pg_namespace_tuple->nspname));

		nnewmembers = aclmembers(new_acl, &newmembers);

		/* finished building new ACL value, now insert it */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		MemSet(replaces, false, sizeof(replaces));

		replaces[Anum_pg_namespace_nspacl - 1] = true;
		values[Anum_pg_namespace_nspacl - 1] = PointerGetDatum(new_acl);

		newtuple = caql_modify_current(pcqCtx, values, nulls, replaces);

		caql_update_current(pcqCtx, newtuple);
		/* and Update indexes (implicit) */

		/* MPP-6929: metadata tracking */
		if (Gp_role == GP_ROLE_DISPATCH)
			MetaTrackUpdObject(NamespaceRelationId,
							   nspid,
							   GetUserId(), /* not grantorId, */
							   "PRIVILEGE", 
							   (istmt->is_grant) ? "GRANT" : "REVOKE"
					);

		/* Update the shared dependency ACL info */
		updateAclDependencies(NamespaceRelationId, HeapTupleGetOid(tuple),
							  ownerId, istmt->is_grant,
							  noldmembers, oldmembers,
							  nnewmembers, newmembers);

		caql_endscan(pcqCtx);

		pfree(new_acl);

		/* prevent error when processing duplicate objects */
		CommandCounterIncrement();
	}

	heap_close(relation, RowExclusiveLock);
}

static void
ExecGrant_Tablespace(InternalGrant *istmt)
{
	Relation	relation;
	ListCell   *cell;

	if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
		istmt->privileges = ACL_ALL_RIGHTS_TABLESPACE;

	relation = heap_open(TableSpaceRelationId, RowExclusiveLock);

	foreach(cell, istmt->objects)
	{
		Oid			tblId = lfirst_oid(cell);
		Form_pg_tablespace pg_tablespace_tuple;
		Datum		aclDatum;
		bool		isNull;
		AclMode		avail_goptions;
		AclMode		this_privileges;
		Acl		   *old_acl;
		Acl		   *new_acl;
		Oid			grantorId;
		Oid			ownerId;
		HeapTuple	newtuple;
		Datum		values[Natts_pg_tablespace];
		bool		nulls[Natts_pg_tablespace];
		bool		replaces[Natts_pg_tablespace];
		int			noldmembers;
		int			nnewmembers;
		Oid		   *oldmembers;
		Oid		   *newmembers;
		HeapTuple	tuple;
		cqContext	cqc;
		cqContext  *pcqCtx;

		pcqCtx = caql_beginscan(
				caql_addrel(cqclr(&cqc), relation),
				cql("SELECT * FROM pg_tablespace "
					" WHERE oid = :1 "
					" FOR UPDATE ",
					ObjectIdGetDatum(tblId)));

		tuple = caql_getnext(pcqCtx);

		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "cache lookup failed for tablespace %u", tblId);

		pg_tablespace_tuple = (Form_pg_tablespace) GETSTRUCT(tuple);

		/*
		 * Get owner ID and working copy of existing ACL. If there's no ACL,
		 * substitute the proper default.
		 */
		ownerId = pg_tablespace_tuple->spcowner;
		aclDatum = heap_getattr(tuple, Anum_pg_tablespace_spcacl,
								RelationGetDescr(relation), &isNull);
		if (isNull)
			old_acl = acldefault(ACL_OBJECT_TABLESPACE, ownerId);
		else
			old_acl = DatumGetAclPCopy(aclDatum);

		/* Determine ID to do the grant as, and available grant options */
		select_best_grantor(GetUserId(), istmt->privileges,
							old_acl, ownerId,
							&grantorId, &avail_goptions);

		/*
		 * Restrict the privileges to what we can actually grant, and emit the
		 * standards-mandated warning and error messages.
		 */
		this_privileges =
			restrict_and_check_grant(istmt->is_grant, avail_goptions,
									 istmt->all_privs, istmt->privileges,
									 tblId, grantorId, ACL_KIND_TABLESPACE,
									 NameStr(pg_tablespace_tuple->spcname));

		/*
		 * Generate new ACL.
		 *
		 * We need the members of both old and new ACLs so we can correct the
		 * shared dependency information.
		 */
		noldmembers = aclmembers(old_acl, &oldmembers);

		new_acl = merge_acl_with_grant(old_acl, istmt->is_grant,
									   istmt->grant_option, istmt->behavior,
									   istmt->grantees, this_privileges,
									   grantorId, ownerId, NameStr(pg_tablespace_tuple->spcname));

		nnewmembers = aclmembers(new_acl, &newmembers);

		/* finished building new ACL value, now insert it */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		MemSet(replaces, false, sizeof(replaces));

		replaces[Anum_pg_tablespace_spcacl - 1] = true;
		values[Anum_pg_tablespace_spcacl - 1] = PointerGetDatum(new_acl);

		newtuple = caql_modify_current(pcqCtx, values, nulls, replaces);

		caql_update_current(pcqCtx, newtuple);
		/* and Update indexes (implicit) */

		/* MPP-6929: metadata tracking */
		if (Gp_role == GP_ROLE_DISPATCH)
			MetaTrackUpdObject(TableSpaceRelationId,
							   tblId,
							   GetUserId(), /* not grantorId, */
							   "PRIVILEGE", 
							   (istmt->is_grant) ? "GRANT" : "REVOKE"
					);

		/* Update the shared dependency ACL info */
		updateAclDependencies(TableSpaceRelationId, tblId,
							  ownerId, istmt->is_grant,
							  noldmembers, oldmembers,
							  nnewmembers, newmembers);

		caql_endscan(pcqCtx);

		pfree(new_acl);

		/* prevent error when processing duplicate objects */
		CommandCounterIncrement();
	}

	heap_close(relation, RowExclusiveLock);
}

static void
ExecGrant_ExtProtocol(InternalGrant *istmt)
{
	Relation	relation;
	ListCell   *cell;

	if (istmt->all_privs && istmt->privileges == ACL_NO_RIGHTS)
		istmt->privileges = ACL_ALL_RIGHTS_EXTPROTOCOL;

	relation = heap_open(ExtprotocolRelationId, RowExclusiveLock);

	foreach(cell, istmt->objects)
	{
		Oid			ptcid = lfirst_oid(cell);
		bool		isNull;
		bool		isTrusted;
		AclMode		avail_goptions;
		AclMode		this_privileges;
		Acl		   *old_acl;
		Acl		   *new_acl;
		Oid			grantorId;
		Oid			ownerId;
		Name	    ptcname;
		HeapTuple	tuple;
		HeapTuple	newtuple;
		Datum		values[Natts_pg_extprotocol];
		bool		nulls[Natts_pg_extprotocol];
		bool		replaces[Natts_pg_extprotocol];
		int			noldmembers;
		int			nnewmembers;
		Oid		   *oldmembers;
		Oid		   *newmembers;
		Datum		ownerDatum;
		Datum		aclDatum;
		Datum		trustedDatum;
		Datum		ptcnameDatum;
		cqContext	cqc;
		cqContext  *pcqCtx;
		TupleDesc	reldsc = RelationGetDescr(relation);

		pcqCtx = caql_beginscan(
				caql_addrel(cqclr(&cqc), relation),
				cql("SELECT * FROM pg_extprotocol "
					" WHERE oid = :1 "
					" FOR UPDATE ",
					ObjectIdGetDatum(ptcid)));

		tuple = caql_getnext(pcqCtx);

		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "lookup failed for external protocol %u", ptcid);
		
		ownerDatum = heap_getattr(tuple, 
								  Anum_pg_extprotocol_ptcowner,
								  reldsc, 
								  &isNull);
		
		if(isNull)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("protocol '%u' has no owner defined",
							 ptcid)));	

		/*
		 * Get owner ID and working copy of existing ACL. If there's no ACL,
		 * substitute the proper default.
		 */
		ownerId = DatumGetObjectId(ownerDatum);
		
		aclDatum = heap_getattr(tuple, 
								Anum_pg_extprotocol_ptcacl,
								reldsc, 
								&isNull);
		
		if (isNull)
			old_acl = acldefault(ACL_OBJECT_EXTPROTOCOL, ownerId);
		else
			old_acl = DatumGetAclPCopy(aclDatum);

		ptcnameDatum = heap_getattr(tuple, 
									Anum_pg_extprotocol_ptcname,
									reldsc, 
									&isNull);

		ptcname = DatumGetName(ptcnameDatum);
		
		if(isNull)
			ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("internal error: protocol '%u' has no name defined",
						 ptcid)));
		
		trustedDatum = heap_getattr(tuple, 
									Anum_pg_extprotocol_ptctrusted,
									reldsc, 
									&isNull);
			
		isTrusted = DatumGetBool(trustedDatum);
		
		if (!isTrusted)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("protocol \"%s\" is not trusted",
							NameStr(*ptcname)),
				   errhint("Only superusers may use untrusted protocols.")));

		/* Determine ID to do the grant as, and available grant options */
		select_best_grantor(GetUserId(), istmt->privileges,
							old_acl, ownerId,
							&grantorId, &avail_goptions);

		/*
		 * Restrict the privileges to what we can actually grant, and emit the
		 * standards-mandated warning and error messages.
		 */
		this_privileges =
			restrict_and_check_grant(istmt->is_grant, avail_goptions,
									 istmt->all_privs, istmt->privileges,
									 ptcid, grantorId, ACL_KIND_EXTPROTOCOL,
									 NameStr(*ptcname));

		/*
		 * Generate new ACL.
		 *
		 * We need the members of both old and new ACLs so we can correct the
		 * shared dependency information.
		 */
		noldmembers = aclmembers(old_acl, &oldmembers);

		new_acl = merge_acl_with_grant(old_acl, istmt->is_grant,
									   istmt->grant_option, istmt->behavior,
									   istmt->grantees, this_privileges,
									   grantorId, ownerId, NameStr(*ptcname));

		nnewmembers = aclmembers(new_acl, &newmembers);

		/* finished building new ACL value, now insert it */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));
		MemSet(replaces, false, sizeof(replaces));

		replaces[Anum_pg_extprotocol_ptcacl - 1] = true;
		values[Anum_pg_extprotocol_ptcacl - 1] = PointerGetDatum(new_acl);

		newtuple = caql_modify_current(pcqCtx, values, nulls, replaces);

		caql_update_current(pcqCtx, newtuple);
		/* and Update indexes (implicit) */

		/* Update the shared dependency ACL info */
		updateAclDependencies(ExtprotocolRelationId,
							  HeapTupleGetOid(tuple),
							  ownerId, istmt->is_grant,
							  noldmembers, oldmembers,
							  nnewmembers, newmembers);

		caql_endscan(pcqCtx);

		pfree(new_acl);

		/* prevent error when processing duplicate objects */
		CommandCounterIncrement();
	}

	heap_close(relation, RowExclusiveLock);
}

static AclMode
string_to_privilege(const char *privname)
{
	if (strcmp(privname, "insert") == 0)
		return ACL_INSERT;
	if (strcmp(privname, "select") == 0)
		return ACL_SELECT;
	if (strcmp(privname, "update") == 0)
		return ACL_UPDATE;
	if (strcmp(privname, "delete") == 0)
		return ACL_DELETE;
	if (strcmp(privname, "truncate") == 0)
		return ACL_TRUNCATE;
	if (strcmp(privname, "references") == 0)
		return ACL_REFERENCES;
	if (strcmp(privname, "trigger") == 0)
		return ACL_TRIGGER;
	if (strcmp(privname, "execute") == 0)
		return ACL_EXECUTE;
	if (strcmp(privname, "usage") == 0)
		return ACL_USAGE;
	if (strcmp(privname, "create") == 0)
		return ACL_CREATE;
	if (strcmp(privname, "temporary") == 0)
		return ACL_CREATE_TEMP;
	if (strcmp(privname, "temp") == 0)
		return ACL_CREATE_TEMP;
	if (strcmp(privname, "connect") == 0)
		return ACL_CONNECT;
	if (strcmp(privname, "rule") == 0)
		return 0;				/* ignore old RULE privileges */
	ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			 errmsg("unrecognized privilege type \"%s\"", privname)));
	return 0;					/* appease compiler */
}

static const char *
privilege_to_string(AclMode privilege)
{
	switch (privilege)
	{
		case ACL_INSERT:
			return "INSERT";
		case ACL_SELECT:
			return "SELECT";
		case ACL_UPDATE:
			return "UPDATE";
		case ACL_DELETE:
			return "DELETE";
		case ACL_TRUNCATE:
			return "TRUNCATE";
		case ACL_REFERENCES:
			return "REFERENCES";
		case ACL_TRIGGER:
			return "TRIGGER";
		case ACL_EXECUTE:
			return "EXECUTE";
		case ACL_USAGE:
			return "USAGE";
		case ACL_CREATE:
			return "CREATE";
		case ACL_CREATE_TEMP:
			return "TEMP";
		case ACL_CONNECT:
			return "CONNECT";
		default:
			elog(ERROR, "unrecognized privilege: %d", (int) privilege);
	}
	return NULL;				/* appease compiler */
}

/*
 * Standardized reporting of aclcheck permissions failures.
 *
 * Note: we do not double-quote the %s's below, because many callers
 * supply strings that might be already quoted.
 */

static const char *const no_priv_msg[MAX_ACL_KIND] =
{
	/* ACL_KIND_CLASS */
	gettext_noop("permission denied for relation %s"),
	/* ACL_KIND_SEQUENCE */
	gettext_noop("permission denied for sequence %s"),
	/* ACL_KIND_DATABASE */
	gettext_noop("permission denied for database %s"),
	/* ACL_KIND_PROC */
	gettext_noop("permission denied for function %s"),
	/* ACL_KIND_OPER */
	gettext_noop("permission denied for operator %s"),
	/* ACL_KIND_TYPE */
	gettext_noop("permission denied for type %s"),
	/* ACL_KIND_LANGUAGE */
	gettext_noop("permission denied for language %s"),
	/* ACL_KIND_NAMESPACE */
	gettext_noop("permission denied for schema %s"),
	/* ACL_KIND_OPCLASS */
	gettext_noop("permission denied for operator class %s"),
	/* ACL_KIND_OPFAMILY */
	gettext_noop("permission denied for operator family %s"),
	/* ACL_KIND_CONVERSION */
	gettext_noop("permission denied for conversion %s"),
	/* ACL_KIND_TABLESPACE */
	gettext_noop("permission denied for tablespace %s"),
	/* ACL_KIND_TSDICTIONARY */
	gettext_noop("permission denied for text search dictionary %s"),
	/* ACL_KIND_TSCONFIGURATION */
	gettext_noop("permission denied for text search configuration %s"),
	/* ACL_KIND_FILESPACE */
	gettext_noop("permission denied for filespace %s"),	
	/* ACL_KIND_EXTPROTOCOL */
	gettext_noop("permission denied for external protocol %s")	
};

static const char *const not_owner_msg[MAX_ACL_KIND] =
{
	/* ACL_KIND_CLASS */
	gettext_noop("must be owner of relation %s"),
	/* ACL_KIND_SEQUENCE */
	gettext_noop("must be owner of sequence %s"),
	/* ACL_KIND_DATABASE */
	gettext_noop("must be owner of database %s"),
	/* ACL_KIND_PROC */
	gettext_noop("must be owner of function %s"),
	/* ACL_KIND_OPER */
	gettext_noop("must be owner of operator %s"),
	/* ACL_KIND_TYPE */
	gettext_noop("must be owner of type %s"),
	/* ACL_KIND_LANGUAGE */
	gettext_noop("must be owner of language %s"),
	/* ACL_KIND_NAMESPACE */
	gettext_noop("must be owner of schema %s"),
	/* ACL_KIND_OPCLASS */
	gettext_noop("must be owner of operator class %s"),
	/* ACL_KIND_OPFAMILY */
	gettext_noop("must be owner of operator family %s"),
	/* ACL_KIND_CONVERSION */
	gettext_noop("must be owner of conversion %s"),
	/* ACL_KIND_TABLESPACE */
	gettext_noop("must be owner of tablespace %s"),
	/* ACL_KIND_TSDICTIONARY */
	gettext_noop("must be owner of text search dictionary %s"),
	/* ACL_KIND_TSCONFIGURATION */
	gettext_noop("must be owner of text search configuration %s"),
	/* ACL_KIND_FILESPACE */
	gettext_noop("must be owner of filespace %s"),
	/* ACL_KIND_EXTPROTOCOL */
	gettext_noop("must be owner of external protocol %s")
};


void
aclcheck_error(AclResult aclerr, AclObjectKind objectkind,
			   const char *objectname)
{
	switch (aclerr)
	{
		case ACLCHECK_OK:
			/* no error, so return to caller */
			break;
		case ACLCHECK_NO_PRIV:
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg(no_priv_msg[objectkind], objectname)));
			break;
		case ACLCHECK_NOT_OWNER:
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg(not_owner_msg[objectkind], objectname)));
			break;
		default:
			elog(ERROR, "unrecognized AclResult: %d", (int) aclerr);
			break;
	}
}


/* Check if given user has rolcatupdate privilege according to pg_authid */
static bool
has_rolcatupdate(Oid roleid)
{
	bool		rolcatupdate;
	HeapTuple	tuple;
	cqContext  *pcqCtx;

	/* XXX XXX: SELECT rolcatupdate */

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_authid "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(roleid)));
	
	tuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("role with OID %u does not exist", roleid)));

	rolcatupdate = ((Form_pg_authid) GETSTRUCT(tuple))->rolcatupdate;

	caql_endscan(pcqCtx);

	return rolcatupdate;
}

/*
 * Relay for the various pg_*_mask routines depending on object kind
 */
static AclMode
pg_aclmask(AclObjectKind objkind, Oid table_oid, Oid roleid,
		   AclMode mask, AclMaskHow how)
{
	switch (objkind)
	{
		case ACL_KIND_CLASS:
		case ACL_KIND_SEQUENCE:
			return pg_class_aclmask(table_oid, roleid, mask, how);
		case ACL_KIND_DATABASE:
			return pg_database_aclmask(table_oid, roleid, mask, how);
		case ACL_KIND_PROC:
			return pg_proc_aclmask(table_oid, roleid, mask, how);
		case ACL_KIND_LANGUAGE:
			return pg_language_aclmask(table_oid, roleid, mask, how);
		case ACL_KIND_NAMESPACE:
			return pg_namespace_aclmask(table_oid, roleid, mask, how);
		case ACL_KIND_TABLESPACE:
			return pg_tablespace_aclmask(table_oid, roleid, mask, how);
		case ACL_KIND_EXTPROTOCOL:
			return pg_extprotocol_aclmask(table_oid, roleid, mask, how);
		default:
			elog(ERROR, "unrecognized objkind: %d",
				 (int) objkind);
			/* not reached, but keep compiler quiet */
			return ACL_NO_RIGHTS;
	}
}

/*
 * Exported routine for examining a user's privileges for a table
 *
 * See aclmask() for a description of the API.
 *
 * Note: we give lookup failure the full ereport treatment because the
 * has_table_privilege() family of functions allow users to pass
 * any random OID to this function.  Likewise for the sibling functions
 * below.
 */
AclMode
pg_class_aclmask(Oid table_oid, Oid roleid,
				 AclMode mask, AclMaskHow how)
{
	AclMode		result;
	HeapTuple	tuple;
	Form_pg_class classForm;
	Datum		aclDatum;
	bool		isNull;
	Acl		   *acl;
	Oid			ownerId;
	bool		updating;
	cqContext  *pcqCtx;

	/*
	 * Must get the relation's tuple from pg_class
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_class "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(table_oid)));
	
	tuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_TABLE),
				 errmsg("relation with OID %u does not exist",
						table_oid)));
	classForm = (Form_pg_class) GETSTRUCT(tuple);

	/*
	 * Deny anyone permission to update a system catalog unless
	 * pg_authid.rolcatupdate is set.	(This is to let superusers protect
	 * themselves from themselves.)  Also allow it if allowSystemTableMods.
	 *
	 * As of 7.4 we have some updatable system views; those shouldn't be
	 * protected in this way.  Assume the view rules can take care of
	 * themselves.	ACL_USAGE is if we ever have system sequences.
	 */

	updating = ((mask & (ACL_INSERT | ACL_UPDATE | ACL_DELETE | ACL_TRUNCATE | ACL_USAGE)) != 0);

	if (updating)
	{
		if (IsSystemClass(classForm) &&
			classForm->relkind != RELKIND_VIEW &&
			!has_rolcatupdate(roleid) &&
			!allowSystemTableModsDDL)
		{
#ifdef ACLDEBUG
			elog(DEBUG2, "permission denied for system catalog update");
#endif
			mask &= ~(ACL_INSERT | ACL_UPDATE | ACL_DELETE | ACL_TRUNCATE | ACL_USAGE);
		}

		/* 
		 * Deny even superusers with rolcatupdate permissions to modify the
		 * persistent tables.  These tables are special and rely on precise
		 * settings of the ctids within the tables, attempting to modify these
		 * tables via INSERT/UPDATE/DELETE is a mistake.
		 */
		if (GpPersistent_IsPersistentRelation(table_oid))
		{
			if ((mask & ACL_UPDATE) &&
				gp_permit_persistent_metadata_update)
			{
				// Let this UPDATE through.
			}
			else
			{
#ifdef ACLDEBUG
				elog(DEBUG2, "permission denied for persistent system catalog update");
#endif
				mask &= ~(ACL_INSERT | ACL_UPDATE | ACL_DELETE | ACL_USAGE);
			}
		}

		/*
		 * And, gp_relation_node, too.
		 */
		if (table_oid == GpRelationNodeRelationId)
		{
			if (gp_permit_relation_node_change)
			{
				// Let this change through.
			}
			else
			{
#ifdef ACLDEBUG
				elog(DEBUG2, "permission denied for gp_relation_node system catalog update");
#endif
				mask &= ~(ACL_INSERT | ACL_UPDATE | ACL_DELETE | ACL_USAGE);
			}
		}
	}
	/*
	 * Otherwise, superusers bypass all permission-checking.
	 */
	if (superuser_arg(roleid))
	{
#ifdef ACLDEBUG
		elog(DEBUG2, "OID %u is superuser, home free", roleid);
#endif
		caql_endscan(pcqCtx);
		return mask;
	}

	/*
	 * Normal case: get the relation's ACL from pg_class
	 */
	ownerId = classForm->relowner;

	aclDatum = caql_getattr(pcqCtx, Anum_pg_class_relacl,
							&isNull);
	if (isNull)
	{
		/* No ACL, so build default ACL */
		acl = acldefault(classForm->relkind == RELKIND_SEQUENCE ?
						 ACL_OBJECT_SEQUENCE : ACL_OBJECT_RELATION,
						 ownerId);
		aclDatum = (Datum) 0;
	}
	else
	{
		/* detoast rel's ACL if necessary */
		acl = DatumGetAclP(aclDatum);
	}

	result = aclmask(acl, roleid, ownerId, mask, how);

	/* if we have a detoasted copy, free it */
	if (acl && (Pointer) acl != DatumGetPointer(aclDatum))
		pfree(acl);

	caql_endscan(pcqCtx);

	return result;
}

/*
 * Exported routine for examining a user's privileges for a database
 */
AclMode
pg_database_aclmask(Oid db_oid, Oid roleid,
					AclMode mask, AclMaskHow how)
{
	AclMode		result;
	HeapTuple	tuple;
	Datum		aclDatum;
	bool		isNull;
	Acl		   *acl;
	Oid			ownerId;
	cqContext  *pcqCtx;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return mask;

	/*
	 * Get the database's ACL from pg_database
	 */

	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_database "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(db_oid)));
	
	tuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_DATABASE),
				 errmsg("database with OID %u does not exist", db_oid)));

	ownerId = ((Form_pg_database) GETSTRUCT(tuple))->datdba;

	aclDatum = caql_getattr(pcqCtx, Anum_pg_database_datacl,
							&isNull);
	if (isNull)
	{
		/* No ACL, so build default ACL */
		acl = acldefault(ACL_OBJECT_DATABASE, ownerId);
		aclDatum = (Datum) 0;
	}
	else
	{
		/* detoast ACL if necessary */
		acl = DatumGetAclP(aclDatum);
	}

	result = aclmask(acl, roleid, ownerId, mask, how);

	/* if we have a detoasted copy, free it */
	if (acl && (Pointer) acl != DatumGetPointer(aclDatum))
		pfree(acl);

	caql_endscan(pcqCtx);

	return result;
}

/*
 * Exported routine for examining a user's privileges for a function
 */
AclMode
pg_proc_aclmask(Oid proc_oid, Oid roleid,
				AclMode mask, AclMaskHow how)
{
	AclMode		result;
	HeapTuple	tuple;
	Datum		aclDatum;
	bool		isNull;
	Acl		   *acl;
	Oid			ownerId;
	cqContext  *pcqCtx;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return mask;

	/*
	 * Get the function's ACL from pg_proc
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_proc "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(proc_oid)));

	tuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("function with OID %u does not exist", proc_oid)));

	ownerId = ((Form_pg_proc) GETSTRUCT(tuple))->proowner;

	aclDatum = caql_getattr(pcqCtx, Anum_pg_proc_proacl,
							&isNull);
	if (isNull)
	{
		/* No ACL, so build default ACL */
		acl = acldefault(ACL_OBJECT_FUNCTION, ownerId);
		aclDatum = (Datum) 0;
	}
	else
	{
		/* detoast ACL if necessary */
		acl = DatumGetAclP(aclDatum);
	}

	result = aclmask(acl, roleid, ownerId, mask, how);

	/* if we have a detoasted copy, free it */
	if (acl && (Pointer) acl != DatumGetPointer(aclDatum))
		pfree(acl);

	caql_endscan(pcqCtx);

	return result;
}

/*
 * Exported routine for examining a user's privileges for a language
 */
AclMode
pg_language_aclmask(Oid lang_oid, Oid roleid,
					AclMode mask, AclMaskHow how)
{
	AclMode		result;
	HeapTuple	tuple;
	Datum		aclDatum;
	bool		isNull;
	Acl		   *acl;
	Oid			ownerId;
	cqContext  *pcqCtx;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return mask;

	/*
	 * Get the language's ACL from pg_language
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_language "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(lang_oid)));

	tuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("language with OID %u does not exist", lang_oid)));

	ownerId = ((Form_pg_language) GETSTRUCT(tuple))->lanowner;

	aclDatum = caql_getattr(pcqCtx, Anum_pg_language_lanacl,
							&isNull);
	if (isNull)
	{
		/* No ACL, so build default ACL */
		acl = acldefault(ACL_OBJECT_LANGUAGE, ownerId);
		aclDatum = (Datum) 0;
	}
	else
	{
		/* detoast ACL if necessary */
		acl = DatumGetAclP(aclDatum);
	}

	result = aclmask(acl, roleid, ownerId, mask, how);

	/* if we have a detoasted copy, free it */
	if (acl && (Pointer) acl != DatumGetPointer(aclDatum))
		pfree(acl);

	caql_endscan(pcqCtx);

	return result;
}

/*
 * Exported routine for examining a user's privileges for a namespace
 */
AclMode
pg_namespace_aclmask(Oid nsp_oid, Oid roleid,
					 AclMode mask, AclMaskHow how)
{
	AclMode		result;
	HeapTuple	tuple;
	Datum		aclDatum;
	bool		isNull;
	Acl		   *acl;
	Oid			ownerId;
	cqContext  *pcqCtx;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return mask;

	/*
	 * If we have been assigned this namespace as a temp namespace, check to
	 * make sure we have CREATE TEMP permission on the database, and if so act
	 * as though we have all standard (but not GRANT OPTION) permissions on
	 * the namespace.  If we don't have CREATE TEMP, act as though we have
	 * only USAGE (and not CREATE) rights.
	 *
	 * This may seem redundant given the check in InitTempTableNamespace, but
	 * it really isn't since current user ID may have changed since then. The
	 * upshot of this behavior is that a SECURITY DEFINER function can create
	 * temp tables that can then be accessed (if permission is granted) by
	 * code in the same session that doesn't have permissions to create temp
	 * tables.
	 *
	 * XXX Would it be safe to ereport a special error message as
	 * InitTempTableNamespace does?  Returning zero here means we'll get a
	 * generic "permission denied for schema pg_temp_N" message, which is not
	 * remarkably user-friendly.
	 */
	if (isTempNamespace(nsp_oid))
	{
		if (pg_database_aclcheck(MyDatabaseId, roleid,
								 ACL_CREATE_TEMP) == ACLCHECK_OK)
			return mask & ACL_ALL_RIGHTS_NAMESPACE;
		else
			return mask & ACL_USAGE;
	}

	/*
	 * Get the schema's ACL from pg_namespace
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("SELECT * FROM pg_namespace "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(nsp_oid)));

	tuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_SCHEMA),
				 errmsg("schema with OID %u does not exist", nsp_oid)));

	ownerId = ((Form_pg_namespace) GETSTRUCT(tuple))->nspowner;

	aclDatum = caql_getattr(pcqCtx, Anum_pg_namespace_nspacl,
							   &isNull);
	if (isNull)
	{
		/* No ACL, so build default ACL */
		acl = acldefault(ACL_OBJECT_NAMESPACE, ownerId);
		aclDatum = (Datum) 0;
	}
	else
	{
		/* detoast ACL if necessary */
		acl = DatumGetAclP(aclDatum);
	}

	result = aclmask(acl, roleid, ownerId, mask, how);

	/* if we have a detoasted copy, free it */
	if (acl && (Pointer) acl != DatumGetPointer(aclDatum))
		pfree(acl);

	caql_endscan(pcqCtx);

	return result;
}

/*
 * Exported routine for examining a user's privileges for a tablespace
 */
AclMode
pg_tablespace_aclmask(Oid spc_oid, Oid roleid,
					  AclMode mask, AclMaskHow how)
{
	AclMode		result;
	Relation	pg_tablespace;
	HeapTuple	tuple;
	Datum		aclDatum;
	bool		isNull;
	Acl		   *acl;
	Oid			ownerId;
	cqContext	cqc;
	cqContext  *pcqCtx;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return mask;

	/*
	 * Get the tablespace's ACL from pg_tablespace
	 *
	 * There's no syscache for pg_tablespace, so must look the hard way
	 */
	pg_tablespace = heap_open(TableSpaceRelationId, AccessShareLock);

	/* XXX XXX select spcowner, spcacl */

	pcqCtx = caql_beginscan(
			caql_addrel(cqclr(&cqc), pg_tablespace), 
			cql("SELECT * FROM pg_tablespace "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(spc_oid)));

	tuple = caql_getnext(pcqCtx);

	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("tablespace with OID %u does not exist", spc_oid)));

	ownerId = ((Form_pg_tablespace) GETSTRUCT(tuple))->spcowner;

	aclDatum = heap_getattr(tuple, Anum_pg_tablespace_spcacl,
							RelationGetDescr(pg_tablespace), &isNull);

	if (isNull)
	{
		/* No ACL, so build default ACL */
		acl = acldefault(ACL_OBJECT_TABLESPACE, ownerId);
		aclDatum = (Datum) 0;
	}
	else
	{
		/* detoast ACL if necessary */
		acl = DatumGetAclP(aclDatum);
	}

	result = aclmask(acl, roleid, ownerId, mask, how);

	/* if we have a detoasted copy, free it */
	if (acl && (Pointer) acl != DatumGetPointer(aclDatum))
		pfree(acl);

	caql_endscan(pcqCtx);
	heap_close(pg_tablespace, AccessShareLock);

	return result;
}

/*
 * Exported routine for examining a user's privileges for an external
 * protocol.
 */
AclMode
pg_extprotocol_aclmask(Oid ptcOid, Oid roleid,
					   AclMode mask, AclMaskHow how)
{
	AclMode		result;
	HeapTuple	tuple;
	Datum		aclDatum;
	Datum		ownerDatum;
	bool		isNull;
	Acl		   *acl;
	Oid			ownerId;
	Relation	rel;
	cqContext	cqc;
	cqContext  *pcqCtx;

	/* Bypass permission checks for superusers */
	if (superuser_arg(roleid))
		return mask;
	
	rel = heap_open(ExtprotocolRelationId, AccessShareLock);

	pcqCtx = caql_beginscan(
			caql_addrel(cqclr(&cqc), rel), 
			cql("SELECT * FROM pg_extprotocol "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(ptcOid)));

	tuple = caql_getnext(pcqCtx);

	/* We assume that there can be at most one matching tuple */
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "protocol %u could not be found", ptcOid);

	ownerDatum = heap_getattr(tuple, 
							  Anum_pg_extprotocol_ptcowner, 
							  RelationGetDescr(rel),
							  &isNull);
	
	if(isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid extprotocol owner value: NULL")));	

	ownerId = DatumGetObjectId(ownerDatum);

	aclDatum = heap_getattr(tuple, 
							Anum_pg_extprotocol_ptcacl, 
							RelationGetDescr(rel),
						    &isNull);
	
	if (isNull)
	{
		/* No ACL, so build default ACL */
		acl = acldefault(ACL_OBJECT_EXTPROTOCOL, ownerId);
		aclDatum = (Datum) 0;
	}
	else
	{
		/* detoast rel's ACL if necessary */
		acl = DatumGetAclP(aclDatum);
	}

	result = aclmask(acl, roleid, ownerId, mask, how);

	/* if we have a detoasted copy, free it */
	if (acl && (Pointer) acl != DatumGetPointer(aclDatum))
		pfree(acl);

	/* Finish up scan and close pg_extprotocol catalog. */
	caql_endscan(pcqCtx);	
	heap_close(rel, AccessShareLock);

	return result;
}

/*
 * Exported routine for checking a user's access privileges to a table
 *
 * Returns ACLCHECK_OK if the user has any of the privileges identified by
 * 'mode'; otherwise returns a suitable error code (in practice, always
 * ACLCHECK_NO_PRIV).
 */
AclResult
pg_class_aclcheck(Oid table_oid, Oid roleid, AclMode mode)
{
	if (pg_class_aclmask(table_oid, roleid, mode, ACLMASK_ANY) != 0)
		return ACLCHECK_OK;
	else
		return ACLCHECK_NO_PRIV;
}

/*
 * Exported routine for checking a user's access privileges to a database
 */
AclResult
pg_database_aclcheck(Oid db_oid, Oid roleid, AclMode mode)
{
	if (pg_database_aclmask(db_oid, roleid, mode, ACLMASK_ANY) != 0)
		return ACLCHECK_OK;
	else
		return ACLCHECK_NO_PRIV;
}

/*
 * Exported routine for checking a user's access privileges to a function
 */
AclResult
pg_proc_aclcheck(Oid proc_oid, Oid roleid, AclMode mode)
{
	if (pg_proc_aclmask(proc_oid, roleid, mode, ACLMASK_ANY) != 0)
		return ACLCHECK_OK;
	else
		return ACLCHECK_NO_PRIV;
}

/*
 * Exported routine for checking a user's access privileges to a language
 */
AclResult
pg_language_aclcheck(Oid lang_oid, Oid roleid, AclMode mode)
{
	if (pg_language_aclmask(lang_oid, roleid, mode, ACLMASK_ANY) != 0)
		return ACLCHECK_OK;
	else
		return ACLCHECK_NO_PRIV;
}

/*
 * Exported routine for checking a user's access privileges to a namespace
 */
AclResult
pg_namespace_aclcheck(Oid nsp_oid, Oid roleid, AclMode mode)
{
	if (pg_namespace_aclmask(nsp_oid, roleid, mode, ACLMASK_ANY) != 0)
		return ACLCHECK_OK;
	else
		return ACLCHECK_NO_PRIV;
}

/*
 * Exported routine for checking a user's access privileges to a tablespace
 */
AclResult
pg_tablespace_aclcheck(Oid spc_oid, Oid roleid, AclMode mode)
{
	if (pg_tablespace_aclmask(spc_oid, roleid, mode, ACLMASK_ANY) != 0)
		return ACLCHECK_OK;
	else
		return ACLCHECK_NO_PRIV;
}

/*
 * Exported routine for checking a user's access privileges to an
 * external protocol
 */
AclResult
pg_extprotocol_aclcheck(Oid ptcid, Oid roleid, AclMode mode)
{
	if (pg_extprotocol_aclmask(ptcid, roleid, mode, ACLMASK_ANY) != 0)
		return ACLCHECK_OK;
	else
		return ACLCHECK_NO_PRIV;
}

/*
 * Ownership check for a relation (specified by OID).
 */
bool
pg_class_ownercheck(Oid class_oid, Oid roleid)
{
	Oid			ownerId;
	int			fetchCount = 0;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	ownerId = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT relowner FROM pg_class WHERE oid = :1 ",
				ObjectIdGetDatum(class_oid)));

	if (0 == fetchCount)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_TABLE),
				 errmsg("relation with OID %u does not exist", class_oid)));

	return has_privs_of_role(roleid, ownerId);
}

/*
 * Ownership check for a type (specified by OID).
 */
bool
pg_type_ownercheck(Oid type_oid, Oid roleid)
{
	Oid			ownerId;
	int			fetchCount = 0;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	ownerId = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT typowner FROM pg_type WHERE oid = :1 ",
				ObjectIdGetDatum(type_oid)));

	if (0 == fetchCount)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("type with OID %u does not exist", type_oid)));

	return has_privs_of_role(roleid, ownerId);
}

/*
 * Ownership check for an operator (specified by OID).
 */
bool
pg_oper_ownercheck(Oid oper_oid, Oid roleid)
{
	Oid			ownerId;
	int			fetchCount = 0;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	ownerId = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oprowner FROM pg_operator WHERE oid = :1 ",
				ObjectIdGetDatum(oper_oid)));

	if (0 == fetchCount)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("operator with OID %u does not exist", oper_oid)));

	return has_privs_of_role(roleid, ownerId);
}

/*
 * Ownership check for a function (specified by OID).
 */
bool
pg_proc_ownercheck(Oid proc_oid, Oid roleid)
{
	Oid			ownerId;
	int			fetchCount = 0;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	ownerId = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT proowner FROM pg_proc "
				" WHERE oid = :1 ",
				ObjectIdGetDatum(proc_oid)));

	if (0 == fetchCount)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("function with OID %u does not exist", proc_oid)));

	return has_privs_of_role(roleid, ownerId);
}

/*
 * Ownership check for a procedural language (specified by OID)
 */
bool
pg_language_ownercheck(Oid lan_oid, Oid roleid)
{
	HeapTuple	tuple;
	Oid			ownerId;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	tuple = SearchSysCache(LANGOID,
						   ObjectIdGetDatum(lan_oid),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),
				 errmsg("language with OID %u does not exist", lan_oid)));

	ownerId = ((Form_pg_language) GETSTRUCT(tuple))->lanowner;

	ReleaseSysCache(tuple);

	return has_privs_of_role(roleid, ownerId);
}

/*
 * Ownership check for a namespace (specified by OID).
 */
bool
pg_namespace_ownercheck(Oid nsp_oid, Oid roleid)
{
	Oid			ownerId;
	int			fetchCount = 0;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	ownerId = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT nspowner FROM pg_namespace WHERE oid = :1 ",
				ObjectIdGetDatum(nsp_oid)));

	if (0 == fetchCount)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_SCHEMA),
				 errmsg("schema with OID %u does not exist", nsp_oid)));

	return has_privs_of_role(roleid, ownerId);
}

/*
 * Ownership check for a tablespace (specified by OID).
 */
bool
pg_tablespace_ownercheck(Oid spc_oid, Oid roleid)
{
	Oid			spcowner;
	int			fetchCount = 0;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	spcowner = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT spcowner FROM pg_tablespace WHERE oid = :1 ",
				ObjectIdGetDatum(spc_oid)));

	if (0 == fetchCount)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("tablespace with OID %u does not exist", spc_oid)));

	return has_privs_of_role(roleid, spcowner);
}

/*
 * Ownership check for a filespace (specified by OID).
 */
bool
pg_filespace_ownercheck(Oid fsoid, Oid roleid)
{
	Oid			owner;
	int			fetchCount = 0;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	owner = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT fsowner FROM pg_filespace WHERE oid = :1 ",
				ObjectIdGetDatum(fsoid)));

	if (0 == fetchCount)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("filepace with OID %u does not exist", fsoid)));

	return has_privs_of_role(roleid, owner);
}


/*
 * Ownership check for an operator class (specified by OID).
 */
bool
pg_opclass_ownercheck(Oid opc_oid, Oid roleid)
{
	Oid			ownerId;
	int			fetchCount = 0;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	ownerId = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT opcowner FROM pg_opclass WHERE oid = :1 ",
				ObjectIdGetDatum(opc_oid)));

	if (0 == fetchCount)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("operator class with OID %u does not exist",
						opc_oid)));

	return has_privs_of_role(roleid, ownerId);
}

/*
 * Ownership check for an operator family (specified by OID).
 */
bool
pg_opfamily_ownercheck(Oid opf_oid, Oid roleid)
{
	HeapTuple	tuple;
	Oid			ownerId;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	tuple = SearchSysCache(OPFAMILYOID,
						   ObjectIdGetDatum(opf_oid),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("operator family with OID %u does not exist",
						opf_oid)));

	ownerId = ((Form_pg_opfamily) GETSTRUCT(tuple))->opfowner;

	ReleaseSysCache(tuple);

	return has_privs_of_role(roleid, ownerId);
}

/*
 * Ownership check for a text search dictionary (specified by OID).
 */
bool
pg_ts_dict_ownercheck(Oid dict_oid, Oid roleid)
{
	HeapTuple	tuple;
	Oid			ownerId;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	tuple = SearchSysCache(TSDICTOID,
						   ObjectIdGetDatum(dict_oid),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("text search dictionary with OID %u does not exist",
						dict_oid)));

	ownerId = ((Form_pg_ts_dict) GETSTRUCT(tuple))->dictowner;

	ReleaseSysCache(tuple);

	return has_privs_of_role(roleid, ownerId);
}

/*
 * Ownership check for a text search configuration (specified by OID).
 */
bool
pg_ts_config_ownercheck(Oid cfg_oid, Oid roleid)
{
	HeapTuple	tuple;
	Oid			ownerId;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	tuple = SearchSysCache(TSCONFIGOID,
						   ObjectIdGetDatum(cfg_oid),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
			   errmsg("text search configuration with OID %u does not exist",
					  cfg_oid)));

	ownerId = ((Form_pg_ts_config) GETSTRUCT(tuple))->cfgowner;

	ReleaseSysCache(tuple);

	return has_privs_of_role(roleid, ownerId);
}


/*
 * Ownership check for a database (specified by OID).
 */
bool
pg_database_ownercheck(Oid db_oid, Oid roleid)
{
	Oid			dba;
	int			fetchCount = 0;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	dba = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT datdba FROM pg_database WHERE oid = :1 ",
				ObjectIdGetDatum(db_oid)));

	if (0 == fetchCount)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_DATABASE),
				 errmsg("database with OID %u does not exist", db_oid)));

	return has_privs_of_role(roleid, dba);
}

/*
 * Ownership check for a conversion (specified by OID).
 */
bool
pg_conversion_ownercheck(Oid conv_oid, Oid roleid)
{
	HeapTuple	tuple;
	Oid			ownerId;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	tuple = SearchSysCache(CONVOID,
						   ObjectIdGetDatum(conv_oid),
						   0, 0, 0);
	if (!HeapTupleIsValid(tuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("conversion with OID %u does not exist", conv_oid)));

	ownerId = ((Form_pg_conversion) GETSTRUCT(tuple))->conowner;

	ReleaseSysCache(tuple);

	return has_privs_of_role(roleid, ownerId);
}

/*
 * Ownership check for a foreign server (specified by OID).
 */
bool
pg_extprotocol_ownercheck(Oid protOid, Oid roleid)
{
	Oid			ownerId;
	int			fetchCount = 0;
	bool		isNull;

	/* Superusers bypass all permission checking. */
	if (superuser_arg(roleid))
		return true;

	ownerId = caql_getoid_plus(
			NULL,
			&fetchCount,
			&isNull,
			cql("SELECT ptcowner FROM pg_extprotocol WHERE oid = :1 ",
				ObjectIdGetDatum(protOid)));

	/* We assume that there can be at most one matching tuple */
	if (0 == fetchCount)
		elog(ERROR, "protocol %u could not be found", protOid);

	/* XXX XXX: this column not null (?) */
	if(isNull)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("got invalid extprotocol owner value: NULL")));	

	return has_privs_of_role(roleid, ownerId);
}
