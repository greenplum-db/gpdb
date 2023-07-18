//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStats.h
//
//	@doc:
//		SAX parse handler class for parsing extended stats objects
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerExtStats_H
#define GPDXL_CParseHandlerExtStats_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

class CParseHandlerExtStats : public CParseHandlerMetadataObject
{
private:
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

	IMDId *m_mdid;

	CMDName *m_mdname;

public:
	CParseHandlerExtStats(const CParseHandlerExtStats &) = delete;

	// ctor
	CParseHandlerExtStats(CMemoryPool *mp,
						  CParseHandlerManager *parse_handler_mgr,
						  CParseHandlerBase *parse_handler_root);

	~CParseHandlerExtStats() override;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerExtStats_H

// EOF
