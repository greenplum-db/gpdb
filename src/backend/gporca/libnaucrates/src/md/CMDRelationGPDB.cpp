//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDRelationGPDB.cpp
//
//	@doc:
//		Implementation of the class for representing metadata cache relations
//---------------------------------------------------------------------------


#include "naucrates/md/CMDRelationGPDB.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/exception.h"

using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::CMDRelationGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDRelationGPDB::CMDRelationGPDB(
	CMemoryPool *mp, IMDId *mdid, CMDName *mdname, BOOL fTemporary,
	Erelstoragetype rel_storage_type, Ereldistrpolicy rel_distr_policy,
	CMDColumnArray *mdcol_array, ULongPtrArray *distr_col_array,
	IMdIdArray *distr_opfamilies, ULongPtrArray *partition_cols_array,
	CharPtrArray *str_part_types_array, IMdIdArray *partition_oids,
	BOOL convert_hash_to_random, ULongPtr2dArray *keyset_array,
	CMDIndexInfoArray *md_index_info_array,
	IMdIdArray *mdid_check_constraint_array, CDXLNode *mdpart_constraint,
	IMDId *foreign_server)
	: m_mp(mp),
	  m_mdid(mdid),
	  m_mdname(mdname),
	  m_is_temp_table(fTemporary),
	  m_rel_storage_type(rel_storage_type),
	  m_rel_distr_policy(rel_distr_policy),
	  m_md_col_array(mdcol_array),
	  m_dropped_cols(0),
	  m_distr_col_array(distr_col_array),
	  m_distr_opfamilies(distr_opfamilies),
	  m_convert_hash_to_random(convert_hash_to_random),
	  m_partition_cols_array(partition_cols_array),
	  m_str_part_types_array(str_part_types_array),
	  m_partition_oids(partition_oids),
	  m_keyset_array(keyset_array),
	  m_mdindex_info_array(md_index_info_array),
	  m_mdid_check_constraint_array(mdid_check_constraint_array),
	  m_mdpart_constraint(mdpart_constraint),
	  m_system_columns(0),
	  m_foreign_server(foreign_server),
	  m_colpos_nondrop_colpos_map(nullptr),
	  m_attrno_nondrop_col_pos_map(nullptr),
	  m_nondrop_col_pos_array(nullptr)
{
	GPOS_ASSERT(mdid->IsValid());
	GPOS_ASSERT(nullptr != mdcol_array);
	GPOS_ASSERT(nullptr != md_index_info_array);
	GPOS_ASSERT(nullptr != mdid_check_constraint_array);
	GPOS_ASSERT_IMP(
		convert_hash_to_random,
		IMDRelation::EreldistrHash == rel_distr_policy &&
			"Converting hash distributed table to random only possible for hash distributed tables");
	GPOS_ASSERT(nullptr == distr_opfamilies ||
				distr_opfamilies->Size() == m_distr_col_array->Size());

	m_colpos_nondrop_colpos_map = GPOS_NEW(m_mp) UlongToUlongMap(m_mp);
	m_attrno_nondrop_col_pos_map = GPOS_NEW(m_mp) IntToUlongMap(m_mp);
	m_nondrop_col_pos_array = GPOS_NEW(m_mp) ULongPtrArray(m_mp);
	m_col_width_array = GPOS_NEW(mp) CDoubleArray(mp);

	const ULONG arity = mdcol_array->Size();
	ULONG non_dropped_col_pos = 0;
	for (ULONG ul = 0; ul < arity; ul++)
	{
		IMDColumn *mdcol = (*mdcol_array)[ul];
		BOOL is_system_col = mdcol->IsSystemColumn();
		if (is_system_col)
		{
			m_system_columns++;
		}

		(void) m_attrno_nondrop_col_pos_map->Insert(
			GPOS_NEW(m_mp) INT(mdcol->AttrNum()), GPOS_NEW(m_mp) ULONG(ul));

		if (mdcol->IsDropped())
		{
			m_dropped_cols++;
		}
		else
		{
			if (!is_system_col)
			{
				m_nondrop_col_pos_array->Append(GPOS_NEW(m_mp) ULONG(ul));
			}
			(void) m_colpos_nondrop_colpos_map->Insert(
				GPOS_NEW(m_mp) ULONG(ul),
				GPOS_NEW(m_mp) ULONG(non_dropped_col_pos));
			non_dropped_col_pos++;
		}

		m_col_width_array->Append(GPOS_NEW(mp) CDouble(mdcol->Length()));
	}
	m_dxl_str = CDXLUtils::SerializeMDObj(
		m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::~CMDRelationGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDRelationGPDB::~CMDRelationGPDB()
{
	GPOS_DELETE(m_mdname);
	GPOS_DELETE(m_dxl_str);
	m_mdid->Release();
	m_md_col_array->Release();
	CRefCount::SafeRelease(m_distr_col_array);
	CRefCount::SafeRelease(m_distr_opfamilies);
	CRefCount::SafeRelease(m_partition_oids);
	CRefCount::SafeRelease(m_partition_cols_array);
	CRefCount::SafeRelease(m_str_part_types_array);
	CRefCount::SafeRelease(m_keyset_array);
	m_mdindex_info_array->Release();
	m_mdid_check_constraint_array->Release();
	m_col_width_array->Release();
	CRefCount::SafeRelease(m_foreign_server);
	CRefCount::SafeRelease(m_mdpart_constraint);
	CRefCount::SafeRelease(m_colpos_nondrop_colpos_map);
	CRefCount::SafeRelease(m_attrno_nondrop_col_pos_map);
	CRefCount::SafeRelease(m_nondrop_col_pos_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::MDId
//
//	@doc:
//		Returns the metadata id of this relation
//
//---------------------------------------------------------------------------
IMDId *
CMDRelationGPDB::MDId() const
{
	return m_mdid;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::Mdname
//
//	@doc:
//		Returns the name of this relation
//
//---------------------------------------------------------------------------
CMDName
CMDRelationGPDB::Mdname() const
{
	return *m_mdname;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::IsTemporary
//
//	@doc:
//		Is the relation temporary
//
//---------------------------------------------------------------------------
BOOL
CMDRelationGPDB::IsTemporary() const
{
	return m_is_temp_table;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::RetrieveRelStorageType
//
//	@doc:
//		Returns the storage type for this relation
//
//---------------------------------------------------------------------------
IMDRelation::Erelstoragetype
CMDRelationGPDB::RetrieveRelStorageType() const
{
	return m_rel_storage_type;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::GetRelDistribution
//
//	@doc:
//		Returns the distribution policy for this relation
//
//---------------------------------------------------------------------------
IMDRelation::Ereldistrpolicy
CMDRelationGPDB::GetRelDistribution() const
{
	return m_rel_distr_policy;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::ColumnCount
//
//	@doc:
//		Returns the number of columns of this relation
//
//---------------------------------------------------------------------------
ULONG
CMDRelationGPDB::ColumnCount() const
{
	GPOS_ASSERT(nullptr != m_md_col_array);

	return m_md_col_array->Size();
}

// Return the width of a column with regards to the position
DOUBLE
CMDRelationGPDB::ColWidth(ULONG pos) const
{
	return (*m_col_width_array)[pos]->Get();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::HasDroppedColumns
//
//	@doc:
//		Does relation have dropped columns
//
//---------------------------------------------------------------------------
BOOL
CMDRelationGPDB::HasDroppedColumns() const
{
	return 0 < m_dropped_cols;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::NonDroppedColsCount
//
//	@doc:
//		Number of non-dropped columns
//
//---------------------------------------------------------------------------
ULONG
CMDRelationGPDB::NonDroppedColsCount() const
{
	return ColumnCount() - m_dropped_cols;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::NonDroppedColAt
//
//	@doc:
//		Return the absolute position of the given attribute position excluding
//		dropped columns
//
//---------------------------------------------------------------------------
ULONG
CMDRelationGPDB::NonDroppedColAt(ULONG pos) const
{
	GPOS_ASSERT(pos <= ColumnCount());

	if (!HasDroppedColumns())
	{
		return pos;
	}

	ULONG *colid = m_colpos_nondrop_colpos_map->Find(&pos);

	GPOS_ASSERT(nullptr != colid);
	return *colid;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::GetPosFromAttno
//
//	@doc:
//		Return the position of a column in the metadata object given the
//      attribute number in the system catalog
//---------------------------------------------------------------------------
ULONG
CMDRelationGPDB::GetPosFromAttno(INT attno) const
{
	ULONG *att_pos = m_attrno_nondrop_col_pos_map->Find(&attno);
	GPOS_ASSERT(nullptr != att_pos);

	return *att_pos;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::NonDroppedColsArray
//
//	@doc:
//		Returns the original positions of all the non-dropped columns
//
//---------------------------------------------------------------------------
ULongPtrArray *
CMDRelationGPDB::NonDroppedColsArray() const
{
	return m_nondrop_col_pos_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::SystemColumnsCount
//
//	@doc:
//		Returns the number of system columns of this relation
//
//---------------------------------------------------------------------------
ULONG
CMDRelationGPDB::SystemColumnsCount() const
{
	return m_system_columns;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::KeySetCount
//
//	@doc:
//		Returns the number of key sets
//
//---------------------------------------------------------------------------
ULONG
CMDRelationGPDB::KeySetCount() const
{
	return (m_keyset_array == nullptr) ? 0 : m_keyset_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::KeySetAt
//
//	@doc:
//		Returns the key set at the specified position
//
//---------------------------------------------------------------------------
const ULongPtrArray *
CMDRelationGPDB::KeySetAt(ULONG pos) const
{
	GPOS_ASSERT(nullptr != m_keyset_array);

	return (*m_keyset_array)[pos];
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::DistrColumnCount
//
//	@doc:
//		Returns the number of columns in the distribution column list of this relation
//
//---------------------------------------------------------------------------
ULONG
CMDRelationGPDB::DistrColumnCount() const
{
	return (m_distr_col_array == nullptr) ? 0 : m_distr_col_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::IsPartitioned
//
//	@doc:
//		Is the table partitioned
//
//---------------------------------------------------------------------------
BOOL
CMDRelationGPDB::IsPartitioned() const
{
	return (0 < PartColumnCount());
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::PartColumnCount
//
//	@doc:
//		Returns the number of partition keys
//
//---------------------------------------------------------------------------
ULONG
CMDRelationGPDB::PartColumnCount() const
{
	return (m_partition_cols_array == nullptr) ? 0
											   : m_partition_cols_array->Size();
}

// Retrieve list of partition types
CharPtrArray *
CMDRelationGPDB::GetPartitionTypes() const
{
	return m_str_part_types_array;
}

// Returns the partition type of the given level
CHAR
CMDRelationGPDB::PartTypeAtLevel(ULONG ulLevel) const
{
	return *(*m_str_part_types_array)[ulLevel];
}


//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::PartColAt
//
//	@doc:
//		Returns the partition column at the specified position in the
//		partition key list
//
//---------------------------------------------------------------------------
const IMDColumn *
CMDRelationGPDB::PartColAt(ULONG pos) const
{
	ULONG partition_key_pos = (*(*m_partition_cols_array)[pos]);
	return GetMdCol(partition_key_pos);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::IndexCount
//
//	@doc:
//		Returns the number of indices of this relation
//
//---------------------------------------------------------------------------
ULONG
CMDRelationGPDB::IndexCount() const
{
	return m_mdindex_info_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::GetMdCol
//
//	@doc:
//		Returns the column at the specified position
//
//---------------------------------------------------------------------------
const IMDColumn *
CMDRelationGPDB::GetMdCol(ULONG pos) const
{
	GPOS_ASSERT(pos < m_md_col_array->Size());

	return (*m_md_col_array)[pos];
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::GetDistrColAt
//
//	@doc:
//		Returns the distribution column at the specified position in the distribution column list
//
//---------------------------------------------------------------------------
const IMDColumn *
CMDRelationGPDB::GetDistrColAt(ULONG pos) const
{
	GPOS_ASSERT(pos < m_distr_col_array->Size());

	ULONG distr_key_pos = (*(*m_distr_col_array)[pos]);
	return GetMdCol(distr_key_pos);
}

IMDId *
CMDRelationGPDB::GetDistrOpfamilyAt(ULONG pos) const
{
	if (m_distr_opfamilies == nullptr)
	{
		GPOS_RAISE(CException::ExmaInvalid, CException::ExmiInvalid,
				   GPOS_WSZ_LIT("GetDistrOpfamilyAt() returning NULL."));
	}

	GPOS_ASSERT(pos < m_distr_opfamilies->Size());
	return (*m_distr_opfamilies)[pos];
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::ConvertHashToRandom
//
//	@doc:
//		Return true if a hash distributed table needs to be considered as random during planning
//---------------------------------------------------------------------------
BOOL
CMDRelationGPDB::ConvertHashToRandom() const
{
	return m_convert_hash_to_random;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::IndexMDidAt
//
//	@doc:
//		Returns the id of the index at the specified position of the index array
//
//---------------------------------------------------------------------------
IMDId *
CMDRelationGPDB::IndexMDidAt(ULONG pos) const
{
	return (*m_mdindex_info_array)[pos]->MDId();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::CheckConstraintCount
//
//	@doc:
//		Returns the number of check constraints on this relation
//
//---------------------------------------------------------------------------
ULONG
CMDRelationGPDB::CheckConstraintCount() const
{
	return m_mdid_check_constraint_array->Size();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::CheckConstraintMDidAt
//
//	@doc:
//		Returns the id of the check constraint at the specified position of
//		the check constraint array
//
//---------------------------------------------------------------------------
IMDId *
CMDRelationGPDB::CheckConstraintMDidAt(ULONG pos) const
{
	return (*m_mdid_check_constraint_array)[pos];
}

//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::MDPartConstraint
//
//	@doc:
//		Return the part constraint
//
//---------------------------------------------------------------------------
CDXLNode *
CMDRelationGPDB::MDPartConstraint() const
{
	return m_mdpart_constraint;
}

IMDId *
CMDRelationGPDB::ForeignServer() const
{
	return m_foreign_server;
}


//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::Serialize
//
//	@doc:
//		Serialize relation metadata in DXL format
//
//---------------------------------------------------------------------------
void
CMDRelationGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	GPOS_CHECK_ABORT;

	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenRelation));

	m_mdid->Serialize(xml_serializer,
					  CDXLTokens::GetDXLTokenStr(EdxltokenMdid));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenName),
								 m_mdname->GetMDName());
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenRelTemporary), m_is_temp_table);
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenRelStorageType),
		IMDRelation::GetStorageTypeStr(m_rel_storage_type));
	xml_serializer->AddAttribute(
		CDXLTokens::GetDXLTokenStr(EdxltokenRelDistrPolicy),
		GetDistrPolicyStr(m_rel_distr_policy));

	if (EreldistrHash == m_rel_distr_policy)
	{
		GPOS_ASSERT(nullptr != m_distr_col_array);

		// serialize distribution columns
		CWStringDynamic *distr_col_str_array =
			ColumnsToStr(m_mp, m_distr_col_array);
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenDistrColumns),
			distr_col_str_array);
		GPOS_DELETE(distr_col_str_array);
	}

	// serialize key sets
	if (m_keyset_array != nullptr && m_keyset_array->Size() > 0)
	{
		CWStringDynamic *keyset_str_array =
			CDXLUtils::Serialize(m_mp, m_keyset_array);
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenKeys),
									 keyset_str_array);
		GPOS_DELETE(keyset_str_array);
	}

	if (IsPartitioned())
	{
		// Fall back, instead of segfaulting when m_partition_oids is NULL
		// (e.g in minidumps)
		GPOS_RTL_ASSERT(nullptr != m_partition_oids);

		// serialize partition keys
		CWStringDynamic *part_keys_str_array =
			CDXLUtils::Serialize(m_mp, m_partition_cols_array);
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenPartKeys), part_keys_str_array);
		GPOS_DELETE(part_keys_str_array);
	}

	if (m_str_part_types_array)
	{
		// serialize partition types
		CWStringDynamic *part_types_str_array =
			CDXLUtils::SerializeToCommaSeparatedString(m_mp,
													   m_str_part_types_array);
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenPartTypes),
			part_types_str_array);
		GPOS_DELETE(part_types_str_array);
	}

	if (m_convert_hash_to_random)
	{
		xml_serializer->AddAttribute(
			CDXLTokens::GetDXLTokenStr(EdxltokenConvertHashToRandom),
			m_convert_hash_to_random);
	}

	if (m_foreign_server)
	{
		m_foreign_server->Serialize(
			xml_serializer,
			CDXLTokens::GetDXLTokenStr(EdxltokenRelForeignServer));
	}
	// serialize columns
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));
	for (ULONG ul = 0; ul < m_md_col_array->Size(); ul++)
	{
		CMDColumn *mdcol = (*m_md_col_array)[ul];
		mdcol->Serialize(xml_serializer);

		GPOS_CHECK_ABORT;
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenColumns));

	// serialize index infos
	xml_serializer->OpenElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenIndexInfoList));
	const ULONG indexes = m_mdindex_info_array->Size();
	for (ULONG ul = 0; ul < indexes; ul++)
	{
		CMDIndexInfo *index_info = (*m_mdindex_info_array)[ul];
		index_info->Serialize(xml_serializer);

		GPOS_CHECK_ABORT;
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenIndexInfoList));


	// serialize check constraint information
	SerializeMDIdList(xml_serializer, m_mdid_check_constraint_array,
					  CDXLTokens::GetDXLTokenStr(EdxltokenCheckConstraints),
					  CDXLTokens::GetDXLTokenStr(EdxltokenCheckConstraint));

	// serialize operator class information, if present
	if (EreldistrHash == m_rel_distr_policy && nullptr != m_distr_opfamilies)
	{
		SerializeMDIdList(
			xml_serializer, m_distr_opfamilies,
			CDXLTokens::GetDXLTokenStr(EdxltokenRelDistrOpfamilies),
			CDXLTokens::GetDXLTokenStr(EdxltokenRelDistrOpfamily));
	}

	// serialize part constraint
	if (nullptr != m_mdpart_constraint)
	{
		xml_serializer->OpenElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenPartConstraint));

		// serialize the scalar expression
		if (nullptr != m_mdpart_constraint)
		{
			m_mdpart_constraint->SerializeToDXL(xml_serializer);
		}

		xml_serializer->CloseElement(
			CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
			CDXLTokens::GetDXLTokenStr(EdxltokenPartConstraint));

		GPOS_CHECK_ABORT;
	}

	if (IsPartitioned())
	{
		SerializeMDIdList(xml_serializer, m_partition_oids,
						  CDXLTokens::GetDXLTokenStr(EdxltokenPartitions),
						  CDXLTokens::GetDXLTokenStr(EdxltokenPartition));
	}

	xml_serializer->CloseElement(
		CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
		CDXLTokens::GetDXLTokenStr(EdxltokenRelation));

	GPOS_CHECK_ABORT;
}

IMdIdArray *
CMDRelationGPDB::ChildPartitionMdids() const
{
	return m_partition_oids;
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CMDRelationGPDB::DebugPrint
//
//	@doc:
//		Prints a metadata cache relation to the provided output
//
//---------------------------------------------------------------------------
void
CMDRelationGPDB::DebugPrint(IOstream &os) const
{
	os << "Relation id: ";
	MDId()->OsPrint(os);
	os << std::endl;

	os << "Relation name: " << (Mdname()).GetMDName()->GetBuffer() << std::endl;

	os << "Storage type: "
	   << IMDRelation::GetStorageTypeStr(m_rel_storage_type)->GetBuffer()
	   << std::endl;

	os << "Distribution policy: "
	   << GetDistrPolicyStr(m_rel_distr_policy)->GetBuffer() << std::endl;

	os << "Relation columns: " << std::endl;
	const ULONG num_of_columns = ColumnCount();
	for (ULONG ul = 0; ul < num_of_columns; ul++)
	{
		const IMDColumn *mdcol = GetMdCol(ul);
		mdcol->DebugPrint(os);
	}
	os << std::endl;

	os << "Distributed by: ";
	const ULONG distr_columns = DistrColumnCount();
	for (ULONG ul = 0; ul < distr_columns; ul++)
	{
		if (0 < ul)
		{
			os << ", ";
		}

		const IMDColumn *mdcol_distr_key = GetDistrColAt(ul);
		os << (mdcol_distr_key->Mdname()).GetMDName()->GetBuffer();
	}

	os << std::endl;

	os << "Partition keys: ";
	const ULONG part_columns = PartColumnCount();
	for (ULONG ul = 0; ul < part_columns; ul++)
	{
		if (0 < ul)
		{
			os << ", ";
		}

		const IMDColumn *mdcol_part_key = PartColAt(ul);
		os << (mdcol_part_key->Mdname()).GetMDName()->GetBuffer();
	}

	os << std::endl;

	os << "Index Info: ";
	const ULONG indexes = m_mdindex_info_array->Size();
	for (ULONG ul = 0; ul < indexes; ul++)
	{
		CMDIndexInfo *mdindex_info = (*m_mdindex_info_array)[ul];
		mdindex_info->DebugPrint(os);
	}

	os << "Check Constraint: ";
	CDXLUtils::DebugPrintMDIdArray(os, m_mdid_check_constraint_array);
}

#endif	// GPOS_DEBUG

// EOF
