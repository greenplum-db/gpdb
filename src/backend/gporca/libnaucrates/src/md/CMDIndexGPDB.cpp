//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDIndexGPDB.cpp
//
//	@doc:
//		Implementation of the class for representing metadata indexes
//---------------------------------------------------------------------------


#include "naucrates/md/CMDIndexGPDB.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/exception.h"
#include "naucrates/md/CMDRelationGPDB.h"
#include "naucrates/md/IMDScalarOp.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::CMDIndexGPDB
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CMDIndexGPDB::CMDIndexGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
						   BOOL is_clustered, BOOL is_partitioned,
						   EmdindexType index_type, IMDId *mdid_item_type,
						   ULongPtrArray *index_key_cols_array,
						   ULongPtrArray *included_cols_array,
						   IMdIdArray *mdid_opfamilies_array,
						   IMdIdArray *child_index_oids,
						   ULongPtrArray *index_key_cols_sort_order,
						   ULongPtrArray *index_key_cols_null_order)

	: m_mp(mp),
	  m_mdid(mdid),
	  m_mdname(mdname),
	  m_clustered(is_clustered),
	  m_partitioned(is_partitioned),
	  m_index_type(index_type),
	  m_mdid_item_type(mdid_item_type),
	  m_index_key_cols_array(index_key_cols_array),
	  m_included_cols_array(included_cols_array),
	  m_mdid_opfamilies_array(mdid_opfamilies_array),
	  m_child_index_oids(child_index_oids),
	  m_index_key_cols_sort_order(index_key_cols_sort_order),
	  m_index_key_cols_nulls_order(index_key_cols_null_order)
{
	GPOS_ASSERT(mdid->IsValid());
	GPOS_ASSERT(IMDIndex::EmdindSentinel > index_type);
	GPOS_ASSERT(nullptr != index_key_cols_array);
	GPOS_ASSERT(0 < index_key_cols_array->Size());
	GPOS_ASSERT(nullptr != included_cols_array);
	GPOS_ASSERT_IMP(nullptr != mdid_item_type,
					IMDIndex::EmdindBitmap == index_type ||
						IMDIndex::EmdindBtree == index_type ||
						IMDIndex::EmdindGist == index_type ||
						IMDIndex::EmdindGin == index_type ||
						IMDIndex::EmdindBrin == index_type ||
						IMDIndex::EmdindHash == index_type);
	GPOS_ASSERT_IMP(IMDIndex::EmdindBitmap == index_type,
					nullptr != mdid_item_type && mdid_item_type->IsValid());
	GPOS_ASSERT(nullptr != mdid_opfamilies_array);
	GPOS_ASSERT(nullptr != index_key_cols_sort_order);
	GPOS_ASSERT(nullptr != index_key_cols_null_order);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::~CMDIndexGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDIndexGPDB::~CMDIndexGPDB()
{
	GPOS_DELETE(m_mdname);
	if (nullptr != m_dxl_str)
	{
		GPOS_DELETE(m_dxl_str);
	}
	m_mdid->Release();
	CRefCount::SafeRelease(m_mdid_item_type);
	m_index_key_cols_array->Release();
	m_included_cols_array->Release();
	m_mdid_opfamilies_array->Release();
	CRefCount::SafeRelease(m_child_index_oids);
	m_index_key_cols_sort_order->Release();
	m_index_key_cols_nulls_order->Release();
}

const CWStringDynamic *
CMDIndexGPDB::GetStrRepr()
{
	if (nullptr == m_dxl_str)
	{
		m_dxl_str = CDXLUtils::SerializeMDObj(
			m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);
	}
	return m_dxl_str;
}
//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::MDId
//
//	@doc:
//		Returns the metadata id of this index
//
//---------------------------------------------------------------------------
IMDId *
CMDIndexGPDB::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::Mdname
//
//	@doc:
//		Returns the name of this index
//
//---------------------------------------------------------------------------
CMDName
CMDIndexGPDB::Mdname() const
{
	return *m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::IsClustered
//
//	@doc:
//		Is the index clustered
//
//---------------------------------------------------------------------------
BOOL
CMDIndexGPDB::IsClustered() const
{
	return m_clustered;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::IsPartitioned
//
//	@doc:
//		Is the index partitioned
//
//---------------------------------------------------------------------------
BOOL
CMDIndexGPDB::IsPartitioned() const
{
	return m_partitioned;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::IndexType
//
//	@doc:
//		Index type
//
//---------------------------------------------------------------------------
IMDIndex::EmdindexType
CMDIndexGPDB::IndexType() const
{
	return m_index_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::Keys
//
//	@doc:
//		Returns the number of index keys
//
//---------------------------------------------------------------------------
ULONG
CMDIndexGPDB::Keys() const
{
	return m_index_key_cols_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::KeyAt
//
//	@doc:
//		Returns the n-th key column
//
//---------------------------------------------------------------------------
ULONG
CMDIndexGPDB::KeyAt(ULONG pos) const
{
	return *((*m_index_key_cols_array)[pos]);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::GetKeyPos
//
//	@doc:
//		Return the position of the key column in the index
//
//---------------------------------------------------------------------------
ULONG
CMDIndexGPDB::GetKeyPos(ULONG column) const
{
	const ULONG size = Keys();

	for (ULONG ul = 0; ul < size; ul++)
	{
		if (KeyAt(ul) == column)
		{
			return ul;
		}
	}

	return gpos::ulong_max;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::IncludedCols
//
//	@doc:
//		Returns the number of included columns
//
//---------------------------------------------------------------------------
ULONG
CMDIndexGPDB::IncludedCols() const
{
	return m_included_cols_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::IncludedColAt
//
//	@doc:
//		Returns the n-th included column
//
//---------------------------------------------------------------------------
ULONG
CMDIndexGPDB::IncludedColAt(ULONG pos) const
{
	return *((*m_included_cols_array)[pos]);
}


ULONG
CMDIndexGPDB::KeySortOrderAt(ULONG pos) const
{
	return *((*m_index_key_cols_sort_order)[pos]);
}


ULONG
CMDIndexGPDB::KeyNullOrderAt(ULONG pos) const
{
	return *((*m_index_key_cols_nulls_order)[pos]);
}
//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::GetIncludedColPos
//
//	@doc:
//		Return the position of the included column in the index
//
//---------------------------------------------------------------------------
ULONG
CMDIndexGPDB::GetIncludedColPos(ULONG column) const
{
	const ULONG size = IncludedCols();

	for (ULONG ul = 0; ul < size; ul++)
	{
		if (IncludedColAt(ul) == column)
		{
			return ul;
		}
	}

	GPOS_ASSERT("Column not found in Index's included columns");

	return gpos::ulong_max;
}


//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::Serialize
//
//	@doc:
//		Serialize MD index in DXL format
//
//---------------------------------------------------------------------------
void
CMDIndexGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenIndex));

	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenIndexClustered), m_clustered);

	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenIndexType),
								 GetDXLStr(m_index_type));
	if (nullptr != m_mdid_item_type)
	{
		m_mdid_item_type->Serialize(
			xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenIndexItemType));
	}

	// serialize index keys
	CWStringDynamic *index_key_cols_str =
		CDXLUtils::Serialize(m_mp, m_index_key_cols_array);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenIndexKeyCols), index_key_cols_str);
	GPOS_DELETE(index_key_cols_str);

	CWStringDynamic *available_cols_str =
		CDXLUtils::Serialize(m_mp, m_included_cols_array);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenIndexIncludedCols),
		available_cols_str);
	GPOS_DELETE(available_cols_str);

	if (m_index_type == EmdindBtree)
	{
		CWStringDynamic *index_key_cols_sort_order_str =
			SerializeSortAndNullsOrder(m_mp, m_index_key_cols_sort_order, true);
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenIndexKeyColsSortOrder),
			index_key_cols_sort_order_str);
		GPOS_DELETE(index_key_cols_sort_order_str);

		CWStringDynamic *index_key_cols_nulls_order_str =
			SerializeSortAndNullsOrder(m_mp, m_index_key_cols_nulls_order,
									   false);
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenIndexKeyColsNullsOrder),
			index_key_cols_nulls_order_str);
		GPOS_DELETE(index_key_cols_nulls_order_str);
	}

	// serialize operator class information
	SerializeMDIdList(xml_serializer, m_mdid_opfamilies_array,
					  CDXLTokens::GetDXLTokenStr(EdxltokenOpfamilies),
					  CDXLTokens::GetDXLTokenStr(EdxltokenOpfamily));

	if (IsPartitioned())
	{
		SerializeMDIdList(xml_serializer, m_child_index_oids,
						  CDXLTokens::GetDXLTokenStr(EdxltokenPartitions),
						  CDXLTokens::GetDXLTokenStr(EdxltokenPartition));
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenIndex));
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::DebugPrint
//
//	@doc:
//		Prints a MD index to the provided output
//
//---------------------------------------------------------------------------
void
CMDIndexGPDB::DebugPrint(IOstream &os) const
{
	os << "Index id: ";
	MDId()->OsPrint(os);
	os << std::endl;

	os << "Index name: " << (Mdname()).GetMDName()->GetBuffer() << std::endl;
	os << "Index type: " << GetDXLStr(m_index_type)->GetBuffer() << std::endl;

	os << "Index keys: ";
	for (ULONG ul = 0; ul < Keys(); ul++)
	{
		ULONG ulKey = KeyAt(ul);
		if (ul > 0)
		{
			os << ", ";
		}
		os << ulKey;
	}
	os << std::endl;

	os << "Included Columns: ";
	for (ULONG ul = 0; ul < IncludedCols(); ul++)
	{
		ULONG ulKey = IncludedColAt(ul);
		if (ul > 0)
		{
			os << ", ";
		}
		os << ulKey;
	}
	os << std::endl;
}

