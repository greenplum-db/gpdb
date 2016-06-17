/*-------------------------------------------------------------------------
 *
 * catalog.c
 *		routines concerned with catalog naming conventions and other
 *		bits of hard-wired knowledge
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/catalog/catalog.c,v 1.72.2.1 2008/02/20 17:44:14 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <fcntl.h>
#include <unistd.h>

#include "access/genam.h"
#include "access/transam.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_auth_members.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_database.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_pltemplate.h"
#include "catalog/pg_resqueue.h"
#include "catalog/pg_shdepend.h"
#include "catalog/pg_shdescription.h"
#include "catalog/pg_filespace.h"
#include "catalog/pg_filespace_entry.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_resqueue.h"
#include "catalog/pg_rewrite.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_tidycat.h"
#include "catalog/pg_trigger.h"

#include "catalog/gp_configuration.h"
#include "catalog/gp_configuration.h"
#include "catalog/gp_segment_config.h"
#include "catalog/gp_san_config.h"

#include "catalog/gp_persistent.h"
#include "catalog/gp_global_sequence.h"
#include "catalog/gp_id.h"
#include "catalog/gp_version.h"
#include "catalog/toasting.h"
#include "catalog/gp_policy.h"

#include "miscadmin.h"
#include "storage/fd.h"
#include "utils/fmgroids.h"
#include "utils/relcache.h"
#include "utils/lsyscache.h"

#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbvars.h"

#define OIDCHARS	10			/* max chars printed by %u */

static void GetFilespacePathForTablespace(
	Oid tablespaceOid,

	char **filespacePath)
{
	PersistentTablespaceGetFilespaces tablespaceGetFilespaces;
	Oid filespaceOid;

	/* All other tablespaces are accessed via filespace locations */
	char *primary_path;
	char *mirror_path;	/* unused */

	Assert(tablespaceOid != GLOBALTABLESPACE_OID);
	Assert(tablespaceOid != DEFAULTTABLESPACE_OID);
	
	/* Lookup filespace location from the persistent object layer. */
	tablespaceGetFilespaces = 
			PersistentTablespace_TryGetPrimaryAndMirrorFilespaces(
														tablespaceOid, 
														&primary_path, 
														&mirror_path,
														&filespaceOid);
	switch (tablespaceGetFilespaces)
	{
	case PersistentTablespaceGetFilespaces_TablespaceNotFound:
		ereport(ERROR, 
				(errcode(ERRCODE_CDB_INTERNAL_ERROR),
				 errmsg("Unable to find entry for tablespace OID = %u when forming file-system path",
						tablespaceOid)));
		break;
			
	case PersistentTablespaceGetFilespaces_FilespaceNotFound:
		ereport(ERROR, 
				(errcode(ERRCODE_CDB_INTERNAL_ERROR),
				 errmsg("Unable to find entry for filespace OID = %u when forming file-system path for tablespace OID = %u",
				 		filespaceOid,
						tablespaceOid)));
		break;
					
	case PersistentTablespaceGetFilespaces_Ok:
		// Go below and pass back the result.
		break;
		
	default:
		elog(ERROR, "Unexpected tablespace filespace fetch result: %d",
			 tablespaceGetFilespaces);
	}
	
	/*
	 * We immediately throw out the mirror_path because it is not
	 * relevant here.
	 */
	if (mirror_path)
		pfree(mirror_path);
	Assert(primary_path != NULL);

	*filespacePath = primary_path;
}

/*
 * relpath			- construct path to a relation's file
 *
 * Result is a palloc'd string.
 */
char *
relpath(RelFileNode rnode)
{
	int			pathlen;
	char	   *path;
	int 		snprintfResult;

	if (rnode.spcNode == GLOBALTABLESPACE_OID)
	{
		/* Shared system relations live in {datadir}/global */
		Assert(rnode.dbNode == 0);
		pathlen = 7 + OIDCHARS + 1;
		path = (char *) palloc(pathlen);
		snprintfResult =
			snprintf(path, pathlen, "global/%u",
					 rnode.relNode);
		
	}
	else if (rnode.spcNode == DEFAULTTABLESPACE_OID)
	{
		/* The default tablespace is {datadir}/base */
		pathlen = 5 + OIDCHARS + 1 + OIDCHARS + 1;
		path = (char *) palloc(pathlen);
		snprintfResult =
			snprintf(path, pathlen, "base/%u/%u",
					 rnode.dbNode, rnode.relNode);
	}
	else
	{
		char *primary_path;

		/* All other tablespaces are accessed via filespace locations */
		GetFilespacePathForTablespace(
								rnode.spcNode,
								&primary_path);

		/* 
		 * We should develop an interface for the above that doesn't
		 * require reallocating to a slightly larger size...
		 */
		pathlen = strlen(primary_path)+1+OIDCHARS+1+OIDCHARS+1+OIDCHARS+1;
		path = (char *) palloc(pathlen);
		snprintfResult =
			snprintf(path, pathlen, "%s/%u/%u/%u",
					 primary_path, rnode.spcNode, rnode.dbNode, rnode.relNode);

		/* Throw away the allocation we got from persistent layer */
		pfree(primary_path);
	}
	
	Assert(snprintfResult >= 0);
	Assert(snprintfResult < pathlen);

	return path;
}

