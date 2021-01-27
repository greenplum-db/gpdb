//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerMetadataColumn.h
//
//	@doc:
//		SAX parse handler class for column metadata
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMetadataColumn_H
#define GPDXL_CParseHandlerMetadataColumn_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "naucrates/md/CMDColumn.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMetadataColumn
//
//	@doc:
//		Parse handler for column metadata
//
//---------------------------------------------------------------------------
class CParseHandlerMetadataColumn : public CParseHandlerBase
{
private:
	// the metadata column
	CMDColumn *m_mdcol;

	// column name
	CMDName *m_mdname;

	// attribute number
	INT m_attno;

	// attribute type oid
	IMDId *m_mdid_type;

	INT m_type_modifier;

	// are nulls allowed for this column
	BOOL m_is_nullable;

	// is column dropped
	BOOL m_is_dropped;

	// default value expression if one exists
	CDXLNode *m_dxl_default_val;

	// width of the column
	ULONG m_width;

	// private copy ctor
	CParseHandlerMetadataColumn(const CParseHandlerMetadataColumn &);

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
	CParseHandlerMetadataColumn(CMemoryPool *mp,
								CParseHandlerManager *parse_handler_mgr,
								CParseHandlerBase *parse_handler_root);

	~CParseHandlerMetadataColumn();

	CMDColumn *GetMdCol();
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMetadataColumn_H

// EOF
