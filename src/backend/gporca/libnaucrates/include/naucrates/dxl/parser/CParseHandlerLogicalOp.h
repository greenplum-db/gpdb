//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerLogicalOp.h
//
//	@doc:
//		SAX parse handler class for parsing logical operators.
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerLogicalOp_H
#define GPDXL_CParseHandlerLogicalOp_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalOp
//
//	@doc:
//		Parse handler for logical operators
//
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalOp : public CParseHandlerOp
{
private:
protected:
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
	CParseHandlerLogicalOp(const CParseHandlerLogicalOp &) = delete;

	// ctor/dtor
	CParseHandlerLogicalOp(CMemoryPool *mp,
						   CParseHandlerManager *parse_handler_mgr,
						   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalOp_H

// EOF
