//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerRoutedMotion.h
//
//	@doc:
//		SAX parse handler class for parsing routed motion operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerRoutedMotion_H
#define GPDXL_CParseHandlerRoutedMotion_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalRoutedDistributeMotion.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerRoutedMotion
//
//	@doc:
//		Parse handler for routed motion operators
//
//---------------------------------------------------------------------------
class CParseHandlerRoutedMotion : public CParseHandlerPhysicalOp
{
private:
	// motion operator
	CDXLPhysicalRoutedDistributeMotion *m_dxl_op;

	// private copy ctor
	CParseHandlerRoutedMotion(const CParseHandlerRoutedMotion &);

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
	CParseHandlerRoutedMotion(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerRoutedMotion_H

// EOF
