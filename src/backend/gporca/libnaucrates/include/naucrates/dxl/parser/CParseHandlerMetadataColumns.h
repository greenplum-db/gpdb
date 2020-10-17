//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerMetadataColumns.h
//
//	@doc:
//		SAX parse handler class for columns metadata
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMetadataColumns_H
#define GPDXL_CParseHandlerMetadataColumns_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "naucrates/md/CMDColumn.h"
#include "naucrates/md/CMDRelationGPDB.h"


namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMetadataColumns
//
//	@doc:
//		Parse handler for relation columns
//
//---------------------------------------------------------------------------
class CParseHandlerMetadataColumns : public CParseHandlerBase
{
private:
	// list of columns
	CMDColumnArray *m_md_col_array;

	// private copy ctor
	CParseHandlerMetadataColumns(const CParseHandlerMetadataColumns &);

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
	CParseHandlerMetadataColumns(CMemoryPool *mp,
								 CParseHandlerManager *parse_handler_mgr,
								 CParseHandlerBase *parse_handler_root);

	~CParseHandlerMetadataColumns();


	// returns the constructed columns list
	CMDColumnArray *GetMdColArray();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMetadataColumns_H

// EOF
