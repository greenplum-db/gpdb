//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerPlan.h
//
//	@doc:
//		SAX parse handler class for converting physical plans from a DXL document
//		into a DXL tree.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerPlan_H
#define GPDXL_CParseHandlerPlan_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

// fwd decl
class CDXLDirectDispatchInfo;

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerPlan
//
//	@doc:
//		Parse handler for converting physical plans from a DXL document
//		into a DXL tree.
//---------------------------------------------------------------------------
class CParseHandlerPlan : public CParseHandlerBase
{
private:
	// plan id
	ULLONG m_plan_id;

	// size of plan space
	ULLONG m_plan_space_size;

	// the root of the parsed DXL tree constructed by the parse handler
	CDXLNode *m_dxl_node;

	// direct dispatch info spec
	CDXLDirectDispatchInfo *m_direct_dispatch_info;

	// private ctor
	CParseHandlerPlan(const CParseHandlerPlan &);

	// process the end of an element
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
	CParseHandlerPlan(CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
					  CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerPlan();

	// returns the root of constructed DXL plan
	CDXLNode *CreateDXLNode();

	// return plan id
	ULLONG
	PlanId() const
	{
		return m_plan_id;
	}

	// return size of plan space
	ULLONG
	PlanSpaceSize() const
	{
		return m_plan_space_size;
	}

	EDxlParseHandlerType GetParseHandlerType() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerPlan_H

// EOF
