//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerPhysicalCTEConsumer.h
//
//	@doc:
//		Parse handler for parsing a physical CTE Consumer operator
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerPhysicalCTEConsumer_H
#define GPDXL_CParseHandlerPhysicalCTEConsumer_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerPhysicalCTEConsumer
//
//	@doc:
//		Parse handler for parsing a physical CTE Consumer operator
//
//---------------------------------------------------------------------------
class CParseHandlerPhysicalCTEConsumer : public CParseHandlerPhysicalOp
{
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
	CParseHandlerPhysicalCTEConsumer(const CParseHandlerPhysicalCTEConsumer &) =
		delete;

	// ctor
	CParseHandlerPhysicalCTEConsumer(CMemoryPool *mp,
									 CParseHandlerManager *parse_handler_mgr,
									 CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerPhysicalCTEConsumer_H

// EOF
