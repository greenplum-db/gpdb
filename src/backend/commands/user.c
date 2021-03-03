/*-------------------------------------------------------------------------
 *
 * user.c
 *	  Commands for manipulating roles (formerly called users).
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/commands/user.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_auth_members.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_database.h"
#include "catalog/pg_db_role_setting.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "commands/seclabel.h"
#include "commands/user.h"
#include "libpq/crypt.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/fmgroids.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"

#include "catalog/oid_dispatch.h"
#include "catalog/pg_auth_time_constraint.h"
#include "catalog/pg_resgroup.h"
#include "catalog/pg_resqueue.h"
#include "commands/resgroupcmds.h"
#include "executor/execdesc.h"
#include "libpq/auth.h"
#include "utils/resource_manager.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbvars.h"


typedef struct extAuthPair
{
	char	   *protocol;
	char	   *type;
} extAuthPair;

/* GUC parameter */
int			Password_encryption = PASSWORD_TYPE_MD5;

/* Hook to check passwords in CreateRole() and AlterRole() */
check_password_hook_type check_password_hook = NULL;

static void AddRoleMems(const char *rolename, Oid roleid,
						List *memberSpecs, List *memberIds,
						Oid grantorId, bool admin_opt);
static void DelRoleMems(const char *rolename, Oid roleid,
						List *memberSpecs, List *memberIds,
						bool admin_opt);
static extAuthPair *TransformExttabAuthClause(DefElem *defel);
static void SetCreateExtTableForRole(List* allow,
			List* disallow, bool* createrextgpfd,
			bool* createrexthttp, bool* createwextgpfd);

static char *daysofweek[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
							 "Thursday", "Friday", "Saturday"};
static int16 ExtractAuthInterpretDay(Value * day);
static void ExtractAuthIntervalClause(DefElem *defel,
			authInterval *authInterval);
static void AddRoleDenials(const char *rolename, Oid roleid,
			List *addintervals);
static void DelRoleDenials(const char *rolename, Oid roleid,
			List *dropintervals);


/* Check if current user has createrole privileges */
static bool
have_createrole_privilege(void)
{
	return has_createrole_privilege(GetUserId());
}


/*
 * CREATE ROLE
 */
Oid
CreateRole(ParseState *pstate, CreateRoleStmt *stmt)
{
	Relation	pg_authid_rel;
	TupleDesc	pg_authid_dsc;
	HeapTuple	tuple;
	Datum		new_record[Natts_pg_authid];
	bool		new_record_nulls[Natts_pg_authid];
	Oid			roleid;
	ListCell   *item;
	ListCell   *option;
	char	   *password = NULL;	/* user password */
	bool		issuper = false;	/* Make the user a superuser? */
	bool		inherit = true; /* Auto inherit privileges? */
	bool		createrole = false; /* Can this user create roles? */
	bool		createdb = false;	/* Can the user create databases? */
	bool		canlogin = false;	/* Can this user login? */
	bool		isreplication = false;	/* Is this a replication role? */
	bool		createrextgpfd = false; /* Can create readable gpfdist exttab? */
	bool		createrexthttp = false; /* Can create readable http exttab? */
	bool		createwextgpfd = false; /* Can create writable gpfdist exttab? */
	List	   *exttabcreate = NIL;		/* external table create privileges being added  */
	List	   *exttabnocreate = NIL;	/* external table create privileges being removed */
	bool		bypassrls = false;	/* Is this a row security enabled role? */
	int			connlimit = -1; /* maximum connections allowed */
	List	   *addroleto = NIL;	/* roles to make this a member of */
	List	   *rolemembers = NIL;	/* roles to be members of this role */
	List	   *adminmembers = NIL; /* roles to be admins of this role */
	char	   *validUntil = NULL;	/* time the login is valid until */
	Datum		validUntil_datum;	/* same, as timestamptz Datum */
	bool		validUntil_null;
	char	   *resqueue = NULL;		/* resource queue for this role */
	char	   *resgroup = NULL;		/* resource group for this role */
	List	   *addintervals = NIL;	/* list of time intervals for which login should be denied */
	DefElem    *dpassword = NULL;
	DefElem    *dresqueue = NULL;
	DefElem    *dresgroup = NULL;
	DefElem    *dissuper = NULL;
	DefElem    *dinherit = NULL;
	DefElem    *dcreaterole = NULL;
	DefElem    *dcreatedb = NULL;
	DefElem    *dcanlogin = NULL;
	DefElem    *disreplication = NULL;
	DefElem    *dconnlimit = NULL;
	DefElem    *daddroleto = NULL;
	DefElem    *drolemembers = NULL;
	DefElem    *dadminmembers = NULL;
	DefElem    *dvalidUntil = NULL;
	DefElem    *dbypassRLS = NULL;

	/* The defaults can vary depending on the original statement type */
	switch (stmt->stmt_type)
	{
		case ROLESTMT_ROLE:
			break;
		case ROLESTMT_USER:
			canlogin = true;
			/* may eventually want inherit to default to false here */
			break;
		case ROLESTMT_GROUP:
			break;
	}

	/* Extract options from the statement node tree */
	foreach(option, stmt->options)
	{
		DefElem    *defel = (DefElem *) lfirst(option);

		if (strcmp(defel->defname, "password") == 0)
		{
			if (dpassword)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			dpassword = defel;
		}
		else if (strcmp(defel->defname, "sysid") == 0)
		{
			if (Gp_role != GP_ROLE_EXECUTE)
			ereport(NOTICE,
					(errmsg("SYSID can no longer be specified")));
		}
		else if (strcmp(defel->defname, "superuser") == 0)
		{
			if (dissuper)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			dissuper = defel;
		}
		else if (strcmp(defel->defname, "inherit") == 0)
		{
			if (dinherit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			dinherit = defel;
		}
		else if (strcmp(defel->defname, "createrole") == 0)
		{
			if (dcreaterole)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			dcreaterole = defel;
		}
		else if (strcmp(defel->defname, "createdb") == 0)
		{
			if (dcreatedb)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			dcreatedb = defel;
		}
		else if (strcmp(defel->defname, "canlogin") == 0)
		{
			if (dcanlogin)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			dcanlogin = defel;
		}
		else if (strcmp(defel->defname, "isreplication") == 0)
		{
			if (disreplication)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			disreplication = defel;
		}
		else if (strcmp(defel->defname, "connectionlimit") == 0)
		{
			if (dconnlimit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			dconnlimit = defel;
		}
		else if (strcmp(defel->defname, "addroleto") == 0)
		{
			if (daddroleto)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			daddroleto = defel;
		}
		else if (strcmp(defel->defname, "rolemembers") == 0)
		{
			if (drolemembers)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			drolemembers = defel;
		}
		else if (strcmp(defel->defname, "adminmembers") == 0)
		{
			if (dadminmembers)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			dadminmembers = defel;
		}
		else if (strcmp(defel->defname, "validUntil") == 0)
		{
			if (dvalidUntil)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			dvalidUntil = defel;
		}
		else if (strcmp(defel->defname, "resourceQueue") == 0)
		{
			if (dresqueue)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dresqueue = defel;
		}
		else if (strcmp(defel->defname, "resourceGroup") == 0)
		{
			if (dresgroup)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dresgroup = defel;
		}
		else if (strcmp(defel->defname, "exttabauth") == 0)
		{
			extAuthPair *extauth;

			extauth = TransformExttabAuthClause(defel);

			/* now actually append our transformed key value pairs to the list */
			exttabcreate = lappend(exttabcreate, extauth);
		}
		else if (strcmp(defel->defname, "exttabnoauth") == 0)
		{
			extAuthPair *extauth;

			extauth = TransformExttabAuthClause(defel);

			/* now actually append our transformed key value pairs to the list */
			exttabnocreate = lappend(exttabnocreate, extauth);
		}
		else if (strcmp(defel->defname, "deny") == 0)
		{
			authInterval *interval = (authInterval *) palloc0(sizeof(authInterval));

			ExtractAuthIntervalClause(defel, interval);

			addintervals = lappend(addintervals, interval);
		}
		else if (strcmp(defel->defname, "bypassrls") == 0)
		{
			if (dbypassRLS)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options"),
						 parser_errposition(pstate, defel->location)));
			dbypassRLS = defel;
		}
		else
			elog(ERROR, "option \"%s\" not recognized",
				 defel->defname);
	}

	if (dpassword && dpassword->arg)
		password = strVal(dpassword->arg);
	if (dissuper)
		issuper = intVal(dissuper->arg) != 0;
	if (dinherit)
		inherit = intVal(dinherit->arg) != 0;
	if (dcreaterole)
		createrole = intVal(dcreaterole->arg) != 0;
	if (dcreatedb)
		createdb = intVal(dcreatedb->arg) != 0;
	if (dcanlogin)
		canlogin = intVal(dcanlogin->arg) != 0;
	if (disreplication)
		isreplication = intVal(disreplication->arg) != 0;
	if (dconnlimit)
	{
		connlimit = intVal(dconnlimit->arg);
		if (connlimit < -1)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid connection limit: %d", connlimit)));
	}
	if (daddroleto)
		addroleto = (List *) daddroleto->arg;
	if (drolemembers)
		rolemembers = (List *) drolemembers->arg;
	if (dadminmembers)
		adminmembers = (List *) dadminmembers->arg;
	if (dvalidUntil)
		validUntil = strVal(dvalidUntil->arg);
	if (dresqueue)
		resqueue = strVal(linitial((List *) dresqueue->arg));
	if (dresgroup)
		resgroup = strVal(linitial((List *) dresgroup->arg));
	if (dbypassRLS)
		bypassrls = intVal(dbypassRLS->arg) != 0;

	/* Check some permissions first */
	if (issuper)
	{
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to create superusers")));
	}
	else if (isreplication)
	{
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to create replication users")));
	}
	else if (bypassrls)
	{
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to change bypassrls attribute")));
	}
	else
	{
		if (!have_createrole_privilege())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("permission denied to create role")));
	}

	/*
	 * Check that the user is not trying to create a role in the reserved
	 * "pg_" namespace.
	 */
	if (IsReservedName(stmt->role))
		ereport(ERROR,
				(errcode(ERRCODE_RESERVED_NAME),
				 errmsg("role name \"%s\" is reserved",
						stmt->role),
				 errdetail("Role names starting with \"pg_\" are reserved.")));

	/*
	 * If built with appropriate switch, whine when regression-testing
	 * conventions for role names are violated.
	 */
