/*-------------------------------------------------------------------------
 *
 * oid_dispatch.c
 *		Functions to ensure that QD and QEs use same OIDs for catalog objects.
 *
 *
 * In Greenplum, it's important that most objects, like relations,
 * functions, operators, have the same OIDs in the master and all QE nodes.
 * Otherwise query plans generated in the master will not work on the QE
 * nodes, because they use the master's OIDs to refer to objects.
 *
 * Whenever a CREATE statement, or any other command that creates new
 * objects, is dispatched, the master also needs to tell the QE servers which
 * OIDs to use for the new objects. Before GPDB 5.0, that was done by
 * modifying all the structs representing DDL statements, like DefineStmt,
 * CreateOpClassStmt, and so forth, by adding a new OID field to them.
 * However, that was annoying when merging with the upstream, because it
 * required scattered changes to all the structs, and the accompanying
 * routines to copy and serialize them. Moreover, for more complicated object
 * types, like a table, a single OID was not enough, as CREATE TABLE not only
 * creates the entry in pg_class, it also creates a composite type, an array
 * type for the composite type, and possibly the same for the associated
 * toast table.
 *
 * Starting with GPDB 5.0, we take a different tact. Whenever an OID is
 * assigned for a system table in the QD node, the OID is recorded in private
 * memory, in the 'dispatch_oids' list, along with the key for that object.
 * AddDispatchOidFromTuple() function does that, and it's called from
 * heap_insert(). For example, when a new type is created, the OID of the new
 * type, and the namespace and the type name of the new type, are recorded in
 * the 'dispatch_oids' list.  When the command is dispatched to the QE
 * servers, all the recorded OIDs are included in the dispatched request, and
 * the QE processes in turn stash the list into backend-private memory. This
 * is the 'preassigned_oids' list.
 *
 * In a QE node, when we are about to create a new object, and assign an OID
 * for it, we look into the 'preassigned_oids' list to see if we had received
 * an OID to use for the named object from the master. Under normal
 * circumstances, we should have a pre-assigned OID for all objects created
 * in QEs, and the GetPreassigned*() functions will throw an error if we
 * don't.
 *
 * All in all, this provides a generic mechanism for DDL commands, to record
 * OIDs that are assigned for new objects in the master, transfer them to QE
 * nodes when the DDL command is dispatched, and for the QE nodes to use the
 * same, pre-assigned, OIDs for the objects.
 *
 * This same mechanism can be used to preserve OIDs during pg_upgrade. In
 * PostgreSQL, pg_upgrade only needs to preserve the OIDs of a few objects,
 * like types, but in GPDB we need to preserve most OIDs, because they need
 * to be kept in sync between the nodes. (Strictly speaking, we only need to
 * ensure that all the nodes use the same OIDs in the upgraded clusters, but
 * they wouldn't need to be the same as before upgrade. However, the most
 * straightforward way to achieve that is to use the same OIDs as before
 * upgrade.)
 *
 * pg_upgrade has its own mechanism to record the OIDs from the old cluster
 * but when restoring the schema in the new cluster, it uses the same
 * 'preassigned_oids' list to restore them, that we use to assign specific
 * OIDs in a QE node at dispatch.
 *
 * (XXX: All the pg_upgrade code described above is to-be-done, as of
 * this writing),
 *
 * Portions Copyright 2016 Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/oid_dispatch.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_attrdef.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_enum.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_extprotocol.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_language.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_partition.h"
#include "catalog/pg_partition_rule.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_resqueue.h"
#include "catalog/pg_resgroup.h"
#include "catalog/pg_rewrite.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_ts_parser.h"
#include "catalog/pg_ts_template.h"
#include "catalog/pg_type.h"
#include "catalog/pg_user_mapping.h"
#include "catalog/oid_dispatch.h"
#include "cdb/cdbvars.h"
#include "nodes/pg_list.h"
#include "executor/execdesc.h"
#include "utils/memutils.h"
#include "miscadmin.h"

/* #define OID_DISPATCH_DEBUG */

/*
 * These were received from the QD, and should be consumed by the current
 * statement.
 */
static List *preassigned_oids = NIL;

/*
 * These will be sent to the QEs on next CdbDispatchUtilityStatement call.
 */
static List *dispatch_oids = NIL;


