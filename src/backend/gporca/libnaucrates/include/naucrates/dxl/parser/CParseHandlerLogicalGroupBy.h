//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp, Inc.
//
//	@filename:
//		CParseHandlerLogicalGroupBy.h
//
//	@doc:
//		Parse handler for parsing a logical GroupBy operator
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerLogicalGroupBy_H
#define GPDXL_CParseHandlerLogicalGroupBy_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLLogicalGet.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalGroupBy
//
//	@doc:
//		Parse handler for parsing a logical GroupBy operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalGroupBy : public CParseHandlerLogicalOp
{
private:
	// private copy ctor
	CParseHandlerLogicalGroupBy(const CParseHandlerLogicalGroupBy &);

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
	CParseHandlerLogicalGroupBy(CMemoryPool *mp,
								CParseHandlerManager *parse_handler_mgr,
								CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalGroupBy_H

// EOF
