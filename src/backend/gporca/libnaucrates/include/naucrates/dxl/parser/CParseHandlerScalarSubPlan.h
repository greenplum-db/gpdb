//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarSubPlan.h
//
//	@doc:
//
//		SAX parse handler class for parsing SubPlan.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerScalarSubPlan_H
#define GPDXL_CParseHandlerScalarSubPlan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLScalarSubPlan.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerScalarSubPlan
//
//	@doc:
//		Parse handler for parsing a scalar SubPlan
//
//---------------------------------------------------------------------------
class CParseHandlerScalarSubPlan : public CParseHandlerScalarOp
{
private:
	// first col type
	IMDId *m_mdid_first_col;

	// subplan type
	EdxlSubPlanType m_dxl_subplan_type;

	// map character sequence to subplan type
	static EdxlSubPlanType GetDXLSubplanType(const XMLCh *xml_subplan_type);

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
	CParseHandlerScalarSubPlan(const CParseHandlerScalarSubPlan &) = delete;

	// ctor
	CParseHandlerScalarSubPlan(CMemoryPool *mp,
							   CParseHandlerManager *parse_handler_mgr,
							   CParseHandlerBase *parse_handler_root);
};

}  // namespace gpdxl
#endif	// GPDXL_CParseHandlerScalarSubPlan_H

//EOF