/*
 * Create an OidAssignment struct, for a catalog table tuple.
 *
 * When a new tuple is inserted in the master, this is used to construct the
 * OidAssignment struct to dispatch to the QEs. In the QEs, this is used to
 * construct a search key, in the GetPreassignedOidForXXX() functions.
 *
 * On return, those "key" fields in the returned OidAssignment struct are
 * filled in that are applicable for this type of object. Others are reset to
 * 0. If the catalog table does not require OID synchronization, *exempt is
 * set to true. If the catalog table is not recognized, *recognized is set to
 * false.
 */
static OidAssignment
CreateKeyFromCatalogTuple(Relation catalogrel, HeapTuple tuple,
						  bool *recognized, bool *exempt)
{
	OidAssignment key;

	*recognized = true;
	*exempt = false;

	memset(&key, 0, sizeof(OidAssignment));
	key.type = T_OidAssignment;
	key.catalog = catalogrel->rd_id;

	switch(catalogrel->rd_id)
	{
		case AccessMethodOperatorRelationId:
			{
				Form_pg_amop amopForm = (Form_pg_amop) GETSTRUCT(tuple);

				key.keyOid1 = amopForm->amopmethod;
				break;
			}
		case AttrDefaultRelationId:
			{
				Form_pg_attrdef adForm = (Form_pg_attrdef) GETSTRUCT(tuple);

				key.keyOid1 = adForm->adrelid;
				key.keyOid2 = (Oid) adForm->adnum;
				break;
			}
		case AuthIdRelationId:
			{
				Form_pg_authid rolForm = (Form_pg_authid) GETSTRUCT(tuple);

				key.objname = (char *) NameStr(rolForm->rolname);
				break;
			}
		case CastRelationId:
			{
				Form_pg_cast castForm = (Form_pg_cast) GETSTRUCT(tuple);

				key.keyOid1 = castForm->castsource;
				key.keyOid2 = castForm->casttarget;
				break;
			}
		case ConstraintRelationId:
			{
				Form_pg_constraint conForm = (Form_pg_constraint) GETSTRUCT(tuple);

				key.namespaceOid = conForm->connamespace;
				key.objname = NameStr(conForm->conname);
				key.keyOid1 = conForm->conrelid;
				key.keyOid2 = conForm->contypid;
				break;
			}
		case ConversionRelationId:
			{
				Form_pg_conversion conForm = (Form_pg_conversion) GETSTRUCT(tuple);

				key.namespaceOid = conForm->connamespace;
				key.objname = NameStr(conForm->conname);
				break;
			}
		case DatabaseRelationId:
			{
				Form_pg_database datForm = (Form_pg_database) GETSTRUCT(tuple);

				key.objname = (char *) NameStr(datForm->datname);
				break;
			}
		case EnumRelationId:
			{
				Form_pg_enum enumForm = (Form_pg_enum) GETSTRUCT(tuple);

				key.keyOid1 = enumForm->enumtypid;
				key.objname = NameStr(enumForm->enumlabel);
				break;
			}
		case ExtensionRelationId:
			{
				Form_pg_extension extForm = (Form_pg_extension) GETSTRUCT(tuple);

				/*
				 * Note that unlike most catalogs with a "namespace" column,
				 * extnamespace is not meant to imply that the extension
				 * belongs to that schema.
				 */
				key.objname = NameStr(extForm->extname);
				break;
			}
		case ExtprotocolRelationId:
			{
				Form_pg_extprotocol protForm = (Form_pg_extprotocol) GETSTRUCT(tuple);

				key.objname = NameStr(protForm->ptcname);
				break;
			}
		case ForeignDataWrapperRelationId:
			{
				Form_pg_foreign_data_wrapper fdwForm = (Form_pg_foreign_data_wrapper) GETSTRUCT(tuple);

				key.keyOid1 = fdwForm->fdwowner;
				key.objname = NameStr(fdwForm->fdwname);
				break;
			}
		case ForeignServerRelationId:
			{
				Form_pg_foreign_server fsrvForm = (Form_pg_foreign_server) GETSTRUCT(tuple);

				key.keyOid1 = fsrvForm->srvfdw;
				key.objname = NameStr(fsrvForm->srvname);
				break;
			}
		case LanguageRelationId:
			{
				Form_pg_language lanForm = (Form_pg_language) GETSTRUCT(tuple);

				key.objname = NameStr(lanForm->lanname);
				break;
			}
		case NamespaceRelationId:
			{
				Form_pg_namespace nspForm = (Form_pg_namespace) GETSTRUCT(tuple);

				key.objname = NameStr(nspForm->nspname);
				break;
			}

		case OperatorRelationId:
			{
				Form_pg_operator oprForm = (Form_pg_operator) GETSTRUCT(tuple);

				key.namespaceOid = oprForm->oprnamespace;
				key.objname = NameStr(oprForm->oprname);
				break;
			}
		case OperatorClassRelationId:
			{
				Form_pg_opclass opcForm = (Form_pg_opclass) GETSTRUCT(tuple);

				key.namespaceOid = opcForm->opcnamespace;
				key.objname = NameStr(opcForm->opcname);
				break;
			}
		case OperatorFamilyRelationId:
			{
				Form_pg_opfamily opfForm = (Form_pg_opfamily) GETSTRUCT(tuple);

				key.namespaceOid = opfForm->opfnamespace;
				key.objname = NameStr(opfForm->opfname);
				break;
			}
		case ProcedureRelationId:
			{
				Form_pg_proc proForm = (Form_pg_proc) GETSTRUCT(tuple);

				key.namespaceOid = proForm->pronamespace;
				key.objname = NameStr(proForm->proname);
				break;
			}
		case RelationRelationId:
			{
				Form_pg_class relForm = (Form_pg_class) GETSTRUCT(tuple);

				key.namespaceOid = relForm->relnamespace;
				key.objname = NameStr(relForm->relname);
				break;
			}
		case ResQueueRelationId:
			{
				Form_pg_resqueue rsqForm = (Form_pg_resqueue) GETSTRUCT(tuple);

				key.objname = NameStr(rsqForm->rsqname);
				break;
			}
		case ResQueueCapabilityRelationId:
			{
				Form_pg_resqueuecapability rqcForm = (Form_pg_resqueuecapability) GETSTRUCT(tuple);

				key.keyOid1 = rqcForm->resqueueid;
				key.keyOid2 = (Oid) rqcForm->restypid;
				break;
			}
		case RewriteRelationId:
			{
				Form_pg_rewrite rewriteForm = (Form_pg_rewrite) GETSTRUCT(tuple);

				key.keyOid1 = rewriteForm->ev_class;
				key.objname = NameStr(rewriteForm->rulename);
				break;
			}
		case TableSpaceRelationId:
			{
				Form_pg_tablespace spcForm = (Form_pg_tablespace) GETSTRUCT(tuple);

				key.objname = NameStr(spcForm->spcname);
				break;
			}
		case TSParserRelationId:
			{
				Form_pg_ts_parser prsForm = (Form_pg_ts_parser) GETSTRUCT(tuple);

				key.namespaceOid = prsForm->prsnamespace;
				key.objname = NameStr(prsForm->prsname);
				break;
			}
		case TSDictionaryRelationId:
			{
				Form_pg_ts_dict dictForm = (Form_pg_ts_dict) GETSTRUCT(tuple);

				key.namespaceOid = dictForm->dictnamespace;
				key.objname = NameStr(dictForm->dictname);
				break;
			}
		case TSTemplateRelationId:
			{
				Form_pg_ts_template tmplForm = (Form_pg_ts_template) GETSTRUCT(tuple);

				key.namespaceOid = tmplForm->tmplnamespace;
				key.objname = NameStr(tmplForm->tmplname);
				break;
			}
		case TSConfigRelationId:
			{
				Form_pg_ts_config cfgForm = (Form_pg_ts_config) GETSTRUCT(tuple);

				key.namespaceOid = cfgForm->cfgnamespace;
				key.objname = NameStr(cfgForm->cfgname);
				break;
			}
		case TypeRelationId:
			{
				Form_pg_type typForm = (Form_pg_type) GETSTRUCT(tuple);

				key.namespaceOid = typForm->typnamespace;
				key.objname = NameStr(typForm->typname);
				break;
			}

		case ResGroupRelationId:
			{
				Form_pg_resgroup rsgForm = (Form_pg_resgroup) GETSTRUCT(tuple);

				key.objname = NameStr(rsgForm->rsgname);
				break;
			}
		case ResGroupCapabilityRelationId:
			{
				Form_pg_resgroupcapability rsgCapForm = (Form_pg_resgroupcapability) GETSTRUCT(tuple);

				key.keyOid1 = rsgCapForm->resgroupid;
				key.keyOid2 = (Oid) rsgCapForm->reslimittype;
				break;
			}
		case UserMappingRelationId:
			{
				Form_pg_user_mapping usermapForm = (Form_pg_user_mapping) GETSTRUCT(tuple);

				key.keyOid1 = usermapForm->umuser;
				key.keyOid2 = usermapForm->umserver;
				break;
			}

		/* These tables don't need to have their OIDs synchronized. */
		case AccessMethodProcedureRelationId:
		case PartitionRelationId:
		case PartitionRuleRelationId:
			*exempt = true;
			 break;

		 /*
		  * These objects need to have their OIDs synchronized, but there is bespoken
		  * code to deal with it.
		  */
		case TriggerRelationId:
			*exempt = true;
			 break;

		default:
			*recognized = false;
			break;
	}
	return key;
}

