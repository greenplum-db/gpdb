//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerMDIndex.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing an MD index
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerMDIndex.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerMetadataIdList.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/md/CMDIndexGPDB.h"

using namespace gpdxl;
using namespace gpmd;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDIndex::CParseHandlerMDIndex
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerMDIndex::CParseHandlerMDIndex(
	CMemoryPool *mp, CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerMetadataObject(mp, parse_handler_mgr, parse_handler_root),
      m_mdid(nullptr),
      m_mdname(nullptr),
      m_clustered(false),
      m_index_type(IMDIndex::EmdindSentinel),
      m_mdid_item_type(nullptr),
      m_index_key_cols_array(nullptr),
      m_included_cols_array(nullptr),
      m_index_key_cols_sort_order(nullptr),
      m_index_key_cols_nulls_order(nullptr),
      m_child_indexes_parse_handler(nullptr)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDIndex::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDIndex::StartElement(const XMLCh *const element_uri,
								   const XMLCh *const element_local_name,
								   const XMLCh *const element_qname,
								   const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPartitions),
								 element_local_name))
	{
		// parse handler for child indexes of a partitioned index
		m_child_indexes_parse_handler = CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataIdList),
			m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(
			m_child_indexes_parse_handler);
		this->Append(m_child_indexes_parse_handler);
		m_child_indexes_parse_handler->startElement(
			element_uri, element_local_name, element_qname, attrs);

		return;
	}

	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenIndex),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	// new index object
	GPOS_ASSERT(nullptr == m_mdid);

	// parse mdid
	m_mdid = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs, EdxltokenMdid,
		EdxltokenIndex);

	// parse index name
	const XMLCh *parsed_column_name = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenName, EdxltokenIndex);
	CWStringDynamic *column_name = CDXLUtils::CreateDynamicStringFromXMLChArray(
		m_parse_handler_mgr->GetDXLMemoryManager(), parsed_column_name);

	// create a copy of the string in the CMDName constructor
	m_mdname = GPOS_NEW(m_mp) CMDName(m_mp, column_name);
	GPOS_DELETE(column_name);

	// parse index clustering, key columns and included columns information
	m_clustered = CDXLOperatorFactory::ExtractConvertAttrValueToBool(
		m_parse_handler_mgr->GetDXLMemoryManager(), attrs,
		EdxltokenIndexClustered, EdxltokenIndex);

	m_index_type = CDXLOperatorFactory::ParseIndexType(attrs);
	const XMLCh *xmlszItemType =
		attrs.getValue(CDXLTokens::XmlstrToken(EdxltokenIndexItemType));
	if (nullptr != xmlszItemType)
	{
		m_mdid_item_type = CDXLOperatorFactory::MakeMdIdFromStr(
			m_parse_handler_mgr->GetDXLMemoryManager(), xmlszItemType,
			EdxltokenIndexItemType, EdxltokenIndex);
	}

	const XMLCh *xmlszIndexKeys = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenIndexKeyCols, EdxltokenIndex);
	m_index_key_cols_array = CDXLOperatorFactory::ExtractIntsToUlongArray(
		m_parse_handler_mgr->GetDXLMemoryManager(), xmlszIndexKeys,
		EdxltokenIndexKeyCols, EdxltokenIndex);



	const XMLCh *xmlszIndexIncludedCols = CDXLOperatorFactory::ExtractAttrValue(
		attrs, EdxltokenIndexIncludedCols, EdxltokenIndex);
	m_included_cols_array = CDXLOperatorFactory::ExtractIntsToUlongArray(
		m_parse_handler_mgr->GetDXLMemoryManager(), xmlszIndexIncludedCols,
		EdxltokenIndexIncludedCols, EdxltokenIndex);

  if (m_index_type == gpmd::IMDIndex::EmdindBtree) {
      const XMLCh *xmlszIndexKeyColsSortOrder = CDXLOperatorFactory::ExtractAttrValue(
              attrs, EdxltokenIndexKeyColsSortOrder, EdxltokenIndex, true);
      m_index_key_cols_sort_order = CDXLOperatorFactory::ExtractSortAndNullsToULongArray(
              m_parse_handler_mgr->GetDXLMemoryManager(), xmlszIndexKeyColsSortOrder,
              true, m_index_key_cols_array->Size());

      const XMLCh *xmlszIndexKeyColsNullsOrder = CDXLOperatorFactory::ExtractAttrValue(
              attrs, EdxltokenIndexKeyColsNullsOrder, EdxltokenIndex, true);
      m_index_key_cols_nulls_order = CDXLOperatorFactory::ExtractSortAndNullsToULongArray(
              m_parse_handler_mgr->GetDXLMemoryManager(), xmlszIndexKeyColsNullsOrder,
		  false, m_index_key_cols_array->Size());

  }
  else {
      CMemoryPool *mp = m_parse_handler_mgr->GetDXLMemoryManager()->Pmp();
      m_index_key_cols_sort_order = GPOS_NEW(mp) ULongPtrArray (mp);
      m_index_key_cols_nulls_order = GPOS_NEW(mp) ULongPtrArray (mp);
  }
	// parse handler for operator class list
	CParseHandlerBase *opfamilies_list_parse_handler =
		CParseHandlerFactory::GetParseHandler(
			m_mp, CDXLTokens::XmlstrToken(EdxltokenMetadataIdList),
			m_parse_handler_mgr, this);
	m_parse_handler_mgr->ActivateParseHandler(opfamilies_list_parse_handler);

	this->Append(opfamilies_list_parse_handler);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerMDIndex::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerMDIndex::EndElement(const XMLCh *const,  // element_uri,
								 const XMLCh *const element_local_name,
								 const XMLCh *const	 // element_qname
)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenIndex),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,
				   str->GetBuffer());
	}

	CParseHandlerMetadataIdList *pphMdidOpfamilies =
		dynamic_cast<CParseHandlerMetadataIdList *>((*this)[0]);
	IMdIdArray *mdid_opfamilies_array = pphMdidOpfamilies->GetMdIdArray();
	mdid_opfamilies_array->AddRef();

	BOOL is_partitioned = false;
	IMdIdArray *child_indexes = nullptr;
	if (nullptr != m_child_indexes_parse_handler)
	{
		is_partitioned = true;
		child_indexes = dynamic_cast<CParseHandlerMetadataIdList *>(
							m_child_indexes_parse_handler)
							->GetMdIdArray();
		child_indexes->AddRef();
	}

	m_imd_obj = GPOS_NEW(m_mp) CMDIndexGPDB(
            m_mp, m_mdid, m_mdname, m_clustered, is_partitioned, m_index_type,
            m_mdid_item_type, m_index_key_cols_array, m_included_cols_array,
            mdid_opfamilies_array, child_indexes, m_index_key_cols_sort_order, m_index_key_cols_nulls_order);

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