#ifdef ENFORCE_REGRESSION_TEST_NAME_RESTRICTIONS
	if (strncmp(stmt->role, "regress_", 8) != 0)
		elog(WARNING, "roles created by regression test cases should have names starting with \"regress_\"");
#endif

	/*
	 * Check the pg_authid relation to be certain the role doesn't already
	 * exist.
	 */
	pg_authid_rel = table_open(AuthIdRelationId, RowExclusiveLock);
	pg_authid_dsc = RelationGetDescr(pg_authid_rel);

	if (OidIsValid(get_role_oid(stmt->role, true)))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("role \"%s\" already exists",
						stmt->role)));

	/* Convert validuntil to internal form */
	if (validUntil)
	{
		validUntil_datum = DirectFunctionCall3(timestamptz_in,
											   CStringGetDatum(validUntil),
											   ObjectIdGetDatum(InvalidOid),
											   Int32GetDatum(-1));
		validUntil_null = false;
	}
	else
	{
		validUntil_datum = (Datum) 0;
		validUntil_null = true;
	}

	/*
	 * Call the password checking hook if there is one defined
	 */
	if (check_password_hook && password)
		(*check_password_hook) (stmt->role,
								password,
								get_password_type(password),
								validUntil_datum,
								validUntil_null);

	/*
	 * Build a tuple to insert
	 */
	MemSet(new_record, 0, sizeof(new_record));
	MemSet(new_record_nulls, false, sizeof(new_record_nulls));

	new_record[Anum_pg_authid_rolname - 1] =
		DirectFunctionCall1(namein, CStringGetDatum(stmt->role));

	new_record[Anum_pg_authid_rolsuper - 1] = BoolGetDatum(issuper);
	new_record[Anum_pg_authid_rolinherit - 1] = BoolGetDatum(inherit);
	new_record[Anum_pg_authid_rolcreaterole - 1] = BoolGetDatum(createrole);
	new_record[Anum_pg_authid_rolcreatedb - 1] = BoolGetDatum(createdb);
	new_record[Anum_pg_authid_rolcanlogin - 1] = BoolGetDatum(canlogin);
	new_record[Anum_pg_authid_rolreplication - 1] = BoolGetDatum(isreplication);
	new_record[Anum_pg_authid_rolconnlimit - 1] = Int32GetDatum(connlimit);

	/* Set the CREATE EXTERNAL TABLE permissions for this role */
	if (exttabcreate || exttabnocreate)
		SetCreateExtTableForRole(exttabcreate, exttabnocreate, &createrextgpfd,
								 &createrexthttp, &createwextgpfd);

	new_record[Anum_pg_authid_rolcreaterextgpfd - 1] = BoolGetDatum(createrextgpfd);
	new_record[Anum_pg_authid_rolcreaterexthttp - 1] = BoolGetDatum(createrexthttp);
	new_record[Anum_pg_authid_rolcreatewextgpfd - 1] = BoolGetDatum(createwextgpfd);

	if (password)
	{
		char	   *shadow_pass;
		char	   *logdetail;

		/*
		 * Don't allow an empty password. Libpq treats an empty password the
		 * same as no password at all, and won't even try to authenticate. But
		 * other clients might, so allowing it would be confusing. By clearing
		 * the password when an empty string is specified, the account is
		 * consistently locked for all clients.
		 *
		 * Note that this only covers passwords stored in the database itself.
		 * There are also checks in the authentication code, to forbid an
		 * empty password from being used with authentication methods that
		 * fetch the password from an external system, like LDAP or PAM.
		 */
		if (password[0] == '\0' ||
			plain_crypt_verify(stmt->role, password, "", &logdetail) == STATUS_OK)
		{
			ereport(NOTICE,
					(errmsg("empty string is not a valid password, clearing password")));
			new_record_nulls[Anum_pg_authid_rolpassword - 1] = true;
		}
		else
		{
			/* Encrypt the password to the requested format. */
			shadow_pass = encrypt_password(Password_encryption, stmt->role,
										   password);
			new_record[Anum_pg_authid_rolpassword - 1] =
				CStringGetTextDatum(shadow_pass);
		}
	}
	else
		new_record_nulls[Anum_pg_authid_rolpassword - 1] = true;

	new_record[Anum_pg_authid_rolvaliduntil - 1] = validUntil_datum;
	new_record_nulls[Anum_pg_authid_rolvaliduntil - 1] = validUntil_null;

	if (resqueue)
	{
		Oid		queueid;

		if (strcmp(resqueue, "none") == 0)
			ereport(ERROR,
					(errcode(ERRCODE_RESERVED_NAME),
					 errmsg("resource queue name \"%s\" is reserved",
							resqueue)));

		queueid = GetResQueueIdForName(resqueue);
		if (queueid == InvalidOid)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("resource queue \"%s\" does not exist",
							resqueue)));

		new_record[Anum_pg_authid_rolresqueue - 1] =
		ObjectIdGetDatum(queueid);

		/*
		 * Don't complain if you CREATE a superuser,
		 * who doesn't use the queue
		 */
		if (!IsResQueueEnabled() && !issuper)
			ereport(WARNING,
					(errmsg("resource queue is disabled"),
					 errhint("To enable set gp_resource_manager=queue")));
	}
	else
	{
		/*
		 * Resource queue required -- use default queue
		 * Don't complain if you CREATE a superuser, who doesn't use the queue
		 */
		new_record[Anum_pg_authid_rolresqueue - 1] = ObjectIdGetDatum(DEFAULTRESQUEUE_OID);

		if (IsResQueueEnabled() && Gp_role == GP_ROLE_DISPATCH && !issuper)
			ereport(NOTICE,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("resource queue required -- using default resource queue \"%s\"",
							GP_DEFAULT_RESOURCE_QUEUE_NAME)));
	}

	if (resgroup)
	{
		Oid			rsgid;

		rsgid = get_resgroup_oid(resgroup, false);
		if (rsgid == ADMINRESGROUP_OID && !issuper)
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("only superuser can be assigned to admin resgroup")));

		ResGroupCheckForRole(rsgid);

		new_record[Anum_pg_authid_rolresgroup - 1] = ObjectIdGetDatum(rsgid);
		if (!IsResGroupActivated() && Gp_role == GP_ROLE_DISPATCH)
			ereport(WARNING,
					(errmsg("resource group is disabled"),
					 errhint("To enable set gp_resource_manager=group")));
	}
	else if (issuper)
	{
		if (IsResGroupActivated() && Gp_role == GP_ROLE_DISPATCH)
		{
			ereport(NOTICE,
					(errmsg("resource group required -- using admin resource group \"admin_group\"")));
		}

		new_record[Anum_pg_authid_rolresgroup - 1] = ObjectIdGetDatum(ADMINRESGROUP_OID);
	}
	else
	{
		if (IsResGroupActivated() && Gp_role == GP_ROLE_DISPATCH)
		{
			ereport(NOTICE,
					(errmsg("resource group required -- using default resource group \"default_group\"")));
		}

		new_record[Anum_pg_authid_rolresgroup - 1] = ObjectIdGetDatum(DEFAULTRESGROUP_OID);
	}

	new_record_nulls[Anum_pg_authid_rolresgroup - 1] = false;

	new_record[Anum_pg_authid_rolbypassrls - 1] = BoolGetDatum(bypassrls);

	/*
	 * pg_largeobject_metadata contains pg_authid.oid's, so we use the
	 * binary-upgrade override.
	 *
	 * GPDB_12_MERGE_FIXME: GetNewOidForAuthId() will return the pre-assigned
	 * OID, if any, but should we do something special here to check and error out
	 * if there was no pre-assigned values in binary upgrade mode.
	 */
#if 0
	if (IsBinaryUpgrade)
	{
		if (!OidIsValid(binary_upgrade_next_pg_authid_oid))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("pg_authid OID value not set when in binary upgrade mode")));

		roleid = binary_upgrade_next_pg_authid_oid;
		binary_upgrade_next_pg_authid_oid = InvalidOid;
	}
	else
#endif
	{
		roleid = GetNewOidForAuthId(pg_authid_rel, AuthIdOidIndexId,
									Anum_pg_authid_oid,
									stmt->role);
	}

	new_record[Anum_pg_authid_oid - 1] = ObjectIdGetDatum(roleid);

	tuple = heap_form_tuple(pg_authid_dsc, new_record, new_record_nulls);

	/*
	 * Insert new record in the pg_authid table
	 */
	CatalogTupleInsert(pg_authid_rel, tuple);

	/*
	 * Advance command counter so we can see new record; else tests in
	 * AddRoleMems may fail.
	 */
	if (addroleto || adminmembers || rolemembers)
		CommandCounterIncrement();

	/*
	 * Add the new role to the specified existing roles.
	 */
	foreach(item, addroleto)
	{
		RoleSpec   *oldrole = lfirst(item);
		HeapTuple	oldroletup = get_rolespec_tuple(oldrole);
		Form_pg_authid oldroleform = (Form_pg_authid) GETSTRUCT(oldroletup);
		Oid			oldroleid = oldroleform->oid;
		char	   *oldrolename = NameStr(oldroleform->rolname);

		AddRoleMems(oldrolename, oldroleid,
					list_make1(makeString(stmt->role)),
					list_make1_oid(roleid),
					GetUserId(), false);

		ReleaseSysCache(oldroletup);
	}

	/*
	 * Add the specified members to this new role. adminmembers get the admin
	 * option, rolemembers don't.
	 */
	AddRoleMems(stmt->role, roleid,
				adminmembers, roleSpecsToIds(adminmembers),
				GetUserId(), true);
	AddRoleMems(stmt->role, roleid,
				rolemembers, roleSpecsToIds(rolemembers),
				GetUserId(), false);

	/* Post creation hook for new role */
	InvokeObjectPostCreateHook(AuthIdRelationId, roleid, 0);

	/*
	 * Populate pg_auth_time_constraint with intervals for which this
	 * particular role should be denied access.
	 */
	if (addintervals)
	{
		if (issuper)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot create superuser with DENY rules")));
		AddRoleDenials(stmt->role, roleid, addintervals);
	}

	/*
	 * Close pg_authid, but keep lock till commit.
	 */
	table_close(pg_authid_rel, NoLock);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		Assert(stmt->type == T_CreateRoleStmt);
		Assert(stmt->type < 1000);
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);

		/* MPP-6929: metadata tracking */
		MetaTrackAddObject(AuthIdRelationId,
						   roleid,
						   GetUserId(),
						   "CREATE", "ROLE");
	}

	return roleid;
}


