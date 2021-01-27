//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarArrayComp.h
//
//	@doc:
//
//		SAX parse handler class for parsing CDXLScalarArrayComp.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarArrayComp_H
#define GPDXL_CParseHandlerScalarArrayComp_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarArrayComp.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarArrayComp
//
//	@doc:
//		Parse handler for parsing a scalar array comparator
//
//---------------------------------------------------------------------------
class CParseHandlerScalarArrayComp : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarArrayComp(const CParseHandlerScalarArrayComp &);

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
	CParseHandlerScalarArrayComp(CMemoryPool *mp,
								 CParseHandlerManager *parse_handler_mgr,
								 CParseHandlerBase *parse_handler_root);
};

}  // namespace gpdxl
#endif	// GPDXL_CParseHandlerScalarArrayComp_H

//EOF
