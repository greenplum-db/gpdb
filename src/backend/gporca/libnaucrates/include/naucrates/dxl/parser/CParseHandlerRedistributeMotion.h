//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerRedistributeMotion.h
//
//	@doc:
//		SAX parse handler class for parsing redistribute motion operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerRedistributeMotion_H
#define GPDXL_CParseHandlerRedistributeMotion_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

#include "naucrates/dxl/operators/CDXLPhysicalRedistributeMotion.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerRedistributeMotion
//
//	@doc:
//		Parse handler for redistribute motion operators
//
//---------------------------------------------------------------------------
class CParseHandlerRedistributeMotion : public CParseHandlerPhysicalOp
{
private:
	// the redistribute motion operator
	CDXLPhysicalRedistributeMotion *m_dxl_op;

	// private copy ctor
	CParseHandlerRedistributeMotion(const CParseHandlerRedistributeMotion &);

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
	CParseHandlerRedistributeMotion(CMemoryPool *mp,
									CParseHandlerManager *parse_handler_mgr,
									CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerRedistributeMotion_H

// EOF