void
CopyRelPath(char *target, int targetMaxLen, RelFileNode rnode)
{
	int 		snprintfResult;

	if (rnode.spcNode == GLOBALTABLESPACE_OID)
	{
		/* Shared system relations live in {datadir}/global */
		Assert(rnode.dbNode == 0);
		snprintfResult =
			snprintf(target, targetMaxLen, "global/%u",
					 rnode.relNode);
	}
	else if (rnode.spcNode == DEFAULTTABLESPACE_OID)
	{
		/* The default tablespace is {datadir}/base */
		snprintfResult =
			snprintf(target, targetMaxLen, "base/%u/%u",
					 rnode.dbNode, rnode.relNode);
	}
	else
	{
		char *primary_path;

		/* All other tablespaces are accessed via filespace locations */
		GetFilespacePathForTablespace(
								rnode.spcNode,
								&primary_path);

		/* Copy path into the passed in target location */
		snprintfResult =
			snprintf(target, targetMaxLen, "%s/%u/%u/%u",
					 primary_path, rnode.spcNode, rnode.dbNode, rnode.relNode);

		/* Throw away the allocation we got from persistent layer */
		pfree(primary_path);
	}

	if (snprintfResult < 0)
		elog(ERROR, "CopyRelPath formatting error");

	/*
	 * Magically truncating the result to fit in the target string is unacceptable here
	 * because it can result in the wrong file-system object being referenced.
	 */
	if (snprintfResult >= targetMaxLen)
		elog(ERROR, "CopyRelPath formatting result length %d exceeded the maximum length %d",
					snprintfResult,
					targetMaxLen);
}

/*
 * GetDatabasePath			- construct path to a database dir
 *
 * Result is a palloc'd string.
 *
 * XXX this must agree with relpath()!
 */
char *
GetDatabasePath(Oid dbNode, Oid spcNode)
{
	int			pathlen;
	char	   *path;
	int 		snprintfResult;

	if (spcNode == GLOBALTABLESPACE_OID)
	{
		/* Shared system relations live in {datadir}/global */
		Assert(dbNode == 0);
		pathlen = 6 + 1;
		path = (char *) palloc(pathlen);

		// Using strncpy is error prone.
		snprintfResult =
			snprintf(path, pathlen, "global");
	}
	else if (spcNode == DEFAULTTABLESPACE_OID)
	{
		/* The default tablespace is {datadir}/base */
		pathlen = 5 + OIDCHARS + 1;
		path = (char *) palloc(pathlen);
		snprintfResult =
			snprintf(path, pathlen, "base/%u",
					 dbNode);
	}
	else
	{
		char *primary_path;

		/* All other tablespaces are accessed via filespace locations */
		GetFilespacePathForTablespace(
								spcNode,
								&primary_path);

		/* 
		 * We should develop an interface for the above that doesn't
		 * require reallocating to a slightly larger size...
		 */
		pathlen = strlen(primary_path)+1+OIDCHARS+1+OIDCHARS+1;
		path = (char *) palloc(pathlen);
		snprintfResult =
			snprintf(path, pathlen, "%s/%u/%u",
					 primary_path, spcNode, dbNode);

		/* Throw away the allocation we got from persistent layer */
		pfree(primary_path);
	}
	
	Assert(snprintfResult >= 0);
	Assert(snprintfResult < pathlen);

	return path;
}

void
CopyDatabasePath(char *target, int targetMaxLen, Oid dbNode, Oid spcNode)
{
	int 		snprintfResult;

	if (spcNode == GLOBALTABLESPACE_OID)
	{
		/* Shared system relations live in {datadir}/global */
		Assert(dbNode == 0);

		// Using strncpy is error prone.
		snprintfResult =
			snprintf(target, targetMaxLen, "global");
	}
	else if (spcNode == DEFAULTTABLESPACE_OID)
	{
		/* The default tablespace is {datadir}/base */
		snprintfResult =
			snprintf(target, targetMaxLen, "base/%u",
					 dbNode);
	}
	else
	{
		char *primary_path;

		/* All other tablespaces are accessed via filespace locations */
		GetFilespacePathForTablespace(
								spcNode,
								&primary_path);

		/* Copy path into the passed in target location */
		snprintfResult =
			snprintf(target, targetMaxLen, "%s/%u/%u",
					 primary_path, spcNode, dbNode);

		/* Throw away the allocation we got from persistent layer */
		pfree(primary_path);
	}

	if (snprintfResult < 0)
		elog(ERROR, "CopyDatabasePath formatting error");

	/*
	 * Magically truncating the result to fit in the target string is unacceptable here
	 * because it can result in the wrong file-system object being referenced.
	 */
	if (snprintfResult >= targetMaxLen)
		elog(ERROR, "CopyDatabasePath formatting result length %d exceeded the maximum length %d",
					snprintfResult,
					targetMaxLen);
}


