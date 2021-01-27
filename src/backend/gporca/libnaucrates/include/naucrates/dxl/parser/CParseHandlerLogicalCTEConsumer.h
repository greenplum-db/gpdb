//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalCTEConsumer.h
//
//	@doc:
//		Parse handler for parsing a logical CTE Consumer operator
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerLogicalCTEConsumer_H
#define GPDXL_CParseHandlerLogicalCTEConsumer_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalCTEConsumer
//
//	@doc:
//		Parse handler for parsing a logical CTE Consumer operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalCTEConsumer : public CParseHandlerLogicalOp
{
private:
	// private copy ctor
	CParseHandlerLogicalCTEConsumer(const CParseHandlerLogicalCTEConsumer &);

	// process the start of an element
	virtual void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process the end of an element
	virtual void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

public:
	// ctor
	CParseHandlerLogicalCTEConsumer(CMemoryPool *mp,
									CParseHandlerManager *parse_handler_mgr,
									CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalCTEConsumer_H

// EOF
