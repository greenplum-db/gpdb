//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2024 Broadcom
//
//	@filename:
//		CParseHandlerScalarParam.h
//
//	@doc:
//		SAX parse handler class for parsing scalar param nodes.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarParam_H
#define GPDXL_CParseHandlerScalarParam_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarParam.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarParam
//
//	@doc:
//		Parse handler for parsing a scalar param operator
//
//---------------------------------------------------------------------------
class CParseHandlerScalarParam : public CParseHandlerScalarOp
{
private:
	// the scalar param
	CDXLScalarParam *m_dxl_op;

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
		) override;

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
		) override;

public:
	CParseHandlerScalarParam(const CParseHandlerScalarParam &) = delete;

	CParseHandlerScalarParam(CMemoryPool *mp,
							 CParseHandlerManager *parse_handler_mgr,
							 CParseHandlerBase *parse_handler_root);

	~CParseHandlerScalarParam() override;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarParam_H

// EOF
