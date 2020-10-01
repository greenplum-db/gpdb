//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerMaterialize.h
//
//	@doc:
//		SAX parse handler class for parsing materialize operator nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMaterialize_H
#define GPDXL_CParseHandlerMaterialize_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"

#include "naucrates/dxl/operators/CDXLPhysicalMaterialize.h"


namespace gpdxl
{
using namespace gpos;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMaterialize
//
//	@doc:
//		Parse handler for parsing a materialize operator
//
//---------------------------------------------------------------------------
class CParseHandlerMaterialize : public CParseHandlerPhysicalOp
{
private:
	// the materialize operator
	CDXLPhysicalMaterialize *m_dxl_op;

	// private copy ctor
	CParseHandlerMaterialize(const CParseHandlerMaterialize &);

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
	CParseHandlerMaterialize(CMemoryPool *mp,
							 CParseHandlerManager *parse_handler_mgr,
							 CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMaterialize_H

// EOF
