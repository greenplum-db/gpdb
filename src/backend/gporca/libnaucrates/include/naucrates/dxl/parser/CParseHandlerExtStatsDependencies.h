//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStatsDependencies.h
//
//	@doc:
//		SAX parse handler class for parsing extended stats dependencies object
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerExtStatsDependencies_H
#define GPDXL_CParseHandlerExtStatsDependencies_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/md/CMDDependency.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

class CParseHandlerExtStatsDependencies : public CParseHandlerBase
{
private:
	// dependency list values
	CMDDependencyArray *m_dependencies;

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
	CParseHandlerExtStatsDependencies(
		const CParseHandlerExtStatsDependencies &) = delete;

	// ctor
	CParseHandlerExtStatsDependencies(CMemoryPool *mp,
									  CParseHandlerManager *parse_handler_mgr,
									  CParseHandlerBase *parse_handler_base);

	// dtor
	~CParseHandlerExtStatsDependencies() override;

	// returns the constructed bucket
	CMDDependencyArray *GetDependencies() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerExtStatsDependencies_H

// EOF