void FormDatabasePath(
	char *databasePath,

	char *filespaceLocation,

	Oid tablespaceOid,

	Oid databaseOid)
{
	int			targetMaxLen = MAXPGPATH + 1;
	int 		snprintfResult;

	if (tablespaceOid == GLOBALTABLESPACE_OID)
	{
		/* Shared system relations live in {datadir}/global */
		Assert(databaseOid == 0);
		Assert(filespaceLocation == NULL);

		// Using strncpy is error prone.
		snprintfResult =
			snprintf(databasePath, targetMaxLen, "global");
	}
	else if (tablespaceOid == DEFAULTTABLESPACE_OID)
	{
		/* The default tablespace is {datadir}/base */
		Assert(filespaceLocation == NULL);

		snprintfResult =
			snprintf(databasePath, targetMaxLen, "base/%u",
					 databaseOid);
	}
	else
	{
		/* All other tablespaces are in filespace locations */
		Assert(filespaceLocation != NULL);

		snprintfResult =
			snprintf(databasePath, targetMaxLen, "%s/%u/%u",
					 filespaceLocation, tablespaceOid, databaseOid);
	}

	if (snprintfResult < 0)
		elog(ERROR, "CopyDatabasePath formatting error");

	/*
	 * Magically truncating the result to fit in the target string is unacceptable here
	 * because it can result in the wrong file-system object being referenced.
	 */
	if (snprintfResult >= targetMaxLen)
		elog(ERROR, "CopyDatabasePath formatting result length %d exceeded the maximum length %d",
					snprintfResult,
					targetMaxLen);
}

void FormTablespacePath(
	char *tablespacePath,

	char *filespaceLocation,

	Oid tablespaceOid)
{
	int			targetMaxLen = MAXPGPATH + 1;
	int 		snprintfResult;

	if (tablespaceOid == GLOBALTABLESPACE_OID)
	{
		/* Shared system relations live in {datadir}/global */
		Assert(filespaceLocation == NULL);

		// Using strncpy is error prone.
		snprintfResult =
			snprintf(tablespacePath, targetMaxLen, "global");
	}
	else if (tablespaceOid == DEFAULTTABLESPACE_OID)
	{
		/* The default tablespace is {datadir}/base */
		Assert(filespaceLocation == NULL);

		// Using strncpy is error prone.
		snprintfResult =
			snprintf(tablespacePath, targetMaxLen, "base");
	}
	else
	{
		/* All other tablespaces are in filespace locations */
		Assert(filespaceLocation != NULL);
		snprintfResult =
			snprintf(tablespacePath, targetMaxLen, "%s/%u",
					 filespaceLocation, tablespaceOid);
	}

	if (snprintfResult < 0)
		elog(ERROR, "FormTablespacePath formatting error");

	/*
	 * Magically truncating the result to fit in the target string is unacceptable here
	 * because it can result in the wrong file-system object being referenced.
	 */
	if (snprintfResult >= targetMaxLen)
		elog(ERROR, "FormTablespacePath formatting result length %d exceeded the maximum length %d",
					snprintfResult,
					targetMaxLen);
}


void 
FormRelationPath(char *relationPath, char *filespaceLocation, RelFileNode rnode)
{
	int			targetMaxLen = MAXPGPATH + 1;
	int 		snprintfResult;

	if (rnode.spcNode == GLOBALTABLESPACE_OID)
	{
		/* Shared system relations live in {datadir}/global */
		Assert(rnode.dbNode == 0);
		
		snprintfResult =
			snprintf(relationPath, targetMaxLen, "global/%u",
					 rnode.relNode);
	}
	else if (rnode.spcNode == DEFAULTTABLESPACE_OID)
	{
		/* The default tablespace is {datadir}/base */
		
		snprintfResult =
			snprintf(relationPath, targetMaxLen, "base/%u/%u",
					 rnode.dbNode, rnode.relNode);
	}
	else
	{
		snprintfResult =
			snprintf(relationPath, targetMaxLen, "%s/%u/%u/%u",
				filespaceLocation,
				rnode.spcNode,
				rnode.dbNode,
				rnode.relNode);
	}	

	if (snprintfResult < 0)
		elog(ERROR, "FormRelationPath formatting error");

	/*
	 * Magically truncating the result to fit in the target string is unacceptable here
	 * because it can result in the wrong file-system object being referenced.
	 */
	if (snprintfResult >= targetMaxLen)
		elog(ERROR, "FormRelationPath formatting result length %d exceeded the maximum length %d",
					snprintfResult,
					targetMaxLen);
}

