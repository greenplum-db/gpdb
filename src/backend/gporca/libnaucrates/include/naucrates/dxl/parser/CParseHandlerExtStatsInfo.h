//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStatsInfo.h
//
//	@doc:
//		SAX parse handler class for parsing extended stats metadata object
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerExtStatsInfo_H
#define GPDXL_CParseHandlerExtStatsInfo_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/md/CMDExtStatsInfo.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

class CParseHandlerExtStatsInfo : public CParseHandlerBase
{
private:
	// extstat infos list
	CMDExtStatsInfo *m_extinfo;

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

	CMDExtStatsInfo::Estattype ParseStatKind(const Attributes &attrs) const;

public:
	CParseHandlerExtStatsInfo(const CParseHandlerExtStatsInfo &) = delete;

	// ctor
	CParseHandlerExtStatsInfo(CMemoryPool *mp,
							  CParseHandlerManager *parse_handler_mgr,
							  CParseHandlerBase *parse_handler_base);

	// dtor
	~CParseHandlerExtStatsInfo() override;

	// returns the constructed bucket
	CMDExtStatsInfo *GetInfo() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerExtStatsInfo_H

// EOF
