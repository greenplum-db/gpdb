//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 VMware, Inc. or its affiliates.
//
//	@filename:
//		CParseHandlerCostModel.h
//
//	@doc:
//		SAX parse handler class for parsing cost model config
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerCostModel_H
#define GPDXL_CParseHandlerCostModel_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "gpopt/cost/ICostModel.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

class CParseHandlerCostParams;
//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerCostModel
//
//	@doc:
//		SAX parse handler class for parsing cost model config options
//
//---------------------------------------------------------------------------
class CParseHandlerCostModel : public CParseHandlerBase
{
private:
	ICostModel::ECostModelType m_cost_model_type;
	ULONG m_num_of_segments;
	// cost model
	ICostModel *m_cost_model;

	CParseHandlerCostParams *m_parse_handler_cost_params;

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
	CParseHandlerCostModel(const CParseHandlerCostModel &) = delete;

	// ctor/dtor
	CParseHandlerCostModel(CMemoryPool *mp,
						   CParseHandlerManager *parse_handler_mgr,
						   CParseHandlerBase *parse_handler_root);

	~CParseHandlerCostModel() override;

	// cost model
	ICostModel *GetCostModel() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerCostModel_H

// EOF