/*
 * IsSystemRelation
 *		True iff the relation is a system catalog relation.
 *
 *		NB: TOAST relations are considered system relations by this test
 *		for compatibility with the old IsSystemRelationName function.
 *		This is appropriate in many places but not all.  Where it's not,
 *		also check IsToastRelation.
 *
 *		We now just test if the relation is in the system catalog namespace;
 *		so it's no longer necessary to forbid user relations from having
 *		names starting with pg_.
 */
bool
IsSystemRelation(Relation relation)
{
	return IsSystemNamespace(RelationGetNamespace(relation)) ||
		   IsToastNamespace(RelationGetNamespace(relation)) ||
		   IsAoSegmentNamespace(RelationGetNamespace(relation));
}

/*
 * IsSystemClass
 *		Like the above, but takes a Form_pg_class as argument.
 *		Used when we do not want to open the relation and have to
 *		search pg_class directly.
 */
bool
IsSystemClass(Form_pg_class reltuple)
{
	Oid			relnamespace = reltuple->relnamespace;

	return IsSystemNamespace(relnamespace) ||
		IsToastNamespace(relnamespace) ||
		IsAoSegmentNamespace(relnamespace);
}

/*
 * IsToastRelation
 *		True iff relation is a TOAST support relation (or index).
 */
bool
IsToastRelation(Relation relation)
{
	return IsToastNamespace(RelationGetNamespace(relation));
}

/*
 * IsToastClass
 *		Like the above, but takes a Form_pg_class as argument.
 *		Used when we do not want to open the relation and have to
 *		search pg_class directly.
 */
bool
IsToastClass(Form_pg_class reltuple)
{
	Oid			relnamespace = reltuple->relnamespace;

	return IsToastNamespace(relnamespace);
}

/*
 * IsAoSegmentRelation
 *		True iff relation is an AO segment support relation (or index).
 */
bool
IsAoSegmentRelation(Relation relation)
{
	return IsAoSegmentNamespace(RelationGetNamespace(relation));
}

/*
 * IsAoSegmentClass
 *		Like the above, but takes a Form_pg_class as argument.
 *		Used when we do not want to open the relation and have to
 *		search pg_class directly.
 */
bool
IsAoSegmentClass(Form_pg_class reltuple)
{
	Oid			relnamespace = reltuple->relnamespace;
	
	return IsAoSegmentNamespace(relnamespace);
}

/*
 * IsSystemNamespace
 *		True iff namespace is pg_catalog.
 *
 * NOTE: the reason this isn't a macro is to avoid having to include
 * catalog/pg_namespace.h in a lot of places.
 */
bool
IsSystemNamespace(Oid namespaceId)
{
	return namespaceId == PG_CATALOG_NAMESPACE;
}

/*
 * IsToastNamespace
 *		True iff namespace is pg_toast or my temporary-toast-table namespace.
 *
 * Note: this will return false for temporary-toast-table namespaces belonging
 * to other backends.  Those are treated the same as other backends' regular
 * temp table namespaces, and access is prevented where appropriate.
 */
bool
IsToastNamespace(Oid namespaceId)
{
	return (namespaceId == PG_TOAST_NAMESPACE) ||
		isTempToastNamespace(namespaceId);
}

/*
 * IsAoSegmentNamespace
 *		True iff namespace is pg_aoseg.
 *
 * NOTE: the reason this isn't a macro is to avoid having to include
 * catalog/pg_namespace.h in a lot of places.
 */
bool
IsAoSegmentNamespace(Oid namespaceId)
{
	return namespaceId == PG_AOSEGMENT_NAMESPACE;
}

/*
 * IsReservedName
 *		True iff name starts with the pg_ prefix.
 *
 *		For some classes of objects, the prefix pg_ is reserved for
 *		system objects only.  As of 8.0, this is only true for
 *		schema and tablespace names.
 *
 *      As of Greenplum 4.0 we also reserve the prefix gp_
 */
bool
IsReservedName(const char *name)
{
	/* ugly coding for speed */
	return ((name[0] == 'p' && name[1] == 'g' && name[2] == '_') ||
			(name[0] == 'g' && name[1] == 'p' && name[2] == '_'));
}

