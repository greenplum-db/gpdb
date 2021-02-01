//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerSearchStrategy.h
//
//	@doc:
//		Parse handler for search strategy
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerSearchStrategy_H
#define GPDXL_CParseHandlerSearchStrategy_H

#include "gpos/base.h"

#include "gpopt/search/CSearchStage.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerSearchStrategy
//
//	@doc:
//		Parse handler for search strategy
//
//---------------------------------------------------------------------------
class CParseHandlerSearchStrategy : public CParseHandlerBase
{
private:
	// search stages
	CSearchStageArray *m_search_stage_array;

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
	// private ctor
	CParseHandlerSearchStrategy(const CParseHandlerSearchStrategy &) = delete;

	// ctor/dtor
	CParseHandlerSearchStrategy(CMemoryPool *mp,
								CParseHandlerManager *parse_handler_mgr,
								CParseHandlerBase *parse_handler_root);

	~CParseHandlerSearchStrategy() override;

	// returns the dxl representation of search stages
	CSearchStageArray *
	GetSearchStageArray()
	{
		return m_search_stage_array;
	}

	EDxlParseHandlerType
	GetParseHandlerType() const override
	{
		return EdxlphSearchStrategy;
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerSearchStrategy_H

// EOF
