//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerRelationExtendedStats.h
//
//	@doc:
//		SAX parse handler class for parsing relation extended stats metadata
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerRelationExtendedStatss_H
#define GPDXL_CParseHandlerRelationExtendedStatss_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/md/CMDExtStatsInfo.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

class CParseHandlerRelationExtendedStats : public CParseHandlerMetadataObject
{
private:
	// dependency list values
	CMDExtStatsInfoArray *m_extinfos;

	IMDId *m_mdid;
	CMDName *m_mdname;

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
	CParseHandlerRelationExtendedStats(
		const CParseHandlerRelationExtendedStats &) = delete;

	// ctor
	CParseHandlerRelationExtendedStats(CMemoryPool *mp,
									   CParseHandlerManager *parse_handler_mgr,
									   CParseHandlerBase *parse_handler_base);

	// dtor
	~CParseHandlerRelationExtendedStats() override = default;

	// returns the constructed bucket
	CMDExtStatsInfoArray *GetInfos() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerRelationExtendedStatss_H

// EOF
