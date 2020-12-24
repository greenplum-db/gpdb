//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerNLJIndexParam.h
//
//	@doc:
//
//		SAX parse handler class for parsing a single NLJ index param
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerNLJIndexParam_H
#define GPDXL_CParseHandlerNLJIndexParam_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerNLJIndexParam
//
//	@doc:
//		Parse handler for parsing a Param of NLJ
//
//---------------------------------------------------------------------------
class CParseHandlerNLJIndexParam : public CParseHandlerBase
{
private:
	// column reference
	CDXLColRef *m_nest_param_colref_dxl;

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
	CParseHandlerNLJIndexParam(const CParseHandlerNLJIndexParam &) = delete;

	// ctor/dtor
	CParseHandlerNLJIndexParam(CMemoryPool *mp,
							   CParseHandlerManager *parse_handler_manager,
							   CParseHandlerBase *parse_handler_root);

	~CParseHandlerNLJIndexParam() override;

	// return column reference
	CDXLColRef *
	GetNestParamColRefDxl(void) const
	{
		return m_nest_param_colref_dxl;
	}
};

}  // namespace gpdxl
#endif	// GPDXL_CParseHandlerNLJIndexParam_H

//EOF
