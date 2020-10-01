//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Software, Inc.
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

	// private copy ctor
	CParseHandlerNLJIndexParam(const CParseHandlerNLJIndexParam &);

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
	CParseHandlerNLJIndexParam(CMemoryPool *mp,
							   CParseHandlerManager *parse_handler_manager,
							   CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerNLJIndexParam();

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
