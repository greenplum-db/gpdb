//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerBroadcastMotion.h
//
//	@doc:
//		SAX parse handler class for parsing gather motion operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerBroadcastMotion_H
#define GPDXL_CParseHandlerBroadcastMotion_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalBroadcastMotion.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerBroadcastMotion
//
//	@doc:
//		Parse handler for broadcast motion operators
//
//---------------------------------------------------------------------------
class CParseHandlerBroadcastMotion : public CParseHandlerPhysicalOp
{
private:
	// the broadcast motion operator
	CDXLPhysicalBroadcastMotion *m_dxl_op;

	// private copy ctor
	CParseHandlerBroadcastMotion(const CParseHandlerBroadcastMotion &);

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
	CParseHandlerBroadcastMotion(CMemoryPool *mp,
								 CParseHandlerManager *parse_handler_mgr,
								 CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerBroadcastMotion_H

// EOF
