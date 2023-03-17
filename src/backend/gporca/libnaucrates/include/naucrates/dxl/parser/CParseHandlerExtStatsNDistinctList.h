//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStatsNDistinctList.h
//
//	@doc:
//		SAX parse handler class for parsing list of extended stats ndistinct
//		objects
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerExtStatsNDistinctList_H
#define GPDXL_CParseHandlerExtStatsNDistinctList_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/md/CMDNDistinct.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

class CParseHandlerExtStatsNDistinctList : public CParseHandlerBase
{
private:
	// dependency list values
	CMDNDistinctArray *m_ndistincts;

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
	CParseHandlerExtStatsNDistinctList(
		const CParseHandlerExtStatsNDistinctList &) = delete;

	// ctor
	CParseHandlerExtStatsNDistinctList(CMemoryPool *mp,
									   CParseHandlerManager *parse_handler_mgr,
									   CParseHandlerBase *parse_handler_base);

	// dtor
	~CParseHandlerExtStatsNDistinctList() override;

	// returns the constructed list
	CMDNDistinctArray *GetNDistinctList() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerExtStatsNDistinctList_H

// EOF