/*
 * GetReservedPrefix
 *		Given a string that is a reserved name return the portion of
 *      the name that makes it reserved - the reserved prefix.
 *
 *      Current return values include "pg_" and "gp_"
 */
char *
GetReservedPrefix(const char *name)
{
	char		*prefix = NULL;

	if (IsReservedName(name))
	{
		prefix = palloc(4);
		memcpy(prefix, name, 3);
		prefix[3] = '\0';
	}

	return prefix;
}

/*
 * IsSharedRelation
 *		Given the OID of a relation, determine whether it's supposed to be
 *		shared across an entire database cluster.
 *
 * Hard-wiring this list is pretty grotty, but we really need it so that
 * we can compute the locktag for a relation (and then lock it) without
 * having already read its pg_class entry.	If we try to retrieve relisshared
 * from pg_class with no pre-existing lock, there is a race condition against
 * anyone who is concurrently committing a change to the pg_class entry:
 * since we read system catalog entries under SnapshotNow, it's possible
 * that both the old and new versions of the row are invalid at the instants
 * we scan them.  We fix this by insisting that updaters of a pg_class
 * row must hold exclusive lock on the corresponding rel, and that users
 * of a relation must hold at least AccessShareLock on the rel *before*
 * trying to open its relcache entry.  But to lock a rel, you have to
 * know if it's shared.  Fortunately, the set of shared relations is
 * fairly static, so a hand-maintained list of their OIDs isn't completely
 * impractical.
 */
