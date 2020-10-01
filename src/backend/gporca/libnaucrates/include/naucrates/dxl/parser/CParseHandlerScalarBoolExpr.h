//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerScalarBoolExpr.h
//
//	@doc:
//
//		SAX parse handler class for parsing scalar BoolExpr.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarBoolExpr_H
#define GPDXL_CParseHandlerScalarBoolExpr_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLScalarBoolExpr.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarBoolExpr
//
//	@doc:
//		Parse handler for parsing a scalar op expression
//
//---------------------------------------------------------------------------
class CParseHandlerScalarBoolExpr : public CParseHandlerScalarOp
{
private:
	EdxlBoolExprType m_dxl_bool_type;

	// private copy ctor
	CParseHandlerScalarBoolExpr(const CParseHandlerScalarBoolExpr &);

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

	// parse the bool type from the Xerces xml string
	static EdxlBoolExprType GetDxlBoolTypeStr(const XMLCh *xmlszBoolType);

public:
	// ctor
	CParseHandlerScalarBoolExpr(CMemoryPool *mp,
								CParseHandlerManager *parse_handler_mgr,
								CParseHandlerBase *parse_handler_root);
};

}  // namespace gpdxl
#endif	// GPDXL_CParseHandlerScalarBoolExpr_H

//EOF
