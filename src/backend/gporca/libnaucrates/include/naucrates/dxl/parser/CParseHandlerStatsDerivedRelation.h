//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerStatsDerivedRelation.h
//
//	@doc:
//		SAX parse handler class for parsing derived relation statistics
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerStatsDerivedRelation_H
#define GPDXL_CParseHandlerStatsDerivedRelation_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "naucrates/md/CDXLStatsDerivedRelation.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerStatsDerivedRelation
//
//	@doc:
//		Base parse handler class for derived relation statistics
//
//---------------------------------------------------------------------------
class CParseHandlerStatsDerivedRelation : public CParseHandlerBase
{
private:
	// number of rows in the relation
	CDouble m_rows;

	// flag to express that the statistics is on an empty input
	BOOL m_empty;

	// relation stats
	CDXLStatsDerivedRelation *m_dxl_stats_derived_relation;

	// private copy ctor
	CParseHandlerStatsDerivedRelation(
		const CParseHandlerStatsDerivedRelation &);

	// process the start of an element
	void StartElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname,		// element's qname
		const Attributes &attr					// element's attributes
	);

	// process the end of an element
	void EndElement(
		const XMLCh *const element_uri,			// URI of element's namespace
		const XMLCh *const element_local_name,	// local part of element's name
		const XMLCh *const element_qname		// element's qname
	);

public:
	// ctor
	CParseHandlerStatsDerivedRelation(CMemoryPool *mp,
									  CParseHandlerManager *parse_handler_mgr,
									  CParseHandlerBase *parse_handler_root);

	// dtor
	virtual ~CParseHandlerStatsDerivedRelation();

	// the derived relation stats
	CDXLStatsDerivedRelation *
	GetDxlStatsDrvdRelation() const
	{
		return m_dxl_stats_derived_relation;
	}
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerStatsDerivedRelation_H

// EOF