/* ----------------------------------------------------------------
 * Functions for use in QE.
 * ----------------------------------------------------------------
 */

/*
 * Remember a list of pre-assigned OIDs, to be consumed later in the
 * transaction, when those system objects are created.
 */
void
AddPreassignedOids(List *l)
{
	ListCell *lc;
	MemoryContext old_context;

	if (IsBinaryUpgrade)
		elog(ERROR, "AddPreassignedOids called during binary upgrade");

	/*
	 * In the master, the OID-assignment-list is usually included in the next
	 * command that is dispatched, after an OID was assigned. In almost all
	 * cases, the dispatched command is the same CREATE command for which the
	 * oid was assigned. But I'm not sure if that's true for *all* commands,
	 * and so we don't require it. It is OK if an OID assignment is included
	 * in one dispatched command, but the command that needs the OID is only
	 * dispatched later in the same transaction. Therefore, keep the
	 * 'preassigned_oids' list in TopTransactionContext.
	 */
	old_context = MemoryContextSwitchTo(TopTransactionContext);

	foreach(lc, l)
	{
		OidAssignment *p = (OidAssignment *) lfirst(lc);

		p = copyObject(p);
		preassigned_oids = lappend(preassigned_oids, p);

#ifdef OID_DISPATCH_DEBUG
		elog(NOTICE, "received OID assignment: catalog %u, namespace: %u, name: \"%s\": %u",
			 p->catalog, p->namespaceOid, p->objname ? p->objname : "", p->oid);
#endif
	}
	MemoryContextSwitchTo(old_context);

}

