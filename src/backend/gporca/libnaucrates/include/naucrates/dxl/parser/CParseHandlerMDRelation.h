//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerMDRelation.h
//
//	@doc:
//		SAX parse handler class for relation metadata
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDRelation_H
#define GPDXL_CParseHandlerMDRelation_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/md/CMDPartConstraintGPDB.h"
#include "naucrates/md/CMDRelationGPDB.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMDRelation
//
//	@doc:
//		Parse handler for relation metadata
//
//---------------------------------------------------------------------------
class CParseHandlerMDRelation : public CParseHandlerMetadataObject
{
protected:
	// id and version of the relation
	IMDId *m_mdid;

	// schema name
	CMDName *m_mdname_schema;

	// table name
	CMDName *m_mdname;

	// is this a temporary relation
	BOOL m_is_temp_table;

	// storage type
	IMDRelation::Erelstoragetype m_rel_storage_type;

	// distribution policy
	IMDRelation::Ereldistrpolicy m_rel_distr_policy;

	// distribution columns
	ULongPtrArray *m_distr_col_array;

	// do we need to consider a hash distributed table as random distributed
	BOOL m_convert_hash_to_random;

	// partition keys
	ULongPtrArray *m_partition_cols_array;

	// partition types
	CharPtrArray *m_str_part_types_array;

	// key sets
	ULongPtr2dArray *m_key_sets_arrays;

	// part constraint
	CDXLNode *m_part_constraint;

	// distribution opfamilies parse handler
	CParseHandlerBase *m_opfamilies_parse_handler;

	// child partition oids parse handler
	CParseHandlerBase *m_child_partitions_parse_handler;

	// is part constraint unbounded
	BOOL m_part_constraint_unbounded;

	// foreign server (if exists)
	IMDId *m_foreign_server;

	// helper function to parse main relation attributes: name, id,
	// distribution policy and keys
	void ParseRelationAttributes(const Attributes &attrs,
								 Edxltoken target_elem);

	// create and activate the parse handler for the children nodes
	void ParseChildNodes();

private:
	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
		) override;

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
		) override;

public:
	CParseHandlerMDRelation(const CParseHandlerMDRelation &) = delete;

	// ctor
	CParseHandlerMDRelation(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMDRelation_H

// EOF
