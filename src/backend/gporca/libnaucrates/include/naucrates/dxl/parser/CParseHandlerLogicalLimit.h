//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp, Inc.
//
//	@filename:
//		CParseHandlerLogicalLimit.h
//
//	@doc:
//		Parse handler for parsing a logical limit operator
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerLogicalLimit_H
#define GPDXL_CParseHandlerLogicalLimit_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"
#include "naucrates/dxl/operators/CDXLNode.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalLimit
//
//	@doc:
//		Parse handler for parsing a logical limit operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalLimit : public CParseHandlerLogicalOp
{
private:
	// private copy ctor
	CParseHandlerLogicalLimit(const CParseHandlerLogicalLimit &);

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
	CParseHandlerLogicalLimit(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalLimit_H

// EOF