/*
 * For pg_upgrade_support functions, created during a binary upgrade, use OIDs
 * from a special reserved block of OIDs. We cannot use "normal" OIDs for these,
 * as they may collide with actual user objects restored later in the upgrade
 * process.
 */
static Oid BinaryUpgradeSchemaReservedOid = FirstBinaryUpgradeReservedObjectId;
static Oid NextBinaryUpgradeReservedOid = FirstBinaryUpgradeReservedObjectId + 1;

static Oid
AssignBinaryUpgradeReservedOid()
{
	Oid		result = NextBinaryUpgradeReservedOid;

	if (result > LastBinaryUpgradeReservedObjectId)
		elog(ERROR, "out of OIDs reserved for binary-upgrade");

	NextBinaryUpgradeReservedOid++;

	return result;
}

/*
 * Helper routine for GetPreassignedOidFor*() functions. Finds an entry in the
 * 'preassigned_oids' list with the given search key.
 *
 * If found, removes the entry from the list, and returns the OID. If not
 * found, returns InvalidOid.
 */
static Oid
GetPreassignedOid(OidAssignment *searchkey)
{
	ListCell   *cur_item;
	ListCell   *prev_item;
	Oid			oid;

	/*
	 * For binary_upgrade schema, and any functions in it, use OIDs
	 * from the reserved block.
	 */
	if (IsBinaryUpgrade)
	{
		if (searchkey->catalog == NamespaceRelationId &&
			strcmp(searchkey->objname, "binary_upgrade") == 0)
			return BinaryUpgradeSchemaReservedOid;
		if (searchkey->catalog == ProcedureRelationId &&
			searchkey->namespaceOid == BinaryUpgradeSchemaReservedOid)
			return AssignBinaryUpgradeReservedOid();
	}

	prev_item = NULL;
	cur_item = list_head(preassigned_oids);

	while (cur_item != NULL)
	{
		OidAssignment *p = (OidAssignment *) lfirst(cur_item);

		if (searchkey->catalog == p->catalog &&
			(searchkey->objname == NULL ||
			 (p->objname != NULL && strcmp(searchkey->objname, p->objname) == 0)) &&
			searchkey->namespaceOid == p->namespaceOid &&
			searchkey->keyOid1 == p->keyOid1 &&
			searchkey->keyOid2 == p->keyOid2)
		{
#ifdef OID_DISPATCH_DEBUG
			elog(NOTICE, "using OID assignment: catalog %u, namespace: %u, name: \"%s\": %u",
				 p->catalog, p->namespaceOid, p->objname ? p->objname : "", p->oid);
#endif

			oid = p->oid;
			preassigned_oids = list_delete_cell(preassigned_oids, cur_item, prev_item);
			pfree(p);
			return oid;
		}
		prev_item = cur_item;
		cur_item = lnext(cur_item);
	}

	return InvalidOid;
}

