//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarSubPlanParamList.h
//
//	@doc:
//
//		SAX parse handler class for parsing SubPlan ParamList
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarSubPlanParamList_H
#define GPDXL_CParseHandlerScalarSubPlanParamList_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarSubPlanParamList
//
//	@doc:
//		Parse handler for parsing a scalar SubPlan ParamList
//
//---------------------------------------------------------------------------
class CParseHandlerScalarSubPlanParamList : public CParseHandlerScalarOp
{
private:
	BOOL m_has_param_list;

	// array of outer column references
	CDXLColRefArray *m_dxl_colref_array;

	// private copy ctor
	CParseHandlerScalarSubPlanParamList(
		const CParseHandlerScalarSubPlanParamList &);

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
	CParseHandlerScalarSubPlanParamList(CMemoryPool *mp,
										CParseHandlerManager *parse_handler_mgr,
										CParseHandlerBase *parse_handler_root);

	// dtor
	virtual ~CParseHandlerScalarSubPlanParamList();

	// return param column references
	CDXLColRefArray *
	GetDXLColRefArray() const
	{
		return m_dxl_colref_array;
	}
};

}  // namespace gpdxl
#endif	// GPDXL_CParseHandlerScalarSubPlanParamList_H

//EOF