/*
 * ALTER ROLE
 *
 * Note: the rolemembers option accepted here is intended to support the
 * backwards-compatible ALTER GROUP syntax.  Although it will work to say
 * "ALTER ROLE role ROLE rolenames", we don't document it.
 */
Oid
AlterRole(AlterRoleStmt *stmt)
{
	Datum		new_record[Natts_pg_authid];
	bool		new_record_nulls[Natts_pg_authid];
	bool		new_record_repl[Natts_pg_authid];
	Relation	pg_authid_rel;
	TupleDesc	pg_authid_dsc;
	HeapTuple	tuple,
				new_tuple;
	Form_pg_authid authform;
	ListCell   *option;
	char	   *rolename = NULL;
	char	   *password = NULL;	/* user password */
	int			issuper = -1;	/* Make the user a superuser? */
	int			inherit = -1;	/* Auto inherit privileges? */
	int			createrole = -1;	/* Can this user create roles? */
	int			createdb = -1;	/* Can the user create databases? */
	int			canlogin = -1;	/* Can this user login? */
	int			isreplication = -1; /* Is this a replication role? */
	int			connlimit = -1; /* maximum connections allowed */
	char	   *resqueue = NULL;	/* resource queue for this role */
	char	   *resgroup = NULL;	/* resource group for this role */
	List	   *exttabcreate = NIL;	/* external table create privileges being added  */
	List	   *exttabnocreate = NIL;	/* external table create privileges being removed */
	List	   *rolemembers = NIL;	/* roles to be added/removed */
	char	   *validUntil = NULL;	/* time the login is valid until */
	Datum		validUntil_datum;	/* same, as timestamptz Datum */
	bool		validUntil_null;
	int			bypassrls = -1;
	DefElem    *dpassword = NULL;
	DefElem    *dresqueue = NULL;
	DefElem    *dresgroup = NULL;
	DefElem    *dissuper = NULL;
	DefElem    *dinherit = NULL;
	DefElem    *dcreaterole = NULL;
	DefElem    *dcreatedb = NULL;
	DefElem    *dcanlogin = NULL;
	DefElem    *disreplication = NULL;
	DefElem    *dconnlimit = NULL;
	DefElem    *drolemembers = NULL;
	DefElem    *dvalidUntil = NULL;
	DefElem    *dbypassRLS = NULL;
	Oid			roleid;
	bool		bWas_super = false;	/* Was the user a superuser? */
	int			numopts = 0;
	char	   *alter_subtype = "";	/* metadata tracking: kind of
										   redundant to say "role" */
	bool		createrextgpfd;
	bool 		createrexthttp;
	bool		createwextgpfd;
	List	   *addintervals = NIL;		/* list of time intervals for which login should be denied */
	List	   *dropintervals = NIL;	/* list of time intervals for which matching rules should be dropped */
	Oid			queueid;

	numopts = list_length(stmt->options);

	if (numopts > 1)
	{
		char allopts[NAMEDATALEN];

		sprintf(allopts, "%d OPTIONS", numopts);

		alter_subtype = pstrdup(allopts);
	}
	else if (0 == numopts)
	{
		alter_subtype = "0 OPTIONS";
	}

	check_rolespec_name(stmt->role,
						"Cannot alter reserved roles.");

	/* Extract options from the statement node tree */
	foreach(option, stmt->options)
	{
		DefElem    *defel = (DefElem *) lfirst(option);

		if (strcmp(defel->defname, "password") == 0)
		{
			if (dpassword)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dpassword = defel;
		}
		else if (strcmp(defel->defname, "superuser") == 0)
		{
			if (dissuper)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dissuper = defel;
			if (1 == numopts) alter_subtype = "SUPERUSER";
		}
		else if (strcmp(defel->defname, "inherit") == 0)
		{
			if (dinherit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dinherit = defel;
			if (1 == numopts) alter_subtype = "INHERIT";
		}
		else if (strcmp(defel->defname, "createrole") == 0)
		{
			if (dcreaterole)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dcreaterole = defel;
			if (1 == numopts) alter_subtype = "CREATEROLE";
		}
		else if (strcmp(defel->defname, "createdb") == 0)
		{
			if (dcreatedb)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dcreatedb = defel;
			if (1 == numopts) alter_subtype = "CREATEDB";
		}
		else if (strcmp(defel->defname, "canlogin") == 0)
		{
			if (dcanlogin)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dcanlogin = defel;
			if (1 == numopts) alter_subtype = "LOGIN";
		}
		else if (strcmp(defel->defname, "isreplication") == 0)
		{
			if (disreplication)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			disreplication = defel;
		}
		else if (strcmp(defel->defname, "connectionlimit") == 0)
		{
			if (dconnlimit)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dconnlimit = defel;
			if (1 == numopts) alter_subtype = "CONNECTION LIMIT";
		}
		else if (strcmp(defel->defname, "rolemembers") == 0 &&
				 stmt->action != 0)
		{
			if (drolemembers)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			drolemembers = defel;
			if (1 == numopts) alter_subtype = "ROLE";
		}
		else if (strcmp(defel->defname, "validUntil") == 0)
		{
			if (dvalidUntil)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dvalidUntil = defel;
			if (1 == numopts) alter_subtype = "VALID UNTIL";
		}
		else if (strcmp(defel->defname, "resourceQueue") == 0)
		{
			if (dresqueue)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dresqueue = defel;
			if (1 == numopts) alter_subtype = "RESOURCE QUEUE";
		}
		else if (strcmp(defel->defname, "resourceGroup") == 0)
		{
			if (dresgroup)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dresgroup = defel;
			if (1 == numopts)
				alter_subtype = "RESOURCE GROUP";
		}
		else if (strcmp(defel->defname, "exttabauth") == 0)
		{
			extAuthPair *extauth;

			extauth = TransformExttabAuthClause(defel);

			/* now actually append our transformed key value pairs to the list */
			exttabcreate = lappend(exttabcreate, extauth);

			if (1 == numopts) alter_subtype = "CREATEEXTTABLE";
		}
		else if (strcmp(defel->defname, "exttabnoauth") == 0)
		{
			extAuthPair *extauth;

			extauth = TransformExttabAuthClause(defel);

			/* now actually append our transformed key value pairs to the list */
			exttabnocreate = lappend(exttabnocreate, extauth);

			if (1 == numopts) alter_subtype = "NO CREATEEXTTABLE";
		}
		else if (strcmp(defel->defname, "deny") == 0)
		{
			authInterval *interval = (authInterval *) palloc0(sizeof(authInterval));

			ExtractAuthIntervalClause(defel, interval);

			addintervals = lappend(addintervals, interval);
		}
		else if (strcmp(defel->defname, "drop_deny") == 0)
		{
			authInterval *interval = (authInterval *) palloc0(sizeof(authInterval));

			ExtractAuthIntervalClause(defel, interval);

			dropintervals = lappend(dropintervals, interval);
		}
		else if (strcmp(defel->defname, "bypassrls") == 0)
		{
			if (dbypassRLS)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("conflicting or redundant options")));
			dbypassRLS = defel;
		}
		else
			elog(ERROR, "option \"%s\" not recognized",
				 defel->defname);
	}

	if (dpassword && dpassword->arg)
		password = strVal(dpassword->arg);
	if (dissuper)
		issuper = intVal(dissuper->arg);
	if (dinherit)
		inherit = intVal(dinherit->arg);
	if (dcreaterole)
		createrole = intVal(dcreaterole->arg);
	if (dcreatedb)
		createdb = intVal(dcreatedb->arg);
	if (dcanlogin)
		canlogin = intVal(dcanlogin->arg);
	if (disreplication)
		isreplication = intVal(disreplication->arg);
	if (dconnlimit)
	{
		connlimit = intVal(dconnlimit->arg);
		if (connlimit < -1)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("invalid connection limit: %d", connlimit)));
	}
	if (drolemembers)
		rolemembers = (List *) drolemembers->arg;
	if (dvalidUntil)
		validUntil = strVal(dvalidUntil->arg);
	if (dresqueue)
		resqueue = strVal(linitial((List *) dresqueue->arg));
	if (dresgroup)
		resgroup = strVal(linitial((List *) dresgroup->arg));
	if (dbypassRLS)
		bypassrls = intVal(dbypassRLS->arg);

	/*
	 * Scan the pg_authid relation to be certain the user exists.
	 */
	pg_authid_rel = table_open(AuthIdRelationId, RowExclusiveLock);
	pg_authid_dsc = RelationGetDescr(pg_authid_rel);

	tuple = get_rolespec_tuple(stmt->role);
	authform = (Form_pg_authid) GETSTRUCT(tuple);
	rolename = pstrdup(NameStr(authform->rolname));
	roleid = authform->oid;

	/*
	 * To mess with a superuser you gotta be superuser; else you need
	 * createrole, or just want to change your own password
	 */

	bWas_super = ((Form_pg_authid) GETSTRUCT(tuple))->rolsuper;

	if (authform->rolsuper || issuper >= 0)
	{
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to alter superusers")));
	}
	else if (authform->rolreplication || isreplication >= 0)
	{
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to alter replication users")));
	}
	else if (authform->rolbypassrls || bypassrls >= 0)
	{
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to change bypassrls attribute")));
	}
	else if (!have_createrole_privilege())
	{
		if (!(inherit < 0 &&
			  createrole < 0 &&
			  createdb < 0 &&
			  canlogin < 0 &&
			  isreplication < 0 &&
			  !dconnlimit &&
			  !rolemembers &&
			  !validUntil &&
			  dpassword &&
			  !exttabcreate &&
			  !exttabnocreate &&
			  roleid == GetUserId()))
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("permission denied")));
	}

	/* Convert validuntil to internal form */
	if (validUntil)
	{
		validUntil_datum = DirectFunctionCall3(timestamptz_in,
											   CStringGetDatum(validUntil),
											   ObjectIdGetDatum(InvalidOid),
											   Int32GetDatum(-1));
		validUntil_null = false;
	}
	else
	{
		/* fetch existing setting in case hook needs it */
		validUntil_datum = SysCacheGetAttr(AUTHNAME, tuple,
										   Anum_pg_authid_rolvaliduntil,
										   &validUntil_null);
	}

	/*
	 * Call the password checking hook if there is one defined
	 */
	if (check_password_hook && password)
		(*check_password_hook) (rolename,
								password,
								get_password_type(password),
								validUntil_datum,
								validUntil_null);

	/*
	 * Build an updated tuple, perusing the information just obtained
	 */
	MemSet(new_record, 0, sizeof(new_record));
	MemSet(new_record_nulls, false, sizeof(new_record_nulls));
	MemSet(new_record_repl, false, sizeof(new_record_repl));

	/*
	 * issuper/createrole/etc
	 */
	if (issuper >= 0)
	{
		new_record[Anum_pg_authid_rolsuper - 1] = BoolGetDatum(issuper > 0);
		new_record_repl[Anum_pg_authid_rolsuper - 1] = true;

		/* get current superuser status */
		bWas_super = (issuper > 0);
	}

	if (inherit >= 0)
	{
		new_record[Anum_pg_authid_rolinherit - 1] = BoolGetDatum(inherit > 0);
		new_record_repl[Anum_pg_authid_rolinherit - 1] = true;
	}

	if (createrole >= 0)
	{
		new_record[Anum_pg_authid_rolcreaterole - 1] = BoolGetDatum(createrole > 0);
		new_record_repl[Anum_pg_authid_rolcreaterole - 1] = true;
	}

	if (createdb >= 0)
	{
		new_record[Anum_pg_authid_rolcreatedb - 1] = BoolGetDatum(createdb > 0);
		new_record_repl[Anum_pg_authid_rolcreatedb - 1] = true;
	}

	if (canlogin >= 0)
	{
		new_record[Anum_pg_authid_rolcanlogin - 1] = BoolGetDatum(canlogin > 0);
		new_record_repl[Anum_pg_authid_rolcanlogin - 1] = true;
	}

	if (isreplication >= 0)
	{
		new_record[Anum_pg_authid_rolreplication - 1] = BoolGetDatum(isreplication > 0);
		new_record_repl[Anum_pg_authid_rolreplication - 1] = true;
	}

	if (dconnlimit)
	{
		new_record[Anum_pg_authid_rolconnlimit - 1] = Int32GetDatum(connlimit);
		new_record_repl[Anum_pg_authid_rolconnlimit - 1] = true;
	}

	/* password */
	if (password)
	{
		char	   *shadow_pass;
		char	   *logdetail;

		/* Like in CREATE USER, don't allow an empty password. */
		if (password[0] == '\0' ||
			plain_crypt_verify(rolename, password, "", &logdetail) == STATUS_OK)
		{
			ereport(NOTICE,
					(errmsg("empty string is not a valid password, clearing password")));
			new_record_nulls[Anum_pg_authid_rolpassword - 1] = true;
		}
		else
		{
			/* Encrypt the password to the requested format. */
			shadow_pass = encrypt_password(Password_encryption, rolename,
										   password);
			new_record[Anum_pg_authid_rolpassword - 1] =
				CStringGetTextDatum(shadow_pass);
		}
		new_record_repl[Anum_pg_authid_rolpassword - 1] = true;
	}

	/* unset password */
	if (dpassword && dpassword->arg == NULL)
	{
		new_record_repl[Anum_pg_authid_rolpassword - 1] = true;
		new_record_nulls[Anum_pg_authid_rolpassword - 1] = true;
	}

	/* valid until */
	new_record[Anum_pg_authid_rolvaliduntil - 1] = validUntil_datum;
	new_record_nulls[Anum_pg_authid_rolvaliduntil - 1] = validUntil_null;
	new_record_repl[Anum_pg_authid_rolvaliduntil - 1] = true;

	/* Set the CREATE EXTERNAL TABLE permissions for this role, if specified in ALTER */
	if (exttabcreate || exttabnocreate)
	{
		bool	isnull;
		Datum 	dcreaterextgpfd;
		Datum 	dcreaterexthttp;
		Datum 	dcreatewextgpfd;

		/*
		 * get bool values from catalog. we don't ever expect a NULL value, but just
		 * in case it is there (perhaps after an upgrade) we treat it as 'false'.
		 */
		dcreaterextgpfd = heap_getattr(tuple, Anum_pg_authid_rolcreaterextgpfd, pg_authid_dsc, &isnull);
		createrextgpfd = (isnull ? false : DatumGetBool(dcreaterextgpfd));
		dcreaterexthttp = heap_getattr(tuple, Anum_pg_authid_rolcreaterexthttp, pg_authid_dsc, &isnull);
		createrexthttp = (isnull ? false : DatumGetBool(dcreaterexthttp));
		dcreatewextgpfd = heap_getattr(tuple, Anum_pg_authid_rolcreatewextgpfd, pg_authid_dsc, &isnull);
		createwextgpfd = (isnull ? false : DatumGetBool(dcreatewextgpfd));

		SetCreateExtTableForRole(exttabcreate, exttabnocreate, &createrextgpfd,
								 &createrexthttp, &createwextgpfd);

		new_record[Anum_pg_authid_rolcreaterextgpfd - 1] = BoolGetDatum(createrextgpfd);
		new_record_repl[Anum_pg_authid_rolcreaterextgpfd - 1] = true;
		new_record[Anum_pg_authid_rolcreaterexthttp - 1] = BoolGetDatum(createrexthttp);
		new_record_repl[Anum_pg_authid_rolcreaterexthttp - 1] = true;
		new_record[Anum_pg_authid_rolcreatewextgpfd - 1] = BoolGetDatum(createwextgpfd);
		new_record_repl[Anum_pg_authid_rolcreatewextgpfd - 1] = true;
	}

	/* resource queue */
	if (resqueue)
	{
		/* NONE not supported -- use default queue  */
		if (strcmp(resqueue, "none") == 0)
		{
			/*
			 * Don't complain if you ALTER a superuser, who doesn't use the
			 * queue
			 */
			if (!bWas_super && IsResQueueEnabled() && Gp_role == GP_ROLE_DISPATCH)
			{
				ereport(NOTICE,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("resource queue required -- using default resource queue \"%s\"",
								GP_DEFAULT_RESOURCE_QUEUE_NAME)));
			}

			resqueue = pstrdup(GP_DEFAULT_RESOURCE_QUEUE_NAME);
		}

		queueid = GetResQueueIdForName(resqueue);
		if (queueid == InvalidOid)
			ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("resource queue \"%s\" does not exist", resqueue)));

		new_record[Anum_pg_authid_rolresqueue - 1] = ObjectIdGetDatum(queueid);
		new_record_repl[Anum_pg_authid_rolresqueue - 1] = true;

		if (!IsResQueueEnabled() && !bWas_super)
		{
			/*
			 * Don't complain if you ALTER a superuser, who doesn't use the
			 * queue
			 */
			ereport(WARNING,
					(errmsg("resource queue is disabled"),
					 errhint("To enable set gp_resource_manager=queue.")));
		}
	}

	/* resource group */
	if (resgroup)
	{
		Oid			rsgid;

		if (strcmp(resgroup, "none") == 0)
		{
			if (bWas_super)
				resgroup = pstrdup("admin_group");
			else
				resgroup = pstrdup("default_group");

			if (IsResGroupActivated() && Gp_role == GP_ROLE_DISPATCH)
				ereport(NOTICE,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("resource group required -- "
								"using default resource group \"%s\"",
								resgroup)));
		}

		rsgid = get_resgroup_oid(resgroup, false);
		if (rsgid == ADMINRESGROUP_OID && !bWas_super)
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("only superuser can be assigned to admin resgroup")));
		ResGroupCheckForRole(rsgid);
		new_record[Anum_pg_authid_rolresgroup - 1] =
			ObjectIdGetDatum(rsgid);
		new_record_repl[Anum_pg_authid_rolresgroup - 1] = true;

		if (!IsResGroupActivated() && Gp_role == GP_ROLE_DISPATCH)
		{
			ereport(WARNING,
					(errmsg("resource group is disabled"),
					 errhint("To enable set gp_resource_manager=group")));
		}
	}

	if (bypassrls >= 0)
	{
		new_record[Anum_pg_authid_rolbypassrls - 1] = BoolGetDatum(bypassrls > 0);
		new_record_repl[Anum_pg_authid_rolbypassrls - 1] = true;
	}

	new_tuple = heap_modify_tuple(tuple, pg_authid_dsc, new_record,
								  new_record_nulls, new_record_repl);
	CatalogTupleUpdate(pg_authid_rel, &tuple->t_self, new_tuple);

	InvokeObjectPostAlterHook(AuthIdRelationId, roleid, 0);

	ReleaseSysCache(tuple);
	heap_freetuple(new_tuple);

	/*
	 * Advance command counter so we can see new record; else tests in
	 * AddRoleMems may fail.
	 */
	if (rolemembers)
		CommandCounterIncrement();

	if (stmt->action == +1)		/* add members to role */
	{
		if (rolemembers)
			alter_subtype = "ADD USER";

		AddRoleMems(rolename, roleid,
					rolemembers, roleSpecsToIds(rolemembers),
					GetUserId(), false);
	}
	else if (stmt->action == -1)	/* drop members from role */
	{
		if (rolemembers)
			alter_subtype = "DROP USER";

		DelRoleMems(rolename, roleid,
					rolemembers, roleSpecsToIds(rolemembers),
					false);
	}

	if (bWas_super)
	{
		if (addintervals)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot alter superuser with DENY rules")));
		else
			DelRoleDenials(rolename, roleid, NIL);	/* drop all preexisting constraints, if any. */
	}

	/*
	 * Disallow the use of DENY and DROP DENY fragments in the same query.
	 *
	 * We do this to prevent commands with unusual behavior.
	 * e.g. consider "ALTER ROLE foo DENY DAY 0 DROP DENY FOR DAY 1 DENY DAY 1 DENY DAY 2"
	 * In the manner that this is currently coded, because all DENY fragments are interpreted
	 * first, this actually becomes equivalent to you "ALTER ROLE foo DENY DAY 0 DENY DAY 2".
	 *
	 * Instead, we could honor the order in which the fragments are presented, but still that
	 * allows users to contradict themselves, as in the example given.
	 */
	if (addintervals && dropintervals)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("conflicting or redundant options"),
				 errhint("DENY and DROP DENY cannot be used in the same ALTER ROLE statement.")));

	/*
	 * Populate pg_auth_time_constraint with the new intervals for which this
	 * particular role should be denied access.
	 */
	if (addintervals)
		AddRoleDenials(rolename, roleid, addintervals);

	/*
	 * Remove pg_auth_time_constraint entries that overlap with the
	 * intervals given by the user.
	 */
	if (dropintervals)
		DelRoleDenials(rolename, roleid, dropintervals);

	/* MPP-6929: metadata tracking */
	if (Gp_role == GP_ROLE_DISPATCH)
		MetaTrackUpdObject(AuthIdRelationId,
						   roleid,
						   GetUserId(),
						   "ALTER", alter_subtype);

	/*
	 * Close pg_authid, but keep lock till commit.
	 */
	table_close(pg_authid_rel, NoLock);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL,
									NULL);
	}

	return roleid;
}