/*
 * Get a pre-assigned OID for a tuple that's being inserted to a system catalog.
 *
 * If the catalog table doesn't need OID synchronization across nodes, returns
 * InvalidOid. (The caller should assign a new OID, with GetNewOid(), in that
 * case). If the table requires synchronized OIDs, but no pre-assigned OID for
 * the given object is found, throws an error.
 */
Oid
GetPreassignedOidForTuple(Relation catalogrel, HeapTuple tuple)
{
	OidAssignment searchkey;
	bool		found;
	bool		exempt;
	Oid			oid;

	searchkey = CreateKeyFromCatalogTuple(catalogrel, tuple, &found, &exempt);
	if (exempt)
		return InvalidOid;
	if (!found)
		elog(ERROR, "pre-assigned OID requested for unrecognized system catalog \"%s\"",
			 RelationGetRelationName(catalogrel));

	if ((oid = GetPreassignedOid(&searchkey)) == InvalidOid)
	{
		bool		missing_ok = false;

		/*
		 * When binary-upgrading the QD node, we must preserve the OIDs of types,
		 * relations from the old cluster, so we should have pre-assigned OIDs for
		 * them.
		 *
		 * When binary-upgrading a QE node, we should have pre-assigned OIDs for
		 * all objects, to ensure that the same OIDs are used for all objects
		 * between the QD and the QEs.
		 */
		if (IsBinaryUpgrade && !IsBinaryUpgradeQE())
		{
			if (RelationGetRelid(catalogrel) == NamespaceRelationId)
			{
				/*
				 * OIDs of schemas must be preserved. (Only because namespace
				 * OIDs are part of the key of types and relations.) The only
				 * exception is pg_temp which we exempt (by definition).
				 */
				if (strncmp(searchkey.objname, "pg_temp", strlen("pg_temp")) == 0 ||
					strncmp(searchkey.objname, "pg_toast_temp", strlen("pg_toast_temp")) == 0)
					missing_ok = true;
				else
					missing_ok = false;
			}
			else if (RelationGetRelid(catalogrel) == TypeRelationId)
			{
				/* No need to preserve the rowtype OIDs of these special relations. */
				if (searchkey.namespaceOid == PG_BITMAPINDEX_NAMESPACE)
					missing_ok = true;
				if (searchkey.namespaceOid == PG_TOAST_NAMESPACE)
					missing_ok = true;
				if (searchkey.namespaceOid == PG_AOSEGMENT_NAMESPACE)
					missing_ok = true;
			}
			else if (RelationGetRelid(catalogrel) == RelationRelationId)
			{
				/* pg_upgrading indexes is currently not supported, so this is OK */
				if (searchkey.namespaceOid == PG_BITMAPINDEX_NAMESPACE)
					missing_ok = true;
			}
			else
			{
				missing_ok = true;
			}
		}

		if (!missing_ok)
			elog(ERROR, "no pre-assigned OID for %s tuple \"%s\" (namespace:%u keyOid1:%u keyOid2:%u)",
				 RelationGetRelationName(catalogrel), searchkey.objname ? searchkey.objname : "",
				 searchkey.namespaceOid, searchkey.keyOid1, searchkey.keyOid2);
	}
	return oid;
}

/*
 * A specialized version of GetPreassignedOidForTuple(). To be used when we don't
 * have a whole pg_database tuple yet.
 */