bool
IsSharedRelation(Oid relationId)
{
	/* These are the shared catalogs (look for BKI_SHARED_RELATION) */
	if (
		relationId == AuthMemRelationId ||
		relationId == DatabaseRelationId ||
		relationId == PLTemplateRelationId ||
		relationId == SharedDescriptionRelationId ||
		relationId == SharedDependRelationId ||
		relationId == FileSpaceRelationId ||
		relationId == TableSpaceRelationId ||

		relationId == GpIdRelationId ||
		relationId == GpVersionRelationId ||

		relationId == GpPersistentRelationNodeRelationId ||
		relationId == GpPersistentDatabaseNodeRelationId ||
		relationId == GpPersistentTablespaceNodeRelationId ||
		relationId == GpPersistentFilespaceNodeRelationId ||
		relationId == GpGlobalSequenceRelationId ||

		/* MPP-6929: metadata tracking */
		relationId == StatLastShOpRelationId ||

/* TIDYCAT_BEGIN_CODEGEN 
*/
/*
   WARNING: DO NOT MODIFY THE FOLLOWING SECTION: 
   Generated by ./tidycat.pl version 29
   on Mon Aug  1 17:08:37 2011
 */
/* relation id: 6026 - pg_resqueue 20100129 */
relationId == ResQueueRelationId || 
/* relation id: 6059 - pg_resourcetype 20100129 */
relationId == ResourceTypeRelationId || 
/* relation id: 6060 - pg_resqueuecapability 20100129 */
relationId == ResQueueCapabilityRelationId || 
/* relation id: 1260 - pg_authid 20100129 */
relationId == AuthIdRelationId || 
/* relation id: 5035 - gp_san_configuration 20101104 */
relationId == GpSanConfigRelationId || 
/* relation id: 5039 - gp_fault_strategy 20101104 */
relationId == GpFaultStrategyRelationId || 
/* relation id: 5000 - gp_configuration 20101104 */
relationId == GpConfigurationRelationId || 
/* relation id: 5006 - gp_configuration_history 20101104 */
relationId == GpConfigHistoryRelationId || 
/* relation id: 5029 - gp_db_interfaces 20101104 */
relationId == GpDbInterfacesRelationId || 
/* relation id: 5030 - gp_interfaces 20101104 */
relationId == GpInterfacesRelationId || 
/* relation id: 5036 - gp_segment_configuration 20101122 */
relationId == GpSegmentConfigRelationId || 
/* relation id: 5033 - pg_filespace_entry 20101122 */
relationId == FileSpaceEntryRelationId || 

/* relation id: 2914 - pg_auth_time_constraint 20110908 */
relationId == AuthTimeConstraintRelationId ||
/* TIDYCAT_END_CODEGEN */
		0 /* OR ZERO */
			)

		return true;
	/* These are their indexes (see indexing.h) */
	if (
		relationId == AuthMemRoleMemIndexId ||
		relationId == AuthMemMemRoleIndexId ||
		relationId == DatabaseNameIndexId ||
		relationId == DatabaseOidIndexId ||
		relationId == PLTemplateNameIndexId ||
		relationId == SharedDescriptionObjIndexId ||
		relationId == SharedDependDependerIndexId ||
		relationId == SharedDependReferenceIndexId ||
		relationId == FilespaceOidIndexId ||
		relationId == FilespaceNameIndexId ||
		relationId == TablespaceOidIndexId ||
		relationId == TablespaceNameIndexId ||

		/* MPP-6929: metadata tracking */
		relationId == StatLastShOpClassidObjidIndexId ||
		relationId == StatLastShOpClassidObjidStaactionnameIndexId ||

/* TIDYCAT_BEGIN_CODEGEN 
*/
/*
   WARNING: DO NOT MODIFY THE FOLLOWING SECTION: 
   Generated by ./tidycat.pl version 3.
   on Thu Jun  9 20:51:39 2011
 */
/* relation id: 6026 - pg_resqueue 20100129 */
relationId == ResQueueOidIndexId || 
/* relation id: 6026 - pg_resqueue 20100129 */
relationId == ResQueueRsqnameIndexId || 
/* relation id: 6059 - pg_resourcetype 20100129 */
relationId == ResourceTypeOidIndexId || 
/* relation id: 6059 - pg_resourcetype 20100129 */
relationId == ResourceTypeRestypidIndexId || 
/* relation id: 6059 - pg_resourcetype 20100129 */
relationId == ResourceTypeResnameIndexId || 
/* relation id: 6060 - pg_resqueuecapability 20100129 */
relationId == ResQueueCapabilityOidIndexId || 
/* relation id: 6060 - pg_resqueuecapability 20100129 */
relationId == ResQueueCapabilityResqueueidIndexId || 
/* relation id: 6060 - pg_resqueuecapability 20100129 */
relationId == ResQueueCapabilityRestypidIndexId || 
/* relation id: 1260 - pg_authid 20100129 */
relationId == AuthIdRolnameIndexId || 
/* relation id: 1260 - pg_authid 20100129 */
relationId == AuthIdOidIndexId || 
/* relation id: 1260 - pg_authid 20100129 */
relationId == AuthIdRolResQueueIndexId || 
/* relation id: 5035 - gp_san_configuration 20101104 */
relationId == GpSanConfigMountidIndexId || 
/* relation id: 5000 - gp_configuration 20101104 */
relationId == GpConfigurationContentDefinedprimaryIndexId || 
/* relation id: 5000 - gp_configuration 20101104 */
relationId == GpConfigurationDbidIndexId || 
/* relation id: 5029 - gp_db_interfaces 20101104 */
relationId == GpDbInterfacesDbidIndexId || 
/* relation id: 5030 - gp_interfaces 20101104 */
relationId == GpInterfacesInterfaceidIndexId || 
/* relation id: 5036 - gp_segment_configuration 20101122 */
relationId == GpSegmentConfigContentPreferred_roleIndexId || 
/* relation id: 5036 - gp_segment_configuration 20101122 */
relationId == GpSegmentConfigDbidIndexId || 
/* relation id: 5033 - pg_filespace_entry 20101122 */
relationId == FileSpaceEntryFsefsoidIndexId || 
/* relation id: 5033 - pg_filespace_entry 20101122 */
relationId == FileSpaceEntryFsefsoidFsedbidIndexId || 


/* TIDYCAT_END_CODEGEN */
		0 /* OR ZERO */

			)
		return true;
	/* These are their toast tables and toast indexes (see toasting.h) */
	if (relationId == PgAuthidToastTable ||
		relationId == PgAuthidToastIndex ||
		relationId == PgDatabaseToastTable ||
		relationId == PgDatabaseToastIndex ||
		relationId == PgShdescriptionToastTable ||
		relationId == PgShdescriptionToastIndex ||

/* TIDYCAT_BEGIN_CODEGEN 
*/

/* relation id: 5036 - gp_segment_configuration 20101122 */
relationId == GpSegmentConfigToastTable || 
relationId == GpSegmentConfigToastIndex || 

/* relation id: 5033 - pg_filespace_entry 20101122 */
relationId == PgFileSpaceEntryToastTable || 
relationId == PgFileSpaceEntryToastIndex || 
/* TIDYCAT_END_CODEGEN */
		0 /* OR ZERO */
			)
		return true;
	return false;
}

/*
 * OIDs for catalog object are normally allocated in the master, and
 * executor nodes should just use the OIDs passed by the master. But
 * there are some exceptions.
 */