/*
 * ALTER ROLE ... SET
 */
Oid
AlterRoleSet(AlterRoleSetStmt *stmt)
{
	HeapTuple	roletuple;
	Form_pg_authid roleform;
	Oid			databaseid = InvalidOid;
	Oid			roleid = InvalidOid;

	if (stmt->role)
	{
		check_rolespec_name(stmt->role,
							"Cannot alter reserved roles.");

		roletuple = get_rolespec_tuple(stmt->role);
		roleform = (Form_pg_authid) GETSTRUCT(roletuple);
		roleid = roleform->oid;

		/*
		 * Obtain a lock on the role and make sure it didn't go away in the
		 * meantime.
		 */
		shdepLockAndCheckObject(AuthIdRelationId, roleid);

		/*
		 * To mess with a superuser you gotta be superuser; else you need
		 * createrole, or just want to change your own settings
		 */
		if (roleform->rolsuper)
		{
			if (!superuser())
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						 errmsg("must be superuser to alter superusers")));
		}
		else
		{
			if (!have_createrole_privilege() && roleid != GetUserId())
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						 errmsg("permission denied")));
		}

		ReleaseSysCache(roletuple);
	}

	/* look up and lock the database, if specified */
	if (stmt->database != NULL)
	{
		databaseid = get_database_oid(stmt->database, false);
		shdepLockAndCheckObject(DatabaseRelationId, databaseid);

		if (!stmt->role)
		{
			/*
			 * If no role is specified, then this is effectively the same as
			 * ALTER DATABASE ... SET, so use the same permission check.
			 */
			if (!pg_database_ownercheck(databaseid, GetUserId()))
				aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_DATABASE,
							   stmt->database);
		}
	}

	if (!stmt->role && !stmt->database)
	{
		/* Must be superuser to alter settings globally. */
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to alter settings globally")));
	}

	AlterSetting(databaseid, roleid, stmt->setstmt);

	return roleid;
}


