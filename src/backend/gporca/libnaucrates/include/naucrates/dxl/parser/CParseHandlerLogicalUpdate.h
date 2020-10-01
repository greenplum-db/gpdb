//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalUpdate.h
//
//	@doc:
//		Parse handler for parsing a logical update operator
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerLogicalUpdate_H
#define GPDXL_CParseHandlerLogicalUpdate_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalUpdate
//
//	@doc:
//		Parse handler for parsing a logical update operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalUpdate : public CParseHandlerLogicalOp
{
private:
	// ctid column id
	ULONG m_ctid_colid;

	// segmentId column id
	ULONG m_segid_colid;

	// delete col ids
	ULongPtrArray *m_deletion_colid_array;

	// insert col ids
	ULongPtrArray *m_insert_colid_array;

	// does update preserve oids
	BOOL m_preserve_oids;

	// tuple oid column id
	ULONG m_tuple_oid_col_oid;

	// private copy ctor
	CParseHandlerLogicalUpdate(const CParseHandlerLogicalUpdate &);

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
	CParseHandlerLogicalUpdate(CMemoryPool *mp,
							   CParseHandlerManager *parse_handler_mgr,
							   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalUpdate_H

// EOF