Oid
GetPreassignedOidForDatabase(const char *datname)
{
	OidAssignment searchkey;
	Oid			oid;

	memset(&searchkey, 0, sizeof(OidAssignment));
	searchkey.catalog = DatabaseRelationId;
	searchkey.objname = (char *) datname;

	if ((oid = GetPreassignedOid(&searchkey)) == InvalidOid)
		elog(ERROR, "no pre-assigned OID for database \"%s\"", datname);
	return oid;
}

/*
 * A specialized version of GetPreassignedOidForTuple(). To be used when we don't
 * have a whole pg_class tuple yet.
 */
Oid
GetPreassignedOidForRelation(Oid namespaceOid, const char *relname)
{
	OidAssignment searchkey;
	Oid			oid;

	memset(&searchkey, 0, sizeof(OidAssignment));
	searchkey.catalog = RelationRelationId;
	searchkey.namespaceOid = namespaceOid;
	searchkey.objname = (char *) relname;

	if ((oid = GetPreassignedOid(&searchkey)) == InvalidOid)
	{
		/*
		 * Special handling for binary upgrading the QD node. See
		 * GetPreassignedOidForTuple().
		 */
		if (IsBinaryUpgrade && !IsBinaryUpgradeQE())
		{
			if (namespaceOid == PG_BITMAPINDEX_NAMESPACE)
				return InvalidOid;

			if (namespaceOid == PG_AOSEGMENT_NAMESPACE)
				return InvalidOid;
		}
		elog(ERROR, "no pre-assigned OID for relation \"%s\"", relname);
	}
	return oid;
}

/*
 * A specialized version of GetPreassignedOidForTuple(). To be used when we don't
 * have a whole pg_type tuple yet.
 */
Oid
GetPreassignedOidForType(Oid namespaceOid, const char *typname)
{
	OidAssignment searchkey;
	Oid			oid;

	memset(&searchkey, 0, sizeof(OidAssignment));
	searchkey.catalog = TypeRelationId;
	searchkey.namespaceOid = namespaceOid;
	searchkey.objname = (char *) typname;

	if ((oid = GetPreassignedOid(&searchkey)) == InvalidOid)
		elog(ERROR, "no pre-assigned OID for type \"%s\"", typname);
	return oid;
}

/* ----------------------------------------------------------------
 * Functions for use in binary-upgrade mode
 * ----------------------------------------------------------------
 */

/*
 * Remember an OID which is set from loading a database dump performed
 * using the binary-upgrade flag.
 */
void
AddPreassignedOidFromBinaryUpgrade(Oid oid, Oid catalog, char *objname,
								   Oid namespaceOid, Oid keyOid1, Oid keyOid2)
{
	OidAssignment assignment;
	MemoryContext oldcontext;

	if (!IsBinaryUpgrade)
		elog(ERROR, "AddPreassignedOidFromBinaryUpgrade called, but not in binary upgrade mode");

	if (catalog == InvalidOid)
		elog(ERROR, "AddPreassignedOidFromBinaryUpgrade called with Invalid catalog relation Oid");

	if (Gp_role != GP_ROLE_UTILITY)
	{
		/* Perhaps we should error out and shut down here? */
		return;
	}

	memset(&assignment, 0, sizeof(OidAssignment));
	assignment.type = T_OidAssignment;

	/*
	 * This is essentially mimicking CreateKeyFromCatalogTuple except we set
	 * the members directly from the binary_upgrade function
	 */
	assignment.oid = oid;
	assignment.catalog = catalog;
	if (objname != NULL)
		assignment.objname = objname;
	if (namespaceOid != InvalidOid)
		assignment.namespaceOid = namespaceOid;
	if (keyOid1)
		assignment.keyOid1 = keyOid1;
	if (keyOid2)
		assignment.keyOid2 = keyOid2;

	/*
	 * Note that in binary-upgrade mode, the OID pre-assign calls are not done in
	 * the same transactions as the DDL commands that consume the OIDs. Hence they
	 * need to survive end-of-xact.
	 */
	oldcontext = MemoryContextSwitchTo(TopMemoryContext);

	preassigned_oids = lappend(preassigned_oids, copyObject(&assignment));

	MemoryContextSwitchTo(oldcontext);

#ifdef OID_DISPATCH_DEBUG
	elog(WARNING, "adding OID assignment: catalog \"%u\", namespace: %u, name: \"%s\": %u",
		 assignment.catalog,
		 assignment.namespaceOid,
		 assignment.objname ? assignment.objname : "",
		 assignment.oid);
#endif
}