/*
 * DROP ROLE
 */
void
DropRole(DropRoleStmt *stmt)
{
	Relation	pg_authid_rel,
				pg_auth_members_rel;
	ListCell   *item;

	if (!have_createrole_privilege())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied to drop role")));

	/*
	 * Scan the pg_authid relation to find the Oid of the role(s) to be
	 * deleted.
	 */
	pg_authid_rel = table_open(AuthIdRelationId, RowExclusiveLock);
	pg_auth_members_rel = table_open(AuthMemRelationId, RowExclusiveLock);

	foreach(item, stmt->roles)
	{
		RoleSpec   *rolspec = lfirst(item);
		char	   *role;
		HeapTuple	tuple,
					tmp_tuple;
		Form_pg_authid roleform;
		ScanKeyData scankey;
		char	   *detail;
		char	   *detail_log;
		SysScanDesc sscan;
		Oid			roleid;

		if (rolspec->roletype != ROLESPEC_CSTRING)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("cannot use special role specifier in DROP ROLE")));
		role = rolspec->rolename;

		tuple = SearchSysCache1(AUTHNAME, PointerGetDatum(role));
		if (!HeapTupleIsValid(tuple))
		{
			if (!stmt->missing_ok)
			{
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("role \"%s\" does not exist", role)));
			}
			if (Gp_role != GP_ROLE_EXECUTE)
			{
				ereport(NOTICE,
						(errmsg("role \"%s\" does not exist, skipping",
								role)));
			}

			continue;
		}

		roleform = (Form_pg_authid) GETSTRUCT(tuple);
		roleid = roleform->oid;

		if (roleid == GetUserId())
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_IN_USE),
					 errmsg("current user cannot be dropped")));
		if (roleid == GetOuterUserId())
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_IN_USE),
					 errmsg("current user cannot be dropped")));
		if (roleid == GetSessionUserId())
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_IN_USE),
					 errmsg("session user cannot be dropped")));

		/*
		 * For safety's sake, we allow createrole holders to drop ordinary
		 * roles but not superuser roles.  This is mainly to avoid the
		 * scenario where you accidentally drop the last superuser.
		 */
		if (roleform->rolsuper && !superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to drop superusers")));

		/* DROP hook for the role being removed */
		InvokeObjectDropHook(AuthIdRelationId, roleid, 0);

		/*
		 * Lock the role, so nobody can add dependencies to her while we drop
		 * her.  We keep the lock until the end of transaction.
		 */
		LockSharedObject(AuthIdRelationId, roleid, 0, AccessExclusiveLock);

		/* Check for pg_shdepend entries depending on this role */
		if (checkSharedDependencies(AuthIdRelationId, roleid,
									&detail, &detail_log))
			ereport(ERROR,
					(errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST),
					 errmsg("role \"%s\" cannot be dropped because some objects depend on it",
							role),
					 errdetail_internal("%s", detail),
					 errdetail_log("%s", detail_log)));

		/*
		 * Remove the role from the pg_authid table
		 */
		CatalogTupleDelete(pg_authid_rel, &tuple->t_self);

		ReleaseSysCache(tuple);

		/*
		 * Remove role from the pg_auth_members table.  We have to remove all
		 * tuples that show it as either a role or a member.
		 *
		 * XXX what about grantor entries?	Maybe we should do one heap scan.
		 */
		ScanKeyInit(&scankey,
					Anum_pg_auth_members_roleid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(roleid));

		sscan = systable_beginscan(pg_auth_members_rel, AuthMemRoleMemIndexId,
								   true, NULL, 1, &scankey);

		while (HeapTupleIsValid(tmp_tuple = systable_getnext(sscan)))
		{
			CatalogTupleDelete(pg_auth_members_rel, &tmp_tuple->t_self);
		}

		systable_endscan(sscan);

		ScanKeyInit(&scankey,
					Anum_pg_auth_members_member,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(roleid));

		sscan = systable_beginscan(pg_auth_members_rel, AuthMemMemRoleIndexId,
								   true, NULL, 1, &scankey);

		while (HeapTupleIsValid(tmp_tuple = systable_getnext(sscan)))
		{
			CatalogTupleDelete(pg_auth_members_rel, &tmp_tuple->t_self);
		}

		systable_endscan(sscan);

		/*
		 * Remove any time constraints on this role.
		 */
		DelRoleDenials(role, roleid, NIL);

		/*
		 * Remove any comments or security labels on this role.
		 */
		DeleteSharedComments(roleid, AuthIdRelationId);
		DeleteSharedSecurityLabel(roleid, AuthIdRelationId);

		/* MPP-6929: metadata tracking */
		if (Gp_role == GP_ROLE_DISPATCH)
			MetaTrackDropObject(AuthIdRelationId,
								roleid);
		/*
		 * Remove settings for this role.
		 */
		DropSetting(InvalidOid, roleid);

		/*
		 * Advance command counter so that later iterations of this loop will
		 * see the changes already made.  This is essential if, for example,
		 * we are trying to drop both a role and one of its direct members ---
		 * we'll get an error if we try to delete the linking pg_auth_members
		 * tuple twice.  (We do not need a CCI between the two delete loops
		 * above, because it's not allowed for a role to directly contain
		 * itself.)
		 */
		CommandCounterIncrement();
	}

	/*
	 * Now we can clean up; but keep locks until commit.
	 */
	table_close(pg_auth_members_rel, NoLock);
	table_close(pg_authid_rel, NoLock);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL,
									NULL);

	}
}

/*
 * Rename role
 */
ObjectAddress
RenameRole(const char *oldname, const char *newname)
{
	HeapTuple	oldtuple,
				newtuple;
	TupleDesc	dsc;
	Relation	rel;
	Datum		datum;
	bool		isnull;
	Datum		repl_val[Natts_pg_authid];
	bool		repl_null[Natts_pg_authid];
	bool		repl_repl[Natts_pg_authid];
	int			i;
	Oid			roleid;
	ObjectAddress address;
	Form_pg_authid authform;

	rel = table_open(AuthIdRelationId, RowExclusiveLock);
	dsc = RelationGetDescr(rel);

	oldtuple = SearchSysCache1(AUTHNAME, CStringGetDatum(oldname));
	if (!HeapTupleIsValid(oldtuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("role \"%s\" does not exist", oldname)));

	/*
	 * XXX Client applications probably store the session user somewhere, so
	 * renaming it could cause confusion.  On the other hand, there may not be
	 * an actual problem besides a little confusion, so think about this and
	 * decide.  Same for SET ROLE ... we don't restrict renaming the current
	 * effective userid, though.
	 */

	authform = (Form_pg_authid) GETSTRUCT(oldtuple);
	roleid = authform->oid;

	if (roleid == GetSessionUserId())
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("session user cannot be renamed")));
	if (roleid == GetOuterUserId())
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("current user cannot be renamed")));

	/*
	 * Check that the user is not trying to rename a system role and not
	 * trying to rename a role into the reserved "pg_" namespace.
	 */
	if (IsReservedName(NameStr(authform->rolname)))
		ereport(ERROR,
				(errcode(ERRCODE_RESERVED_NAME),
				 errmsg("role name \"%s\" is reserved",
						NameStr(authform->rolname)),
				 errdetail("Role names starting with \"pg_\" are reserved.")));

	if (IsReservedName(newname))
		ereport(ERROR,
				(errcode(ERRCODE_RESERVED_NAME),
				 errmsg("role name \"%s\" is reserved",
						newname),
				 errdetail("Role names starting with \"pg_\" are reserved.")));

	/*
	 * If built with appropriate switch, whine when regression-testing
	 * conventions for role names are violated.
	 */
