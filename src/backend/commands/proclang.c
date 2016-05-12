/*-------------------------------------------------------------------------
 *
 * proclang.c
 *	  PostgreSQL PROCEDURAL LANGUAGE support code.
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/proclang.c,v 1.71 2007/01/22 01:35:20 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "catalog/catquery.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_pltemplate.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/proclang.h"
#include "miscadmin.h"
#include "parser/gramparse.h"
#include "parser/parse_func.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdisp_query.h"


typedef struct
{
	bool		tmpltrusted;	/* trusted? */
	char	   *tmplhandler;	/* name of handler function */
	char	   *tmplinline;		/* name of anonymous-block handler, or NULL */
	char	   *tmplvalidator;	/* name of validator function, or NULL */
	char	   *tmpllibrary;	/* path of shared library */
} PLTemplate;

static void create_proc_lang(const char *languageName, Oid handlerOid,
				 Oid inlineOid, Oid valOid, bool trusted, Oid *plangOid);
static PLTemplate *find_language_template(const char *languageName);


/* ---------------------------------------------------------------------
 * CREATE PROCEDURAL LANGUAGE
 * ---------------------------------------------------------------------
 */
void
CreateProceduralLanguage(CreatePLangStmt *stmt)
{
	char	   *languageName;
	PLTemplate *pltemplate;
	Oid			handlerOid,
				inlineOid,
				valOid;
	Oid			funcrettype;
	Oid			funcargtypes[1];

	/*
	 * Check permission
	 */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to create procedural language")));

	/*
	 * Translate the language name and check that this language doesn't
	 * already exist
	 */
	languageName = case_translate_language_name(stmt->plname);

	if (caql_getcount(
				NULL,
				cql("SELECT COUNT(*) FROM pg_language "
					" WHERE lanname = :1 ",
					CStringGetDatum(languageName))))
	{
		/*
		 * MPP-7563: special case plpgsql to omit a notice if it already exists
		 * rather than an error.  This allows us to install plpgsql by default
		 * while allowing it to be dropped and not create issues for 
		 * dump/restore.  This should be phased out in a later releases if/when
		 * plpgsql becomes a true internal language that can not be dropped.
		 *
		 * Note: hardcoding this on the name is semi-safe since we would ignore 
		 * any handler functions anyways since plpgsql exists in pg_pltemplate.
		 * Alternatively this logic could be extended to apply to all languages
		 * in pg_pltemplate.
		 */
		if (strcmp(languageName, "plpgsql") == 0) 
		{
			ereport(NOTICE,
					(errmsg("language \"plpgsql\" already exists, skipping")));
			return;
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_DUPLICATE_OBJECT),
					 errmsg("language \"%s\" already exists", languageName)));
		}
	}

	/*
	 * If we have template information for the language, ignore the supplied
	 * parameters (if any) and use the template information.
	 */
	if ((pltemplate = find_language_template(languageName)) != NULL)
	{
		List	   *funcname;

		/*
		 * Give a notice if we are ignoring supplied parameters.
		 */
		if (stmt->plhandler)
			if (Gp_role != GP_ROLE_EXECUTE)
				ereport(NOTICE,
						(errmsg("using pg_pltemplate information instead of "
								"CREATE LANGUAGE parameters")));

		/*
		 * Find or create the handler function, which we force to be in the
		 * pg_catalog schema.  If already present, it must have the correct
		 * return type.
		 */
		funcname = SystemFuncName(pltemplate->tmplhandler);
		handlerOid = LookupFuncName(funcname, 0, funcargtypes, true);
		if (OidIsValid(handlerOid))
		{
			funcrettype = get_func_rettype(handlerOid);
			if (funcrettype != LANGUAGE_HANDLEROID)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				  errmsg("function %s must return type \"language_handler\"",
						 NameListToString(funcname))));
		}
		else
		{
			handlerOid = ProcedureCreate(pltemplate->tmplhandler,
										 PG_CATALOG_NAMESPACE,
										 false, /* replace */
										 false, /* returnsSet */
										 LANGUAGE_HANDLEROID,
										 ClanguageId,
										 F_FMGR_C_VALIDATOR,
										 InvalidOid, /* describeFuncOid */
										 pltemplate->tmplhandler,
										 pltemplate->tmpllibrary,
										 false, /* isAgg */
										 false, /* isWin */
										 false, /* security_definer */
										 false, /* isStrict */
										 PROVOLATILE_VOLATILE,
										 buildoidvector(funcargtypes, 0),
										 PointerGetDatum(NULL),
										 PointerGetDatum(NULL),
										 PointerGetDatum(NULL),
										 NIL,
										 1,
										 0,
										 PRODATAACCESS_NONE,
										 stmt->plhandlerOid);
		}

		/*
		 * Likewise for the anonymous block handler, if required; but we don't
		 * care about its return type.
		 */
		if (pltemplate->tmplinline)
		{
			funcname = SystemFuncName(pltemplate->tmplinline);
			funcargtypes[0] = INTERNALOID;
			inlineOid = LookupFuncName(funcname, 1, funcargtypes, true);
			if (!OidIsValid(inlineOid))
			{
				inlineOid = ProcedureCreate(pltemplate->tmplinline,
										 PG_CATALOG_NAMESPACE,
										 false, /* replace */
										 false, /* returnsSet */
										 VOIDOID,
										 ClanguageId,
										 F_FMGR_C_VALIDATOR,
										 InvalidOid, /* describeFuncOid */
										 pltemplate->tmplinline,
										 pltemplate->tmpllibrary,
										 false, /* isAgg */
										 false, /* isWin */
										 false, /* security_definer */
										 true, /* isStrict */
										 PROVOLATILE_IMMUTABLE,
										 buildoidvector(funcargtypes, 1),
										 PointerGetDatum(NULL),
										 PointerGetDatum(NULL),
										 PointerGetDatum(NULL),
										 NIL,
										 1,
										 0,
										 PRODATAACCESS_NONE,
										 stmt->plinlineOid);

			}
		}
		else
			inlineOid = InvalidOid;

		/*
		 * Likewise for the validator, if required; but we don't care about
		 * its return type.
		 */
		if (pltemplate->tmplvalidator)
		{
			funcname = SystemFuncName(pltemplate->tmplvalidator);
			funcargtypes[0] = OIDOID;
			valOid = LookupFuncName(funcname, 1, funcargtypes, true);
			if (!OidIsValid(valOid))
			{
				valOid = ProcedureCreate(pltemplate->tmplvalidator,
										 PG_CATALOG_NAMESPACE,
										 false, /* replace */
										 false, /* returnsSet */
										 VOIDOID,
										 ClanguageId,
										 F_FMGR_C_VALIDATOR,
										 InvalidOid, /* describeFuncOid */
										 pltemplate->tmplvalidator,
										 pltemplate->tmpllibrary,
										 false, /* isAgg */
										 false, /* isWin */
										 false, /* security_definer */
										 true, /* isStrict */
										 PROVOLATILE_IMMUTABLE,
										 buildoidvector(funcargtypes, 1),
										 PointerGetDatum(NULL),
										 PointerGetDatum(NULL),
										 PointerGetDatum(NULL),
										 NIL,
										 1,
										 0,
										 PRODATAACCESS_NONE,
										 stmt->plvalidatorOid);
			}
		}
		else
			valOid = InvalidOid;

		/* ok, create it */
		create_proc_lang(languageName, handlerOid, inlineOid, valOid,
						 pltemplate->tmpltrusted, &(stmt->plangOid));
	}
	else
	{
		/*
		 * No template, so use the provided information.  If there's no
		 * handler clause, the user is trying to rely on a template that we
		 * don't have, so complain accordingly.
		 */
		if (!stmt->plhandler)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("unsupported language \"%s\"",
							languageName),
					 errhint("The supported languages are listed in the pg_pltemplate system catalog.")));

		/*
		 * Lookup the PL handler function and check that it is of the expected
		 * return type
		 */
		handlerOid = LookupFuncName(stmt->plhandler, 0, funcargtypes, false);
		funcrettype = get_func_rettype(handlerOid);
		if (funcrettype != LANGUAGE_HANDLEROID)
		{
			/*
			 * We allow OPAQUE just so we can load old dump files.	When we
			 * see a handler function declared OPAQUE, change it to
			 * LANGUAGE_HANDLER.  (This is probably obsolete and removable?)
			 */
			if (funcrettype == OPAQUEOID)
			{
				if (Gp_role != GP_ROLE_EXECUTE)
				ereport(WARNING,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("changing return type of function %s from \"opaque\" to \"language_handler\"",
								NameListToString(stmt->plhandler))));
				SetFunctionReturnType(handlerOid, LANGUAGE_HANDLEROID);
			}
			else
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				  errmsg("function %s must return type \"language_handler\"",
						 NameListToString(stmt->plhandler))));
		}

		/* validate the inline function */
		if (stmt->plinline)
		{
			funcargtypes[0] = INTERNALOID;
			inlineOid = LookupFuncName(stmt->plinline, 1, funcargtypes, false);
			/* return value is ignored, so we don't check the type */
		}
		else
			inlineOid = InvalidOid;

		/* validate the validator function */
		if (stmt->plvalidator)
		{
			funcargtypes[0] = OIDOID;
			valOid = LookupFuncName(stmt->plvalidator, 1, funcargtypes, false);
			/* return value is ignored, so we don't check the type */
		}
		else
			valOid = InvalidOid;

		/* ok, create it */
		create_proc_lang(languageName, handlerOid, inlineOid, valOid, stmt->pltrusted, &(stmt->plangOid));
	}
	
	
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		stmt->plhandlerOid = handlerOid;
		stmt->plinlineOid = inlineOid;
		stmt->plvalidatorOid = valOid;
		CdbDispatchUtilityStatement((Node *) stmt, "CreateProceduralLanguage");
	}
}