static bool
RelationNeedsSynchronizedOIDs(Relation relation)
{
	if (IsSystemNamespace(RelationGetNamespace(relation)))
	{
		switch(RelationGetRelid(relation))
		{
			/*
			 * pg_largeobject is more like a user table, and has
			 * different contents in each segment and master.
			 */
			case LargeObjectRelationId:
				return false;

			/*
			 * We don't currently synchronize the OIDs of these catalogs.
			 * It's a bit sketchy that we don't, but we get away with it
			 * because these OIDs don't appear in any of the Node structs
			 * that are dispatched from master to segments. (Except for the
			 * OIDs, the contents of these tables should be in sync.)
			 */
			case RewriteRelationId:
			case TriggerRelationId:
			case AccessMethodOperatorRelationId:
			case AccessMethodProcedureRelationId:
				return false;
		}

		/*
		 * All other system catalogs are assumed to need synchronized
		 * OIDs.
		 */
		return true;
	}
	return false;
}

/*
 * GetNewOid
 *		Generate a new OID that is unique within the given relation.
 *
 * Caller must have a suitable lock on the relation.
 *
 * Uniqueness is promised only if the relation has a unique index on OID.
 * This is true for all system catalogs that have OIDs, but might not be
 * true for user tables.  Note that we are effectively assuming that the
 * table has a relatively small number of entries (much less than 2^32)
 * and there aren't very long runs of consecutive existing OIDs.  Again,
 * this is reasonable for system catalogs but less so for user tables.
 *
 * Since the OID is not immediately inserted into the table, there is a
 * race condition here; but a problem could occur only if someone else
 * managed to cycle through 2^32 OIDs and generate the same OID before we
 * finish inserting our row.  This seems unlikely to be a problem.	Note
 * that if we had to *commit* the row to end the race condition, the risk
 * would be rather higher; therefore we use SnapshotDirty in the test,
 * so that we will see uncommitted rows.
 */
Oid
GetNewOid(Relation relation)
{
	Oid			newOid;
	Oid			oidIndex;
	Relation	indexrel;

	/* If relation doesn't have OIDs at all, caller is confused */
	Assert(relation->rd_rel->relhasoids);

	/* In bootstrap mode, we don't have any indexes to use */
	if (IsBootstrapProcessingMode())
		return GetNewObjectId();

	/* The relcache will cache the identity of the OID index for us */
	oidIndex = RelationGetOidIndex(relation);

	/* If no OID index, just hand back the next OID counter value */
	if (!OidIsValid(oidIndex))
	{
		/*
		 * System catalogs that have OIDs should *always* have a unique OID
		 * index; we should only take this path for user tables. Give a
		 * warning if it looks like somebody forgot an index.
		 */
		if (IsSystemRelation(relation))
			elog(WARNING, "generating possibly-non-unique OID for \"%s\"",
				 RelationGetRelationName(relation));

		return GetNewObjectId();
	}

	/* Otherwise, use the index to find a nonconflicting OID */
	indexrel = index_open(oidIndex, AccessShareLock);
	newOid = GetNewOidWithIndex(relation, indexrel);
	index_close(indexrel, AccessShareLock);

	/*
	 * Most catalog objects need to have the same OID in the master and all
	 * segments. When creating a new object, the master should allocate the
	 * OID and tell the segments to use the same, so segments should have no
	 * need to ever allocate OIDs on their own. Therefore, give a WARNING if
	 * GetNewOid() is called in a segment. (There are a few exceptions, see
	 * RelationNeedsSynchronizedOIDs()).
	 */
	if (Gp_role == GP_ROLE_EXECUTE && RelationNeedsSynchronizedOIDs(relation))
		elog(WARNING, "allocated OID %u for relation \"%s\" in segment",
			 newOid, RelationGetRelationName(relation));

	return newOid;
}

/*
 * GetNewOidWithIndex
 *		Guts of GetNewOid: use the supplied index
 *
 * This is exported separately because there are cases where we want to use
 * an index that will not be recognized by RelationGetOidIndex: TOAST tables
 * and pg_largeobject have indexes that are usable, but have multiple columns
 * and are on ordinary columns rather than a true OID column.  This code
 * will work anyway, so long as the OID is the index's first column.
 *
 * Caller must have a suitable lock on the relation.
 */
Oid
GetNewOidWithIndex(Relation relation, Relation indexrel)
{
	Oid			newOid;
	SnapshotData SnapshotDirty;
	IndexScanDesc scan;
	ScanKeyData key;
	bool		collides;

	InitDirtySnapshot(SnapshotDirty);

	/* Generate new OIDs until we find one not in the table */
	do
	{
		CHECK_FOR_INTERRUPTS();

		newOid = GetNewObjectId();

		ScanKeyInit(&key,
					(AttrNumber) 1,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(newOid));

		/* see notes above about using SnapshotDirty */
		scan = index_beginscan(relation, indexrel,
							   &SnapshotDirty, 1, &key);

		collides = HeapTupleIsValid(index_getnext(scan, ForwardScanDirection));

		index_endscan(scan);
	} while (collides);

	return newOid;
}