#endif	// GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::MdidType
//
//	@doc:
//		Type of items returned by the index
//
//---------------------------------------------------------------------------
IMDId *
CMDIndexGPDB::GetIndexRetItemTypeMdid() const
{
	return m_mdid_item_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDIndexGPDB::IsCompatible
//
//	@doc:
//		Check if given scalar comparison can be used with the index key
// 		at the specified position
//
//---------------------------------------------------------------------------
BOOL
CMDIndexGPDB::IsCompatible(const IMDScalarOp *md_scalar_op, ULONG key_pos) const
{
	GPOS_ASSERT(nullptr != md_scalar_op);

	// In cover indexes the non-key "payload" should not be considered
	// compatible with a predicate
	if (key_pos >= m_mdid_opfamilies_array->Size())
	{
		return false;
	}

	// check if the index opfamily for the key at the given position is one of
	// the families the scalar comparison belongs to
	const IMDId *mdid_opfamily = (*m_mdid_opfamilies_array)[key_pos];

	const ULONG opfamilies_count = md_scalar_op->OpfamiliesCount();

	for (ULONG ul = 0; ul < opfamilies_count; ul++)
	{
		if (mdid_opfamily->Equals(md_scalar_op->OpfamilyMdidAt(ul)))
		{
			return true;
		}
	}

	return false;
}

IMdIdArray *
CMDIndexGPDB::ChildIndexMdids() const
{
	return m_child_index_oids;
}


CWStringDynamic *
CMDIndexGPDB::SerializeSortAndNullsOrder(CMemoryPool *mp,
										 ULongPtrArray *dynamic_ptr_array,
										 bool serialize_Sort) const
{
	CAutoP<CWStringDynamic> string_var(GPOS_NEW(mp) CWStringDynamic(mp));

	if (nullptr == dynamic_ptr_array)
	{
		return string_var.Reset();
	}

	ULONG length = dynamic_ptr_array->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		ULONG value = *((*dynamic_ptr_array)[ul]);
		const CWStringConst *string_repr =
			GetSortAndNullsString(value, serialize_Sort);
		if (ul == length - 1)
		{
			// last element: do not print a comma
			string_var->AppendFormat(string_repr->GetBuffer());
		}
		else
		{
			string_var->AppendFormat(
				GPOS_WSZ_LIT("%ls%ls"), string_repr->GetBuffer(),
				CDXLTokens::GetDXLTokenStr(EdxltokenComma)->GetBuffer());
		}
	}

	return string_var.Reset();
}

const CWStringConst *
CMDIndexGPDB::GetSortAndNullsString(ULONG value, bool serialize_Sort) const
{
	if (serialize_Sort)
	{
		return (value == 1)
				   ? CDXLTokens::GetDXLTokenStr(EdxltokenIndexKeyColSortDESC)
				   : CDXLTokens::GetDXLTokenStr(EdxltokenIndexKeyColSortASC);
	}
	else
	{
		return (value == 1)
				   ? CDXLTokens::GetDXLTokenStr(EdxltokenIndexKeyColNullFirst)
				   : CDXLTokens::GetDXLTokenStr(EdxltokenIndexKeyColNullLast);
	}
}

// EOF
