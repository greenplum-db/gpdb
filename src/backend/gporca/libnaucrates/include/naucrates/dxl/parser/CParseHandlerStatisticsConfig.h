//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CParseHandlerStatisticsConfig.h
//
//	@doc:
//		SAX parse handler class for parsing statistics configuration
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerStatisticsConfig_H
#define GPDXL_CParseHandlerStatisticsConfig_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerBase.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerStatisticsConfig
//
//	@doc:
//		SAX parse handler class for parsing statistics configuration options
//
//---------------------------------------------------------------------------
class CParseHandlerStatisticsConfig : public CParseHandlerBase
{
private:
	// statistics configuration
	CStatisticsConfig *m_stats_conf;

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
	CParseHandlerStatisticsConfig(const CParseHandlerStatisticsConfig &) =
		delete;

	// ctor
	CParseHandlerStatisticsConfig(CMemoryPool *mp,
								  CParseHandlerManager *parse_handler_mgr,
								  CParseHandlerBase *parse_handler_root);

	// dtor
	~CParseHandlerStatisticsConfig() override;

	// type of the parse handler
	EDxlParseHandlerType GetParseHandlerType() const override;

	// enumerator configuration
	CStatisticsConfig *GetStatsConf() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerStatisticsConfig_H

// EOF