/*
 * Guts of language creation.
 */
static void
create_proc_lang(const char *languageName,
				 Oid handlerOid, Oid inlineOid, Oid valOid, bool trusted, Oid *plangoid)
{
	Datum		values[Natts_pg_language];
	bool		nulls[Natts_pg_language];
	NameData	langname;
	HeapTuple	tup;
	cqContext  *pcqCtx;
	ObjectAddress myself,
				referenced;

	/*
	 * Insert the new language into pg_language
	 */
	pcqCtx = caql_beginscan(
			NULL,
			cql("INSERT INTO pg_language", 
				NULL));

	memset(values, 0, sizeof(values));
	memset(nulls, false, sizeof(nulls));

	namestrcpy(&langname, languageName);
	values[Anum_pg_language_lanname - 1] = NameGetDatum(&langname);
	values[Anum_pg_language_lanispl - 1] = BoolGetDatum(true);
	values[Anum_pg_language_lanpltrusted - 1] = BoolGetDatum(trusted);
	values[Anum_pg_language_lanplcallfoid - 1] = ObjectIdGetDatum(handlerOid);
	values[Anum_pg_language_laninline - 1] = ObjectIdGetDatum(inlineOid);
	values[Anum_pg_language_lanvalidator - 1] = ObjectIdGetDatum(valOid);
	nulls[Anum_pg_language_lanacl - 1] = true;

	tup = caql_form_tuple(pcqCtx, values, nulls);

	/* Keep oids synchronized between master and segments */
	if (OidIsValid(*plangoid))
		HeapTupleSetOid(tup, *plangoid);

	*plangoid = caql_insert(pcqCtx, tup); /* implicit update of index as well*/

	/*
	 * Create dependencies for language
	 */
	myself.classId = LanguageRelationId;
	myself.objectId = HeapTupleGetOid(tup);
	myself.objectSubId = 0;

	/* dependency on the PL handler function */
	referenced.classId = ProcedureRelationId;
	referenced.objectId = handlerOid;
	referenced.objectSubId = 0;
	recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

	/* dependency on the inline handler function, if any */
	if (OidIsValid(inlineOid))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = inlineOid;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	/* dependency on the validator function, if any */
	if (OidIsValid(valOid))
	{
		referenced.classId = ProcedureRelationId;
		referenced.objectId = valOid;
		referenced.objectSubId = 0;
		recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
	}

	caql_endscan(pcqCtx);

}

