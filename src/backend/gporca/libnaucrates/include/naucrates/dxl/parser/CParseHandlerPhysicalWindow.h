//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerPhysicalWindow.h
//
//	@doc:
//		SAX parse handler class for parsing a physical window operator
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerPhysicalWindow_H
#define GPDXL_CParseHandlerPhysicalWindow_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLWindowKey.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerPhysicalWindow
//
//	@doc:
//		Parse handler for parsing a physical window operator
//
//---------------------------------------------------------------------------
class CParseHandlerPhysicalWindow : public CParseHandlerPhysicalOp
{
private:
	// array of partition columns used by the window functions
	ULongPtrArray *m_part_by_colid_array;

	// private copy ctor
	CParseHandlerPhysicalWindow(const CParseHandlerPhysicalWindow &);

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
	CParseHandlerPhysicalWindow(CMemoryPool *mp,
								CParseHandlerManager *parse_handler_mgr,
								CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerPhysicalWindow_H

// EOF
