//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerAppend.h
//
//	@doc:
//		SAX parse handler class for parsing Append operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerAppend_H
#define GPDXL_CParseHandlerAppend_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalAppend.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerAppend
//
//	@doc:
//		Parse handler for Append operator
//
//---------------------------------------------------------------------------
class CParseHandlerAppend : public CParseHandlerPhysicalOp
{
private:
	CDXLPhysicalAppend *m_dxl_op;

	// private copy ctor
	CParseHandlerAppend(const CParseHandlerAppend &);

	// set up initial handlers
	void SetupInitialHandlers(const Attributes &attrs);

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attrs					// element's attributes
	);

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

public:
	// ctor/dtor
	CParseHandlerAppend(CMemoryPool *mp,
						CParseHandlerManager *parse_handler_mgr,
						CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl
#endif	// GPDXL_CParseHandlerAppend_H

// EOF