#ifdef ENFORCE_REGRESSION_TEST_NAME_RESTRICTIONS
	if (strncmp(newname, "regress_", 8) != 0)
		elog(WARNING, "roles created by regression test cases should have names starting with \"regress_\"");
#endif

	/* make sure the new name doesn't exist */
	if (SearchSysCacheExists1(AUTHNAME, CStringGetDatum(newname)))
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("role \"%s\" already exists", newname)));

	/*
	 * createrole is enough privilege unless you want to mess with a superuser
	 */
	if (((Form_pg_authid) GETSTRUCT(oldtuple))->rolsuper)
	{
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to rename superusers")));
	}
	else
	{
		if (!have_createrole_privilege())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("permission denied to rename role")));
	}

	/* OK, construct the modified tuple */
	for (i = 0; i < Natts_pg_authid; i++)
		repl_repl[i] = false;

	repl_repl[Anum_pg_authid_rolname - 1] = true;
	repl_val[Anum_pg_authid_rolname - 1] = DirectFunctionCall1(namein,
															   CStringGetDatum(newname));
	repl_null[Anum_pg_authid_rolname - 1] = false;

	datum = heap_getattr(oldtuple, Anum_pg_authid_rolpassword, dsc, &isnull);

	if (!isnull && get_password_type(TextDatumGetCString(datum)) == PASSWORD_TYPE_MD5)
	{
		/* MD5 uses the username as salt, so just clear it on a rename */
		repl_repl[Anum_pg_authid_rolpassword - 1] = true;
		repl_null[Anum_pg_authid_rolpassword - 1] = true;

		if (Gp_role != GP_ROLE_EXECUTE)
		ereport(NOTICE,
				(errmsg("MD5 password cleared because of role rename")));
	}

	newtuple = heap_modify_tuple(oldtuple, dsc, repl_val, repl_null, repl_repl);
	CatalogTupleUpdate(rel, &oldtuple->t_self, newtuple);

	InvokeObjectPostAlterHook(AuthIdRelationId, roleid, 0);

	ObjectAddressSet(address, AuthIdRelationId, roleid);

	ReleaseSysCache(oldtuple);

	/*
	 * Close pg_authid, but keep lock till commit.
	 */
	table_close(rel, NoLock);

	/* MPP-6929: metadata tracking */
	if (Gp_role == GP_ROLE_DISPATCH)
		MetaTrackUpdObject(AuthIdRelationId,
						   roleid,
						   GetUserId(),
						   "ALTER", "RENAME"
				);

	return address;
}

/*
 * GrantRoleStmt
 *
 * Grant/Revoke roles to/from roles
 */
void
GrantRole(GrantRoleStmt *stmt)
{
	Relation	pg_authid_rel;
	Oid			grantor;
	List	   *grantee_ids;
	ListCell   *item;

	if (stmt->grantor)
		grantor = get_rolespec_oid(stmt->grantor, false);
	else
		grantor = GetUserId();

	grantee_ids = roleSpecsToIds(stmt->grantee_roles);

	/* AccessShareLock is enough since we aren't modifying pg_authid */
	pg_authid_rel = table_open(AuthIdRelationId, AccessShareLock);

	/*
	 * Step through all of the granted roles and add/remove entries for the
	 * grantees, or, if admin_opt is set, then just add/remove the admin
	 * option.
	 *
	 * Note: Permissions checking is done by AddRoleMems/DelRoleMems
	 */
	foreach(item, stmt->granted_roles)
	{
		AccessPriv *priv = (AccessPriv *) lfirst(item);
		char	   *rolename = priv->priv_name;
		Oid			roleid;

		/* Must reject priv(columns) and ALL PRIVILEGES(columns) */
		if (rolename == NULL || priv->cols != NIL)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_GRANT_OPERATION),
					 errmsg("column names cannot be included in GRANT/REVOKE ROLE")));

		roleid = get_role_oid(rolename, false);
		if (stmt->is_grant)
			AddRoleMems(rolename, roleid,
						stmt->grantee_roles, grantee_ids,
						grantor, stmt->admin_opt);
		else
			DelRoleMems(rolename, roleid,
						stmt->grantee_roles, grantee_ids,
						stmt->admin_opt);

		/* MPP-6929: metadata tracking */
		if (Gp_role == GP_ROLE_DISPATCH)
				MetaTrackUpdObject(AuthIdRelationId,
								   roleid,
								   GetUserId(),
								   "PRIVILEGE",
								   (stmt->is_grant) ? "GRANT" : "REVOKE"
						);

	}

	/*
	 * Close pg_authid, but keep lock till commit.
	 */
	table_close(pg_authid_rel, NoLock);

    if (Gp_role == GP_ROLE_DISPATCH)
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL,
									NULL);
}

/*
 * DropOwnedObjects
 *
 * Drop the objects owned by a given list of roles.
 */
void
DropOwnedObjects(DropOwnedStmt *stmt)
{
	List	   *role_ids = roleSpecsToIds(stmt->roles);
	ListCell   *cell;

	/* Check privileges */
	foreach(cell, role_ids)
	{
		Oid			roleid = lfirst_oid(cell);

		if (!has_privs_of_role(GetUserId(), roleid))
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("permission denied to drop objects")));
	}

	if (Gp_role == GP_ROLE_DISPATCH)
    {
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL,
									NULL);
    }

	/* Ok, do it */
	shdepDropOwned(role_ids, stmt->behavior);
}

/*
 * ReassignOwnedObjects
 *
 * Give the objects owned by a given list of roles away to another user.
 */
void
ReassignOwnedObjects(ReassignOwnedStmt *stmt)
{
	List	   *role_ids = roleSpecsToIds(stmt->roles);
	ListCell   *cell;
	Oid			newrole;

	/* Check privileges */
	foreach(cell, role_ids)
	{
		Oid			roleid = lfirst_oid(cell);

		if (!has_privs_of_role(GetUserId(), roleid))
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("permission denied to reassign objects")));
	}

	/* Must have privileges on the receiving side too */
	newrole = get_rolespec_oid(stmt->newrole, false);

	if (!has_privs_of_role(GetUserId(), newrole))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("permission denied to reassign objects")));

	if (Gp_role == GP_ROLE_DISPATCH)
    {
		CdbDispatchUtilityStatement((Node *) stmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									NIL,
									NULL);
    }

	/* Ok, do it */
	shdepReassignOwned(role_ids, newrole);
}

/*
 * roleSpecsToIds
 *
 * Given a list of RoleSpecs, generate a list of role OIDs in the same order.
 *
 * ROLESPEC_PUBLIC is not allowed.
 */
List *
roleSpecsToIds(List *memberNames)
{
	List	   *result = NIL;
	ListCell   *l;

	foreach(l, memberNames)
	{
		RoleSpec   *rolespec = lfirst_node(RoleSpec, l);
		Oid			roleid;

		roleid = get_rolespec_oid(rolespec, false);
		result = lappend_oid(result, roleid);
	}
	return result;
}

/*
 * AddRoleMems -- Add given members to the specified role
 *
 * rolename: name of role to add to (used only for error messages)
 * roleid: OID of role to add to
 * memberSpecs: list of RoleSpec of roles to add (used only for error messages)
 * memberIds: OIDs of roles to add
 * grantorId: who is granting the membership
 * admin_opt: granting admin option?
 *
 * Note: caller is responsible for calling auth_file_update_needed().
 */
static void
AddRoleMems(const char *rolename, Oid roleid,
			List *memberSpecs, List *memberIds,
			Oid grantorId, bool admin_opt)
{
	Relation	pg_authmem_rel;
	TupleDesc	pg_authmem_dsc;
	ListCell   *specitem;
	ListCell   *iditem;

	Assert(list_length(memberSpecs) == list_length(memberIds));

	/* Skip permission check if nothing to do */
	if (!memberIds)
		return;

	/*
	 * Check permissions: must have createrole or admin option on the role to
	 * be changed.  To mess with a superuser role, you gotta be superuser.
	 */
	if (superuser_arg(roleid))
	{
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to alter superusers")));
	}
	else
	{
		if (!have_createrole_privilege() &&
			!is_admin_of_role(grantorId, roleid))
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must have admin option on role \"%s\"",
							rolename)));
	}

	/*
	 * The role membership grantor of record has little significance at
	 * present.  Nonetheless, inasmuch as users might look to it for a crude
	 * audit trail, let only superusers impute the grant to a third party.
	 *
	 * Before lifting this restriction, give the member == role case of
	 * is_admin_of_role() a fresh look.  Ensure that the current role cannot
	 * use an explicit grantor specification to take advantage of the session
	 * user's self-admin right.
	 */
	if (grantorId != GetUserId() && !superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to set grantor")));

	pg_authmem_rel = table_open(AuthMemRelationId, RowExclusiveLock);
	pg_authmem_dsc = RelationGetDescr(pg_authmem_rel);

	forboth(specitem, memberSpecs, iditem, memberIds)
	{
		RoleSpec   *memberRole = lfirst(specitem);
		Oid			memberid = lfirst_oid(iditem);
		HeapTuple	authmem_tuple;
		HeapTuple	tuple;
		Datum		new_record[Natts_pg_auth_members];
		bool		new_record_nulls[Natts_pg_auth_members];
		bool		new_record_repl[Natts_pg_auth_members];

		/*
		 * Refuse creation of membership loops, including the trivial case
		 * where a role is made a member of itself.  We do this by checking to
		 * see if the target role is already a member of the proposed member
		 * role.  We have to ignore possible superuserness, however, else we
		 * could never grant membership in a superuser-privileged role.
		 */
		if (is_member_of_role_nosuper(roleid, memberid))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_GRANT_OPERATION),
					 (errmsg("role \"%s\" is a member of role \"%s\"",
							 rolename, get_rolespec_name(memberRole)))));

		/*
		 * Check if entry for this role/member already exists; if so, give
		 * warning unless we are adding admin option.
		 */
		authmem_tuple = SearchSysCache2(AUTHMEMROLEMEM,
										ObjectIdGetDatum(roleid),
										ObjectIdGetDatum(memberid));
		if (HeapTupleIsValid(authmem_tuple) &&
			(!admin_opt ||
			 ((Form_pg_auth_members) GETSTRUCT(authmem_tuple))->admin_option))
		{
			if (Gp_role != GP_ROLE_EXECUTE)
			ereport(NOTICE,
					(errmsg("role \"%s\" is already a member of role \"%s\"",
							get_rolespec_name(memberRole), rolename)));
			ReleaseSysCache(authmem_tuple);
			continue;
		}

		/* Build a tuple to insert or update */
		MemSet(new_record, 0, sizeof(new_record));
		MemSet(new_record_nulls, false, sizeof(new_record_nulls));
		MemSet(new_record_repl, false, sizeof(new_record_repl));

		new_record[Anum_pg_auth_members_roleid - 1] = ObjectIdGetDatum(roleid);
		new_record[Anum_pg_auth_members_member - 1] = ObjectIdGetDatum(memberid);
		new_record[Anum_pg_auth_members_grantor - 1] = ObjectIdGetDatum(grantorId);
		new_record[Anum_pg_auth_members_admin_option - 1] = BoolGetDatum(admin_opt);

		if (HeapTupleIsValid(authmem_tuple))
		{
			new_record_repl[Anum_pg_auth_members_grantor - 1] = true;
			new_record_repl[Anum_pg_auth_members_admin_option - 1] = true;
			tuple = heap_modify_tuple(authmem_tuple, pg_authmem_dsc,
									  new_record,
									  new_record_nulls, new_record_repl);
			CatalogTupleUpdate(pg_authmem_rel, &tuple->t_self, tuple);
			ReleaseSysCache(authmem_tuple);
		}
		else
		{
			tuple = heap_form_tuple(pg_authmem_dsc,
									new_record, new_record_nulls);
			CatalogTupleInsert(pg_authmem_rel, tuple);
		}

		/* CCI after each change, in case there are duplicates in list */
		CommandCounterIncrement();
	}

	/*
	 * Close pg_authmem, but keep lock till commit.
	 */
	table_close(pg_authmem_rel, NoLock);
}

