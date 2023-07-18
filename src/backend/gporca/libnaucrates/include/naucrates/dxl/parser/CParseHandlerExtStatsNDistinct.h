//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStatsNDistinct.h
//
//	@doc:
//		SAX parse handler class for parsing extended stats ndistinct object
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerExtStatsNDistinct_H
#define GPDXL_CParseHandlerExtStatsNDistinct_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/md/CMDNDistinct.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

class CParseHandlerExtStatsNDistinct : public CParseHandlerBase
{
private:
	// ndistinct
	CDouble m_ndistinct;

	// attnos values
	CBitSet *m_attnos;

	// ndistinct object
	CMDNDistinct *m_ndistinct_md;

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
	CParseHandlerExtStatsNDistinct(const CParseHandlerExtStatsNDistinct &) =
		delete;

	// ctor
	CParseHandlerExtStatsNDistinct(CMemoryPool *mp,
								   CParseHandlerManager *parse_handler_mgr,
								   CParseHandlerBase *parse_handler_base);

	// dtor
	~CParseHandlerExtStatsNDistinct() override;

	// returns the constructed ndistinct
	CMDNDistinct *GetNDistinctAt() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerExtStatsNDistinct_H

// EOF
