//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerMDRelationExternal.h
//
//	@doc:
//		SAX parse handler class for external relation metadata
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDRelationExternal_H
#define GPDXL_CParseHandlerMDRelationExternal_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMDRelation.h"
#include "naucrates/md/CMDRelationExternalGPDB.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMDRelationExternal
//
//	@doc:
//		Parse handler for external relation metadata
//
//---------------------------------------------------------------------------
class CParseHandlerMDRelationExternal : public CParseHandlerMDRelation
{
private:
	// reject limit
	INT m_reject_limit;

	// reject limit in rows?
	BOOL m_is_rej_limit_in_rows;

	// format error table mdid
	IMDId *m_mdid_fmt_err_table;

	// distribution opfamilies parse handler
	CParseHandlerBase *m_opfamilies_parse_handler;

	// part constraint
	CMDPartConstraintGPDB *m_part_constraint;

	// levels that include default partitions
	ULongPtrArray *m_level_with_default_part_array;

	// is part constraint unbounded
	BOOL m_part_constraint_unbounded;

	// private copy ctor
	CParseHandlerMDRelationExternal(const CParseHandlerMDRelationExternal &);

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);


public:
	// ctor
	CParseHandlerMDRelationExternal(CMemoryPool *mp,
									CParseHandlerManager *parse_handler_mgr,
									CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMDRelationExternal_H

// EOF
