//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerCTEList.h
//
//	@doc:
//		Parse handler for parsing a CTE list
//
//---------------------------------------------------------------------------
#ifndef GPDXL_CParseHandlerCTEList_H
#define GPDXL_CParseHandlerCTEList_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerCTEList
//
//	@doc:
//		Parse handler for parsing a CTE list
//
//---------------------------------------------------------------------------
class CParseHandlerCTEList : public CParseHandlerBase
{
private:
	// CTE list
	CDXLNodeArray *m_dxl_array;

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
	CParseHandlerCTEList(const CParseHandlerCTEList &) = delete;

	// ctor
	CParseHandlerCTEList(CMemoryPool *mp,
						 CParseHandlerManager *parse_handler_mgr,
						 CParseHandlerBase *parse_handler_root);

	// dtor
	~CParseHandlerCTEList() override;

	// CTE list
	CDXLNodeArray *
	GetDxlCteArray() const
	{
		return m_dxl_array;
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerCTEList_H

// EOF
