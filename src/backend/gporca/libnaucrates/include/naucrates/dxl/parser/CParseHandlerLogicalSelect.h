//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerLogicalSelect.h
//
//	@doc:
//		Parse handler for parsing a logical Select operator
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerLogicalSelect_H
#define GPDXL_CParseHandlerLogicalSelect_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"
#include "naucrates/dxl/operators/CDXLLogicalGet.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalSelect
//
//	@doc:
//		Parse handler for parsing a logical Select operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalSelect : public CParseHandlerLogicalOp
{
private:
	// private copy ctor
	CParseHandlerLogicalSelect(const CParseHandlerLogicalSelect &);

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
	CParseHandlerLogicalSelect(CMemoryPool *mp,
							   CParseHandlerManager *parse_handler_mgr,
							   CParseHandlerBase *parse_handler_root);

	~CParseHandlerLogicalSelect();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalSelect_H

// EOF
