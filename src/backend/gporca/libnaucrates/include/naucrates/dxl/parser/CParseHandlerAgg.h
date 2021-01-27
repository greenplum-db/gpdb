//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerAgg.h
//
//	@doc:
//		SAX parse handler class for parsing aggregate operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerGroupBy_H
#define GPDXL_CParseHandlerGroupBy_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalAgg.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerAgg
//
//	@doc:
//		Parse handler for aggregate operators
//
//---------------------------------------------------------------------------
class CParseHandlerAgg : public CParseHandlerPhysicalOp
{
private:
	// the aggregate operator
	CDXLPhysicalAgg *m_dxl_op;

	// private copy ctor
	CParseHandlerAgg(const CParseHandlerAgg &);

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
	CParseHandlerAgg(CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
					 CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerGroupBy_H

// EOF
