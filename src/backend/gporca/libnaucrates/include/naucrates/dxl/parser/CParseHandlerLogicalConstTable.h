//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CParseHandlerLogicalConstTable.h
//
//	@doc:
//		SAX parse handler class for parsing logical const tables.
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerLogicalConstTable_H
#define GPDXL_CParseHandlerLogicalConstTable_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLLogicalGet.h"
#include "naucrates/dxl/parser/CParseHandlerLogicalOp.h"


namespace gpdxl
{
using namespace gpos;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerLogicalConstTable
//
//	@doc:
//		Parse handler for parsing a logical const table operator
//
//---------------------------------------------------------------------------
class CParseHandlerLogicalConstTable : public CParseHandlerLogicalOp
{
private:
	// array of datum arrays
	CDXLDatum2dArray *m_const_tuples_datum_array;

	// array of datums
	CDXLDatumArray *m_dxl_datum_array;

	// private copy ctor
	CParseHandlerLogicalConstTable(const CParseHandlerLogicalConstTable &);

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
	// ctor/dtor
	CParseHandlerLogicalConstTable(CMemoryPool *mp,
								   CParseHandlerManager *parse_handler_mgr,
								   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerLogicalConstTable_H

// EOF