/* ----------------------------------------------------------------
 * Functions for use in the master node.
 * ----------------------------------------------------------------
 */

/*
 * Remember a newly assigned OID, to be included in the next command
 * that's dispatched to QEs.
 */
void
AddDispatchOidFromTuple(Relation catalogrel, HeapTuple tuple)
{
	OidAssignment assignment;
	MemoryContext oldcontext;
	bool		found;
	bool		exempt;

	if (Gp_role == GP_ROLE_EXECUTE || IsBootstrapProcessingMode())
		return;

	assignment = CreateKeyFromCatalogTuple(catalogrel, tuple, &found, &exempt);
	if (exempt)
		return;
	if (!found)
	{
		elog(WARNING, "OID assigned for tuple in unrecognized system catalog \"%s\"",
			 RelationGetRelationName(catalogrel));
		return;
	}

	oldcontext = MemoryContextSwitchTo(TopTransactionContext);

	assignment.oid = HeapTupleGetOid(tuple);
	dispatch_oids = lappend(dispatch_oids, copyObject(&assignment));

	MemoryContextSwitchTo(oldcontext);

#ifdef OID_DISPATCH_DEBUG
	elog(NOTICE, "adding OID assignment: catalog \"%s\", namespace: %u, name: \"%s\": %u",
		 RelationGetRelationName(catalogrel),
		 assignment.namespaceOid,
		 assignment.objname ? assignment.objname : "",
		 assignment.oid);
#endif
}

/*
 * Get list of OIDs assigned in this transaction, since the last call.
 */
List *
GetAssignedOidsForDispatch(void)
{
	List	   *l;

	l = dispatch_oids;
	dispatch_oids = NIL;
	return l;
}

/*
 * Called at end-of-transaction. There is normally nothing to do,
 * but we perform some sanity checks.
 */
void
AtEOXact_DispatchOids(bool isCommit)
{
	/*
	 * Reset the list of to-be-dispatched OIDs. (in QD)
	 *
	 * All the OID assignments should've been dispatched before end of
	 * transaction. Unless we're aborting.
	 */
	if (isCommit && dispatch_oids)
	{
		while (dispatch_oids)
		{
			OidAssignment *p = (OidAssignment *) linitial(dispatch_oids);

			elog(WARNING, "OID assignment not dispatched: catalog %u, namespace: %u, name: \"%s\"",
				 p->catalog, p->namespaceOid, p->objname ? p->objname : "");
			dispatch_oids = list_delete_first(dispatch_oids);
		}
		elog(ERROR, "oids were assigned, but not dispatched to QEs");
	}
	else
	{
		/* The list will be free'd with the memory context. */
		dispatch_oids = NIL;
	}

	/*
	 * Reset the list of pre-assigned OIDs (in QE).
	 *
	 * Normally, at commit, all the pre-assigned OIDs should be consumed
	 * already. But there are some corner cases where they're not. For
	 * example, when running a CREATE TABLE AS SELECT query, the command
	 * might be dispatched to multiple slices, but only the QE writer
	 * processes create the table. In the other processes, the OIDs will
	 * go unused.
	 *
	 * In binary-upgrade mode, however, the OID pre-assign calls are not
	 * done in the same transactions as the DDL commands that consume
	 * the OIDs. Hence they need to survive end-of-xact.
	 */
	if (!IsBinaryUpgrade)
	{
#ifdef OID_DISPATCH_DEBUG
		while (preassigned_oids)
		{
			OidAssignment *p = (OidAssignment *) linitial(preassigned_oids);

			elog(NOTICE, "unused pre-assigned OID: catalog %u, namespace: %u, name: \"%s\"",
				 p->catalog, p->namespaceOid, p->objname);
			preassigned_oids = list_delete_first(preassigned_oids);
		}
#endif
		preassigned_oids = NIL;
	}
}


/*
 * Is the given OID reserved for some other object?
 *
 * This is mainly of concern during binary upgrade, where we preassign
 * all the OIDs at the beginning of a restore. During normal operation,
 * there should be no clashes anyway.
 */
bool
IsOidAcceptable(Oid oid)
{
	ListCell *lc;

	foreach(lc, preassigned_oids)
	{
		OidAssignment *p = (OidAssignment *) lfirst(lc);

		if (p->oid == oid)
			return false;
	}

	return true;
}