/*
 * CheckKeywordIsValid
 *
 * check that string in 'keyword' is included in set of strings in 'arr'
 */
static void CheckKeywordIsValid(char *keyword, const char **arr, const int arrsize)
{
	int 	i = 0;
	bool	ok = false;

	for(i = 0 ; i < arrsize ; i++)
	{
		if(strcasecmp(keyword, arr[i]) == 0)
			ok = true;
	}

	if(!ok)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("invalid [NO]CREATEEXTTABLE option \"%s\"", keyword)));

}

/*
 * CheckValueBelongsToKey
 *
 * check that value (e.g 'gpfdist') belogs to the key it was defined for (e.g 'protocol').
 * error out otherwise (for example, [protocol='writable'] includes valid keywords, but makes
 * no sense.
 */
static void CheckValueBelongsToKey(char *key, char *val, const char **keys, const char **vals)
{
	if(strcasecmp(key, keys[0]) == 0)
	{
		if(strcasecmp(val, vals[0]) != 0 &&
		   strcasecmp(val, vals[1]) != 0)

			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("invalid %s value \"%s\"", key, val)));
	}
	else /* keys[1] */
	{
		if(strcasecmp(val, "gpfdist") != 0 &&
		   strcasecmp(val, "gpfdists") != 0 &&
		   strcasecmp(val, "http") != 0)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("invalid %s value \"%s\"", key, val)));
	}

}

/*
 * TransformExttabAuthClause
 *
 * Given a set of key value pairs, take them apart, fill in any default
 * values, and validate that pairs are legal and make sense.
 *
 * defaults are:
 *   - 'readable' when no type defined,
 *   - 'gpfdist' when no protocol defined,
 *   - 'readable' + ' gpfdist' if both type and protocol aren't defined.
 *
 */
static extAuthPair *
TransformExttabAuthClause(DefElem *defel)
{
	List	   	*l = (List *) defel->arg;
	DefElem 	*d1,
				*d2;
	struct
	{
		char	   *key1;
		char	   *val1;
		char	   *key2;
		char	   *val2;
	} genpair;

	const int	numkeys = 2;
	const int	numvals = 5;
	const char *keys[] = { "type", "protocol"};	 /* order matters for validation. don't change! */
	const char *vals[] = { /* types     */ "readable", "writable",
						   /* protocols */ "gpfdist", "gpfdists" , "http"};
	extAuthPair *result;

	if(list_length(l) > 2)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("invalid [NO]CREATEEXTTABLE specification. too many values")));

	if(list_length(l) == 2)
	{
		/* both a protocol and type specification */

		d1 = (DefElem *) linitial(l);
		genpair.key1 = pstrdup(d1->defname);
		genpair.val1 = pstrdup(strVal(d1->arg));

		d2 = (DefElem *) lsecond(l);
		genpair.key2 = pstrdup(d2->defname);
		genpair.val2 = pstrdup(strVal(d2->arg));
	}
	else if(list_length(l) == 1)
	{
		/* either a protocol or type specification */

		d1 = (DefElem *) linitial(l);
		genpair.key1 = pstrdup(d1->defname);
		genpair.val1 = pstrdup(strVal(d1->arg));

		if(strcasecmp(genpair.key1, "type") == 0)
		{
			/* default value for missing protocol */
			genpair.key2 = pstrdup("protocol");
			genpair.val2 = pstrdup("gpfdist");
		}
		else
		{
			/* default value for missing type */
			genpair.key2 = pstrdup("type");
			genpair.val2 = pstrdup("readable");
		}
	}
	else
	{
		/* none specified. use global default */

		genpair.key1 = pstrdup("protocol");
		genpair.val1 = pstrdup("gpfdist");
		genpair.key2 = pstrdup("type");
		genpair.val2 = pstrdup("readable");
	}

	/* check all keys and values are legal */
	CheckKeywordIsValid(genpair.key1, keys, numkeys);
	CheckKeywordIsValid(genpair.key2, keys, numkeys);
	CheckKeywordIsValid(genpair.val1, vals, numvals);
	CheckKeywordIsValid(genpair.val2, vals, numvals);

	/* check all values are of the proper key */
	CheckValueBelongsToKey(genpair.key1, genpair.val1, keys, vals);
	CheckValueBelongsToKey(genpair.key2, genpair.val2, keys, vals);

	if (strcasecmp(genpair.key1, genpair.key2) == 0)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("redundant option for \"%s\"", genpair.key1)));

	/* now create the result struct */
	result = (extAuthPair *) palloc(sizeof(extAuthPair));
	if (strcasecmp(genpair.key1, "protocol") == 0)
	{
		result->protocol = pstrdup(genpair.val1);
		result->type = pstrdup(genpair.val2);
	}
	else
	{
		result->protocol = pstrdup(genpair.val2);
		result->type = pstrdup(genpair.val1);
	}

	pfree(genpair.key1);
	pfree(genpair.key2);
	pfree(genpair.val1);
	pfree(genpair.val2);

	return result;
}

/*
 * SetCreateExtTableForRole
 *
 * Given the allow list (permissions to add) and disallow (permissions
 * to take away) consolidate this information into the 3 catalog
 * boolean columns that will need to get updated. While at it we check
 * that all the options are valid and don't conflict with each other.
 *
 */
static void SetCreateExtTableForRole(List* allow,
									 List* disallow,
									 bool* createrextgpfd,
									 bool* createrexthttp,
									 bool* createwextgpfd)
{
	ListCell*	lc;
	bool		createrextgpfd_specified = false;
	bool		createwextgpfd_specified = false;
	bool		createrexthttp_specified = false;

	if(list_length(allow) > 0)
	{
		/* examine key value pairs */
		foreach(lc, allow)
		{
			extAuthPair* extauth = (extAuthPair*) lfirst(lc);

			/* we use the same privilege for gpfdist and gpfdists */
			if ((strcasecmp(extauth->protocol, "gpfdist") == 0) ||
			    (strcasecmp(extauth->protocol, "gpfdists") == 0))
			{
				if(strcasecmp(extauth->type, "readable") == 0)
				{
					*createrextgpfd = true;
					createrextgpfd_specified = true;
				}
				else
				{
					*createwextgpfd = true;
					createwextgpfd_specified = true;
				}
			}
			else /* http */
			{
				if(strcasecmp(extauth->type, "readable") == 0)
				{
					*createrexthttp = true;
					createrexthttp_specified = true;
				}
				else
				{
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("invalid CREATEEXTTABLE specification. writable http external tables do not exist")));
				}
			}
		}
	}

	/*
	 * go over the disallow list.
	 * if we're in CREATE ROLE, check that we don't negate something from the
	 * allow list. error out with conflicting options if we do.
	 * if we're in ALTER ROLE, just set the flags accordingly.
	 */
	if(list_length(disallow) > 0)
	{
		bool conflict = false;

		/* examine key value pairs */
		foreach(lc, disallow)
		{
			extAuthPair* extauth = (extAuthPair*) lfirst(lc);

			/* we use the same privilege for gpfdist and gpfdists */
			if ((strcasecmp(extauth->protocol, "gpfdist") == 0) ||
				(strcasecmp(extauth->protocol, "gpfdists") == 0))
			{
				if(strcasecmp(extauth->type, "readable") == 0)
				{
					if(createrextgpfd_specified)
						conflict = true;

					*createrextgpfd = false;
				}
				else
				{
					if(createwextgpfd_specified)
						conflict = true;

					*createwextgpfd = false;
				}
			}
			else /* http */
			{
				if(strcasecmp(extauth->type, "readable") == 0)
				{
					if(createrexthttp_specified)
						conflict = true;

					*createrexthttp = false;
				}
				else
				{
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("invalid NOCREATEEXTTABLE specification. writable http external tables do not exist")));
				}
			}
		}

		if(conflict)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("conflicting specifications in CREATEEXTTABLE and NOCREATEEXTTABLE")));

	}

}

/*
 * DelRoleMems -- Remove given members from the specified role
 *
 * rolename: name of role to del from (used only for error messages)
 * roleid: OID of role to del from
 * memberSpecs: list of RoleSpec of roles to del (used only for error messages)
 * memberIds: OIDs of roles to del
 * admin_opt: remove admin option only?
 *
 * Note: caller is responsible for calling auth_file_update_needed().
 */
