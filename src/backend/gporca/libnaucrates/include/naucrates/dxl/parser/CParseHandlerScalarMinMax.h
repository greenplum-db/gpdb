//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerScalarMinMax.h
//
//	@doc:
//
//		SAX parse handler class for parsing MinMax operator
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerScalarMinMax_H
#define GPDXL_CParseHandlerScalarMinMax_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/operators/CDXLScalarMinMax.h"


namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarMinMax
//
//	@doc:
//		Parse handler for parsing a MinMax operator
//
//---------------------------------------------------------------------------
class CParseHandlerScalarMinMax : public CParseHandlerScalarOp
{
private:
	// return type
	IMDId *m_mdid_type;

	// min/max type
	CDXLScalarMinMax::EdxlMinMaxType m_min_max_type;

	// process the start of an element
	void StartElement(const XMLCh *const element_uri,
					  const XMLCh *const element_local_name,
					  const XMLCh *const element_qname,
					  const Attributes &attr) override;

	// process the end of an element
	void EndElement(const XMLCh *const element_uri,
					const XMLCh *const element_local_name,
					const XMLCh *const element_qname) override;

	// parse the min/max type from the attribute value
	static CDXLScalarMinMax::EdxlMinMaxType GetMinMaxType(
		const XMLCh *element_local_name);

public:
	CParseHandlerScalarMinMax(const CParseHandlerScalarMinMax &) = delete;

	// ctor
	CParseHandlerScalarMinMax(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerScalarMinMax_H

//EOF
