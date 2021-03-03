//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerProperties.h
//
//	@doc:
//		SAX parse handler class for parsing properties of physical operators.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerProperties_H
#define GPDXL_CParseHandlerProperties_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalProperties.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "naucrates/md/CDXLStatsDerivedRelation.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerProperties
//
//	@doc:
//		Parse handler for parsing the properties of a physical operator
//
//---------------------------------------------------------------------------
class CParseHandlerProperties : public CParseHandlerBase
{
private:
	// physical properties container
	CDXLPhysicalProperties *m_dxl_properties;

	// statistics of the physical plan
	CDXLStatsDerivedRelation *m_dxl_stats_derived_relation;

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
	// private ctor
	CParseHandlerProperties(const CParseHandlerProperties &) = delete;

	// ctor
	CParseHandlerProperties(CMemoryPool *mp,
							CParseHandlerManager *parse_handler_mgr,
							CParseHandlerBase *parse_handler_root);

	// dtor
	~CParseHandlerProperties() override;

	// returns the constructed properties container
	CDXLPhysicalProperties *GetProperties() const;

	// return the derived relation statistics
	CDXLStatsDerivedRelation *
	GetDxlStatsDrvdRelation() const
	{
		return m_dxl_stats_derived_relation;
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerProperties_H

// EOF