/*
 * GetNewRelFileNode
 *		Generate a new relfilenode number that is unique within the given
 *		tablespace.
 *
 * If the relfilenode will also be used as the relation's OID, pass the
 * opened pg_class catalog, and this routine will guarantee that the result
 * is also an unused OID within pg_class.  If the result is to be used only
 * as a relfilenode for an existing relation, pass NULL for pg_class.
 *
 * As with GetNewOid, there is some theoretical risk of a race condition,
 * but it doesn't seem worth worrying about.
 *
 * Note: we don't support using this in bootstrap mode.  All relations
 * created by bootstrap have preassigned OIDs, so there's no need.
 */
Oid
GetNewRelFileNode(Oid reltablespace, bool relisshared, Relation pg_class)
{
	RelFileNode rnode;
	char	   *rpath;
	int			fd;
	bool		collides;

	/* This should match RelationInitPhysicalAddr */
	rnode.spcNode = reltablespace ? reltablespace : MyDatabaseTableSpace;
	rnode.dbNode = relisshared ? InvalidOid : MyDatabaseId;

	do
	{
		CHECK_FOR_INTERRUPTS();

		/* Generate the OID */
		if (pg_class)
			rnode.relNode = GetNewOid(pg_class);
		else
			rnode.relNode = GetNewObjectId();

		/* Check for existing file of same name */
		rpath = relpath(rnode);
		fd = BasicOpenFile(rpath, O_RDONLY | PG_BINARY, 0);

		if (fd >= 0)
		{
			/* definite collision */
			gp_retry_close(fd);
			collides = true;
		}
		else
		{
			/*
			 * Here we have a little bit of a dilemma: if errno is something
			 * other than ENOENT, should we declare a collision and loop? In
			 * particular one might think this advisable for, say, EPERM.
			 * However there really shouldn't be any unreadable files in a
			 * tablespace directory, and if the EPERM is actually complaining
			 * that we can't read the directory itself, we'd be in an infinite
			 * loop.  In practice it seems best to go ahead regardless of the
			 * errno.  If there is a colliding file we will get an smgr
			 * failure when we attempt to create the new relation file.
			 */
			collides = false;
		}

		pfree(rpath);
	} while (collides);
	
	if (Gp_role == GP_ROLE_EXECUTE)
		Insist(!PointerIsValid(pg_class));

	elog(DEBUG1, "Calling GetNewRelFileNode in %s mode %s pg_class. "
		 "New relOid = %d", 
		 (Gp_role == GP_ROLE_EXECUTE ? "execute" :
		  Gp_role == GP_ROLE_UTILITY ? "utility" :
		  "dispatch"), pg_class ? "with" : "without",
		 rnode.relNode );

	return rnode.relNode;
}


bool
CheckNewRelFileNodeIsOk(Oid newOid, Oid reltablespace, bool relisshared, 
						Relation pg_class)
{
	RelFileNode rnode;
	char	   *rpath;
	int			fd;
	bool		collides;
	SnapshotData SnapshotDirty;

	InitDirtySnapshot(SnapshotDirty);

	if (pg_class)
	{
		Oid			oidIndex;
		Relation	indexrel;
		IndexScanDesc scan;
		ScanKeyData key;
	
		Assert(!IsBootstrapProcessingMode());
		Assert(pg_class->rd_rel->relhasoids);
	
		/* The relcache will cache the identity of the OID index for us */
		oidIndex = RelationGetOidIndex(pg_class);
	
		Assert(OidIsValid(oidIndex));
		
		indexrel = index_open(oidIndex, AccessShareLock);
		
		ScanKeyInit(&key,
					(AttrNumber) 1,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(newOid));

		scan = index_beginscan(pg_class, indexrel, &SnapshotDirty, 1, &key);

		collides = HeapTupleIsValid(index_getnext(scan, ForwardScanDirection));

		index_endscan(scan);
		
		index_close(indexrel, AccessShareLock);
		
		if (collides)
			elog(ERROR, "relfilenode %d already in use in \"pg_class\"",
				 newOid);	
		
	}

	/* This should match RelationInitPhysicalAddr */
	rnode.spcNode = reltablespace ? reltablespace : MyDatabaseTableSpace;
	rnode.dbNode = relisshared ? InvalidOid : MyDatabaseId;
	
	rnode.relNode = newOid;
	
	/* Check for existing file of same name */
	rpath = relpath(rnode);
	fd = BasicOpenFile(rpath, O_RDONLY | PG_BINARY, 0);

	if (fd >= 0)
	{
		/* definite collision */
		gp_retry_close(fd);
		collides = true;
	}
	else
		collides = false;

	pfree(rpath);
	
	if (collides && !relisshared)
		elog(ERROR, "oid %d already in use", newOid);	

	while(GetNewObjectId() < newOid);

	return !collides;
}