static void
DelRoleMems(const char *rolename, Oid roleid,
			List *memberSpecs, List *memberIds,
			bool admin_opt)
{
	Relation	pg_authmem_rel;
	TupleDesc	pg_authmem_dsc;
	ListCell   *specitem;
	ListCell   *iditem;

	Assert(list_length(memberSpecs) == list_length(memberIds));

	/* Skip permission check if nothing to do */
	if (!memberIds)
		return;

	/*
	 * Check permissions: must have createrole or admin option on the role to
	 * be changed.  To mess with a superuser role, you gotta be superuser.
	 */
	if (superuser_arg(roleid))
	{
		if (!superuser())
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must be superuser to alter superusers")));
	}
	else
	{
		if (!have_createrole_privilege() &&
			!is_admin_of_role(GetUserId(), roleid))
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("must have admin option on role \"%s\"",
							rolename)));
	}

	pg_authmem_rel = table_open(AuthMemRelationId, RowExclusiveLock);
	pg_authmem_dsc = RelationGetDescr(pg_authmem_rel);

	forboth(specitem, memberSpecs, iditem, memberIds)
	{
		RoleSpec   *memberRole = lfirst(specitem);
		Oid			memberid = lfirst_oid(iditem);
		HeapTuple	authmem_tuple;

		/*
		 * Find entry for this role/member
		 */
		authmem_tuple = SearchSysCache2(AUTHMEMROLEMEM,
										ObjectIdGetDatum(roleid),
										ObjectIdGetDatum(memberid));
		if (!HeapTupleIsValid(authmem_tuple))
		{
			ereport(WARNING,
					(errmsg("role \"%s\" is not a member of role \"%s\"",
							get_rolespec_name(memberRole), rolename)));
			continue;
		}

		if (!admin_opt)
		{
			/* Remove the entry altogether */
			CatalogTupleDelete(pg_authmem_rel, &authmem_tuple->t_self);
		}
		else
		{
			/* Just turn off the admin option */
			HeapTuple	tuple;
			Datum		new_record[Natts_pg_auth_members];
			bool		new_record_nulls[Natts_pg_auth_members];
			bool		new_record_repl[Natts_pg_auth_members];

			/* Build a tuple to update with */
			MemSet(new_record, 0, sizeof(new_record));
			MemSet(new_record_nulls, false, sizeof(new_record_nulls));
			MemSet(new_record_repl, false, sizeof(new_record_repl));

			new_record[Anum_pg_auth_members_admin_option - 1] = BoolGetDatum(false);
			new_record_repl[Anum_pg_auth_members_admin_option - 1] = true;

			tuple = heap_modify_tuple(authmem_tuple, pg_authmem_dsc,
									  new_record,
									  new_record_nulls, new_record_repl);
			CatalogTupleUpdate(pg_authmem_rel, &tuple->t_self, tuple);
		}

		ReleaseSysCache(authmem_tuple);

		/* CCI after each change, in case there are duplicates in list */
		CommandCounterIncrement();
	}

	/*
	 * Close pg_authmem, but keep lock till commit.
	 */
	table_close(pg_authmem_rel, NoLock);
}

/*
 * ExtractAuthIntervalClause
 *
 * Build an authInterval struct (defined above) from given input
 */
static void
ExtractAuthIntervalClause(DefElem *defel, authInterval *interval)
{
	DenyLoginPoint *start = NULL, *end = NULL;
	char	*temp;
	if (IsA(defel->arg, DenyLoginInterval))
	{
		DenyLoginInterval *span = (DenyLoginInterval *)defel->arg;
		start = span->start;
		end = span->end;
	}
	else
	{
		Assert(IsA(defel->arg, DenyLoginPoint));
		start = (DenyLoginPoint *)defel->arg;
		end = start;
	}
	interval->start.day = ExtractAuthInterpretDay(start->day);
	temp = start->time != NULL ? strVal(start->time) : "00:00:00";
	interval->start.time = DatumGetTimeADT(DirectFunctionCall1(time_in, CStringGetDatum(temp)));
	interval->end.day = ExtractAuthInterpretDay(end->day);
	temp = end->time != NULL ? strVal(end->time) : "24:00:00";
	interval->end.time = DatumGetTimeADT(DirectFunctionCall1(time_in, CStringGetDatum(temp)));
	if (point_cmp(&interval->start, &interval->end) > 0)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("time interval must not wrap around")));
}

/*
 * TransferAuthInterpretDay -- Interpret day of week from parse node
 *
 * day: node which dictates a day of week;
 *		may be either an integer in [0, 6]
 *		or a string giving name of day in English
 */
static int16
ExtractAuthInterpretDay(Value * day)
{
	int16   ret;
	if (day->type == T_Integer)
	{
		ret = intVal(day);
		if (ret < 0 || ret > 6)
			ereport(ERROR,
					 (errcode(ERRCODE_SYNTAX_ERROR),
					  errmsg("numeric day of week must be between 0 and 6")));
	}
	else
	{
		int16		 elems = 7;
		char		*target = strVal(day);
		for (ret = 0; ret < elems; ret++)
			if (strcasecmp(target, daysofweek[ret]) == 0)
				break;
		if (ret == elems)
			ereport(ERROR,
					 (errcode(ERRCODE_SYNTAX_ERROR),
					  errmsg("invalid weekday name \"%s\"", target),
					  errhint("Day of week must be one of 'Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'.")));
	}
	return ret;
}

/*
 * AddRoleDenials -- Populate pg_auth_time_constraint
 *
 * rolename: name of role to add to (used only for error messages)
 * roleid: OID of role to add to
 * addintervals: list of authInterval structs dictating when
 *				  this particular role should be denied access
 *
 * Note: caller is reponsible for checking permissions to edit the given role.
 */
static void
AddRoleDenials(const char *rolename, Oid roleid, List *addintervals)
{
	Relation	pg_auth_time_rel;
	TupleDesc	pg_auth_time_dsc;
	ListCell   *intervalitem;

	pg_auth_time_rel = table_open(AuthTimeConstraintRelationId, RowExclusiveLock);
	pg_auth_time_dsc = RelationGetDescr(pg_auth_time_rel);

	foreach(intervalitem, addintervals)
	{
		authInterval 	*interval = (authInterval *)lfirst(intervalitem);
		HeapTuple   tuple;
		Datum		new_record[Natts_pg_auth_time_constraint];
		bool		new_record_nulls[Natts_pg_auth_time_constraint];

		/* Build a tuple to insert or update */
		MemSet(new_record, 0, sizeof(new_record));
		MemSet(new_record_nulls, false, sizeof(new_record_nulls));

		new_record[Anum_pg_auth_time_constraint_authid - 1] = ObjectIdGetDatum(roleid);
		new_record[Anum_pg_auth_time_constraint_start_day - 1] = Int16GetDatum(interval->start.day);
		new_record[Anum_pg_auth_time_constraint_start_time - 1] = TimeADTGetDatum(interval->start.time);
		new_record[Anum_pg_auth_time_constraint_end_day - 1] = Int16GetDatum(interval->end.day);
		new_record[Anum_pg_auth_time_constraint_end_time - 1] = TimeADTGetDatum(interval->end.time);

		tuple = heap_form_tuple(pg_auth_time_dsc, new_record, new_record_nulls);

		/* Insert tuple into the relation */
		CatalogTupleInsert(pg_auth_time_rel, tuple);
	}

	CommandCounterIncrement();

	/*
	 * Close pg_auth_time_constraint, but keep lock till commit (this is important to
	 * prevent any risk of deadlock failure while updating flat file)
	 */
	table_close(pg_auth_time_rel, NoLock);
}

/*
 * DelRoleDenials -- Trim pg_auth_time_constraint
 *
 * rolename: name of role to edit (used only for error messages)
 * roleid: OID of role to edit
 * dropintervals: list of authInterval structs dictating which
 *                existing rules should be dropped. Here, NIL will mean
 *                remove all constraints for the given role.
 *
 * Note: caller is reponsible for checking permissions to edit the given role.
 */
static void
DelRoleDenials(const char *rolename, Oid roleid, List *dropintervals)
{
	Relation    pg_auth_time_rel;
	ScanKeyData scankey;
	SysScanDesc sscan;
	ListCell	*intervalitem;
	bool		dropped_matching_interval = false;

	HeapTuple 	tmp_tuple;

	pg_auth_time_rel = table_open(AuthTimeConstraintRelationId, RowExclusiveLock);

	ScanKeyInit(&scankey,
				Anum_pg_auth_time_constraint_authid,
				BTEqualStrategyNumber, F_OIDEQ,
				ObjectIdGetDatum(roleid));
	sscan = systable_beginscan(pg_auth_time_rel, InvalidOid,
							   false, NULL, 1, &scankey);

	while (HeapTupleIsValid(tmp_tuple = systable_getnext(sscan)))
	{
		if (dropintervals != NIL)
		{
			Form_pg_auth_time_constraint obj = (Form_pg_auth_time_constraint) GETSTRUCT(tmp_tuple);
			authInterval *interval, *existing = (authInterval *) palloc0(sizeof(authInterval));
			existing->start.day = obj->start_day;
			existing->start.time = obj->start_time;
			existing->end.day = obj->end_day;
			existing->end.time = obj->end_time;
			foreach(intervalitem, dropintervals)
			{
				interval = (authInterval *)lfirst(intervalitem);
				if (interval_overlap(existing, interval))
				{
					if (Gp_role == GP_ROLE_DISPATCH)
						ereport(NOTICE,
								(errmsg("dropping DENY rule for \"%s\" between %s %s and %s %s",
										rolename,
										daysofweek[existing->start.day],
										DatumGetCString(DirectFunctionCall1(time_out, TimeADTGetDatum(existing->start.time))),
										daysofweek[existing->end.day],
										DatumGetCString(DirectFunctionCall1(time_out, TimeADTGetDatum(existing->end.time))))));
					CatalogTupleDelete(pg_auth_time_rel, &tmp_tuple->t_self);
					dropped_matching_interval = true;
					break;
				}
			}
		}
		else
			CatalogTupleDelete(pg_auth_time_rel, &tmp_tuple->t_self);
	}

	/* if intervals were specified and none was found, raise error */
	if (dropintervals && !dropped_matching_interval)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("cannot find matching DENY rules for \"%s\"", rolename)));

	systable_endscan(sscan);

	/*
	 * Close pg_auth_time_constraint, but keep lock till commit (this is important to
	 * prevent any risk of deadlock failure while updating flat file)
	 */
	table_close(pg_auth_time_rel, NoLock);
}
