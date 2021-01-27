//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarConstValue.h
//
//	@doc:
//		SAX parse handler class for parsing scalar ConstVal node.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarConst_H
#define GPDXL_CParseHandlerScalarConst_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarConstValue.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarConstValue
//
//	@doc:
//		Parse handler for parsing a scalar ConstValue
//
//---------------------------------------------------------------------------
class CParseHandlerScalarConstValue : public CParseHandlerScalarOp
{
private:
	// private copy ctor
	CParseHandlerScalarConstValue(const CParseHandlerScalarConstValue &);

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
	CParseHandlerScalarConstValue(CMemoryPool *mp,
								  CParseHandlerManager *parse_handler_mgr,
								  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// GPDXL_CParseHandlerScalarConst_H

// EOF
