//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalInsert.h
//
//	@doc:
//		Parse handler for parsing a logical insert operator
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerLogicalInsert_H
#define GPDXL_CParseHandlerLogicalInsert_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalInsert
//
//	@doc:
//		Parse handler for parsing a logical insert operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalInsert : public CParseHandlerLogicalOp
{
private:
	// source col ids
	ULongPtrArray *m_pdrgpul;

	// private copy ctor
	CParseHandlerLogicalInsert(const CParseHandlerLogicalInsert &);

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
	// ctor/dtor
	CParseHandlerLogicalInsert(CMemoryPool *mp,
							   CParseHandlerManager *parse_handler_mgr,
							   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalInsert_H

// EOF
