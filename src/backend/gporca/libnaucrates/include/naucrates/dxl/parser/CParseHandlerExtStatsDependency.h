//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CParseHandlerExtStatsDependency.h
//
//	@doc:
//		SAX parse handler class for parsing extended stats dependency object
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerExtStatsDependency_H
#define GPDXL_CParseHandlerExtStatsDependency_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/md/CMDDependency.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

class CParseHandlerExtStatsDependency : public CParseHandlerBase
{
private:
	// degree
	CDouble m_degree;

	// from attnos values
	IntPtrArray *m_from_attnos;

	INT m_to_attno;

	// dependency object
	CMDDependency *m_dependency;

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
	CParseHandlerExtStatsDependency(const CParseHandlerExtStatsDependency &) =
		delete;

	// ctor
	CParseHandlerExtStatsDependency(CMemoryPool *mp,
									CParseHandlerManager *parse_handler_mgr,
									CParseHandlerBase *parse_handler_base);

	// dtor
	~CParseHandlerExtStatsDependency() override;

	// returns the constructed bucket
	CMDDependency *GetDependencyAt() const;
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerExtStatsDependency_H

// EOF
