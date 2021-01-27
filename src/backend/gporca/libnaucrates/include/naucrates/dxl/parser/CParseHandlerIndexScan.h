//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerIndexScan.h
//
//	@doc:
//		SAX parse handler class for parsing index scan operator nodes
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerIndexScan_H
#define GPDXL_CParseHandlerIndexScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysicalIndexScan.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"
#include "naucrates/dxl/xml/dxltokens.h"

namespace gpdxl
{
using namespace gpos;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerIndexScan
//
//	@doc:
//		Parse handler for index scan operator nodes
//
//---------------------------------------------------------------------------
class CParseHandlerIndexScan : public CParseHandlerPhysicalOp
{
private:
	// index scan direction
	EdxlIndexScanDirection m_index_scan_dir;

	// private copy ctor
	CParseHandlerIndexScan(const CParseHandlerIndexScan &);

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

protected:
	// common StartElement functionality for IndexScan and IndexOnlyScan
	void StartElementHelper(const XMLCh *const element_local_name,
							const Attributes &attrs, Edxltoken token_type);

	// common EndElement functionality for IndexScan and IndexOnlyScan
	void EndElementHelper(const XMLCh *const element_local_name,
						  Edxltoken token_type, ULONG part_idx_id = 0,
						  ULONG part_idx_id_printable = 0);

public:
	// ctor
	CParseHandlerIndexScan(CMemoryPool *mp,
						   CParseHandlerManager *parse_handler_mgr,
						   CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerIndexScan_H

// EOF
