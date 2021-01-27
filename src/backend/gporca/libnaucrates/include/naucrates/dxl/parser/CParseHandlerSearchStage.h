//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp..
//
//	@filename:
//		CParseHandlerSearchStage.h
//
//	@doc:
//		Parse handler for search stage.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerSearchStage_H
#define GPDXL_CParseHandlerSearchStage_H

#include "gpos/base.h"

#include "gpopt/cost/CCost.h"
#include "gpopt/xforms/CXform.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerSearchStage
//
//	@doc:
//		Parse handler for search stage
//
//---------------------------------------------------------------------------
class CParseHandlerSearchStage : public CParseHandlerBase
{
private:
	// set of search stage xforms
	CXformSet *m_xforms;

	// cost threshold
	CCost m_cost_threshold;

	// time threshold in milliseconds
	ULONG m_time_threshold;

	// private ctor
	CParseHandlerSearchStage(const CParseHandlerSearchStage &);

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
	CParseHandlerSearchStage(CMemoryPool *mp,
							 CParseHandlerManager *parse_handler_mgr,
							 CParseHandlerBase *parse_handler_root);

	// dtor
	virtual ~CParseHandlerSearchStage();

	// returns stage xforms
	CXformSet *
	GetXformSet() const
	{
		return m_xforms;
	}

	// returns stage cost threshold
	CCost
	CostThreshold() const
	{
		return m_cost_threshold;
	}

	// returns time threshold
	ULONG
	TimeThreshold() const
	{
		return m_time_threshold;
	}

	EDxlParseHandlerType
	GetParseHandlerType() const
	{
		return EdxlphSearchStrategy;
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerSearchStage_H

// EOF
