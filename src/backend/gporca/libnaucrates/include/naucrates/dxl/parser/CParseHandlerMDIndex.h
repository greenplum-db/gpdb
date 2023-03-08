//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerMDIndex.h
//
//	@doc:
//		SAX parse handler class for parsing an MD index
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerMDIndex_H
#define GPDXL_CParseHandlerMDIndex_H

#include "gpos/base.h"

#include "naucrates/dxl/parser/CParseHandlerMetadataObject.h"
#include "naucrates/md/CMDPartConstraintGPDB.h"
#include "naucrates/md/IMDIndex.h"

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@class:
//		CParseHandlerMDIndex
//
//	@doc:
//		Parse handler class for parsing an MD index
//
//---------------------------------------------------------------------------
class CParseHandlerMDIndex : public CParseHandlerMetadataObject
{
private:
	// mdid of the index
	IMDId *m_mdid;

	// name of the index
	CMDName *m_mdname;

	// is the index clustered
	BOOL m_clustered;

	// index type
	IMDIndex::EmdindexType m_index_type;

	// type id of index items
	// for instance, for bitmap indexes, this is the type id of the bitmap
	IMDId *m_mdid_item_type;

	// index keys
	ULongPtrArray *m_index_key_cols_array;

	// included columns
	ULongPtrArray *m_included_cols_array;

	// child index oids parse handler
	CParseHandlerBase *m_child_indexes_parse_handler;

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
	CParseHandlerMDIndex(const CParseHandlerMDIndex &) = delete;

	// ctor
	CParseHandlerMDIndex(CMemoryPool *mp,
						 CParseHandlerManager *parse_handler_mgr,
						 CParseHandlerBase *parse_handler_root);
};
}  // namespace gpdxl

#endif	// !GPDXL_CParseHandlerMDIndex_H

// EOF
