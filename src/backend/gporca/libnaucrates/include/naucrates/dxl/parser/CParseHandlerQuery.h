//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerQuery.h
//
//	@doc:
//		Parse handler for converting a query (logical plan) from a DXL document
//		into a DXL tree.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerQuery_H
#define GPDXL_CParseHandlerQuery_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerQuery
//
//	@doc:
//		Parse handler for converting a query (logical plan) from a DXL document
//		into a DXL tree.
//---------------------------------------------------------------------------
class CParseHandlerQuery : public CParseHandlerBase
{
private:
	// the root of the parsed DXL tree constructed by the parse handler
	CDXLNode *m_dxl_node;

	// list of output columns (represented as scalar ident nodes)
	CDXLNodeArray *m_output_colums_dxl_array;

	// list of CTE priducers
	CDXLNodeArray *m_cte_producers;

	// private ctor
	CParseHandlerQuery(const CParseHandlerQuery &);

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
	CParseHandlerQuery(CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
					   CParseHandlerBase *parse_handler_root);

	virtual ~CParseHandlerQuery();

	// returns the root of constructed DXL plan
	CDXLNode *CreateDXLNode() const;

	// returns the dxl representation of the query output
	CDXLNodeArray *GetOutputColumnsDXLArray() const;

	// returns the CTEs
	CDXLNodeArray *GetCTEProducerDXLArray() const;

	EDxlParseHandlerType GetParseHandlerType() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerQuery_H

// EOF