/*
 * Look to see if we have template information for the given language name.
 */
static PLTemplate *
find_language_template(const char *languageName)
{
	PLTemplate *result;
	Relation	rel;
	HeapTuple	tup;
	cqContext	cqc;

	rel = heap_open(PLTemplateRelationId, AccessShareLock);

	tup = caql_getfirst(
			caql_addrel(cqclr(&cqc), rel),
			cql("SELECT * FROM pg_pltemplate "
				" WHERE tmplname = :1 ",
				CStringGetDatum((char *) languageName)));

	if (HeapTupleIsValid(tup))
	{
		Form_pg_pltemplate tmpl = (Form_pg_pltemplate) GETSTRUCT(tup);
		Datum		datum;
		bool		isnull;

		result = (PLTemplate *) palloc0(sizeof(PLTemplate));
		result->tmpltrusted = tmpl->tmpltrusted;

		/* Remaining fields are variable-width so we need heap_getattr */
		datum = heap_getattr(tup, Anum_pg_pltemplate_tmplhandler,
							 RelationGetDescr(rel), &isnull);
		if (!isnull)
			result->tmplhandler =
				DatumGetCString(DirectFunctionCall1(textout, datum));

		datum = heap_getattr(tup, Anum_pg_pltemplate_tmplinline,
							 RelationGetDescr(rel), &isnull);
		if (!isnull)
			result->tmplinline = TextDatumGetCString(datum);

		datum = heap_getattr(tup, Anum_pg_pltemplate_tmplvalidator,
							 RelationGetDescr(rel), &isnull);
		if (!isnull)
			result->tmplvalidator =
				DatumGetCString(DirectFunctionCall1(textout, datum));

		datum = heap_getattr(tup, Anum_pg_pltemplate_tmpllibrary,
							 RelationGetDescr(rel), &isnull);
		if (!isnull)
			result->tmpllibrary =
				DatumGetCString(DirectFunctionCall1(textout, datum));

		/* Ignore template if handler or library info is missing */
		if (!result->tmplhandler || !result->tmpllibrary)
			result = NULL;
	}
	else
		result = NULL;

	heap_close(rel, AccessShareLock);

	return result;
}


/*
 * This just returns TRUE if we have a valid template for a given language
 */
bool
PLTemplateExists(const char *languageName)
{
	return (find_language_template(languageName) != NULL);
}


/* ---------------------------------------------------------------------
 * DROP PROCEDURAL LANGUAGE
 * ---------------------------------------------------------------------
 */
void
DropProceduralLanguage(DropPLangStmt *stmt)
{
	char	   *languageName;
	int			fetchCount;
	Oid			langOid;
	ObjectAddress object;

	/*
	 * Check permission
	 */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to drop procedural language")));

	/*
	 * Translate the language name, check that the language exists
	 */
	languageName = case_translate_language_name(stmt->plname);

	langOid = caql_getoid_plus(
			NULL,
			&fetchCount,
			NULL,
			cql("SELECT oid FROM pg_language "
				" WHERE lanname = :1 ",
				CStringGetDatum(languageName)));

	if (0 == fetchCount)
	{
		if (!stmt->missing_ok)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("language \"%s\" does not exist", languageName)));
		else
			ereport(NOTICE,
					(errmsg("language \"%s\" does not exist, skipping",
							languageName)));

		return;
	}

	object.classId = LanguageRelationId;
	object.objectId = langOid;
	object.objectSubId = 0;

	/*
	 * Do the deletion
	 */
	performDeletion(&object, stmt->behavior);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbDispatchUtilityStatement((Node *) stmt, "DropProceduralLanguage");
	}
}

/*
 * Guts of language dropping.
 */
void
DropProceduralLanguageById(Oid langOid)
{
	if (!caql_getcount(
				NULL,
				cql("DELETE FROM pg_language "
					" WHERE oid = :1 ",
					ObjectIdGetDatum(langOid))))
	{
		/* should not happen */
		elog(ERROR, "cache lookup failed for language %u", langOid);
	}

}

/*
 * Rename language
 */
void
RenameLanguage(const char *oldname, const char *newname)
{
	HeapTuple	tup;
	Relation	rel;
	cqContext	cqc2;
	cqContext	cqc;
	cqContext  *pcqCtx;

	/* Translate both names for consistency with CREATE */
	oldname = case_translate_language_name(oldname);
	newname = case_translate_language_name(newname);

	rel = heap_open(LanguageRelationId, RowExclusiveLock);

	pcqCtx = caql_addrel(cqclr(&cqc), rel);

	tup = caql_getfirst(
			pcqCtx,
			cql("SELECT * FROM pg_language "
				" WHERE lanname = :1 "
				" FOR UPDATE ",
				CStringGetDatum((char *) oldname)));

	if (!HeapTupleIsValid(tup))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("language \"%s\" does not exist", oldname)));

	/* make sure the new name doesn't exist */
	if (caql_getcount(
				caql_addrel(cqclr(&cqc2), rel),
				cql("SELECT COUNT(*) FROM pg_language "
					" WHERE lanname = :1 ",
					CStringGetDatum((char *) newname))))
	{
		ereport(ERROR,
				(errcode(ERRCODE_DUPLICATE_OBJECT),
				 errmsg("language \"%s\" already exists", newname)));
	}

	/* must be superuser, since we do not have owners for PLs */
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to rename procedural language")));

	/* rename */
	namestrcpy(&(((Form_pg_language) GETSTRUCT(tup))->lanname), newname);
	caql_update_current(pcqCtx, tup); /* implicit update of index as well */

	heap_close(rel, NoLock);
	heap_freetuple(tup);
}
