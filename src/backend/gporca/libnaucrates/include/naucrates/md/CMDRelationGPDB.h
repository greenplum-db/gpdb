//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDRelationGPDB.h
//
//	@doc:
//		Class representing MD relations
//---------------------------------------------------------------------------



#ifndef GPMD_CMDRelationGPDB_H
#define GPMD_CMDRelationGPDB_H

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/CMDColumn.h"
#include "naucrates/md/CMDName.h"
#include "naucrates/md/IMDColumn.h"
#include "naucrates/md/IMDRelation.h"

namespace gpdxl
{
class CXMLSerializer;
}

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;


//---------------------------------------------------------------------------
//	@class:
//		CMDRelationGPDB
//
//	@doc:
//		Class representing MD relations
//
//---------------------------------------------------------------------------
class CMDRelationGPDB : public IMDRelation
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// DXL for object
	const CWStringDynamic *m_dxl_str;

	// relation mdid
	IMDId *m_mdid;

	// table name
	CMDName *m_mdname;

	// is this a temporary relation
	BOOL m_is_temp_table;

	// storage type
	Erelstoragetype m_rel_storage_type;

	// distribution policy
	Ereldistrpolicy m_rel_distr_policy;

	// columns
	CMDColumnArray *m_md_col_array;

	// number of dropped columns
	ULONG m_dropped_cols;

	// indices of distribution columns
	ULongPtrArray *m_distr_col_array;

	IMdIdArray *m_distr_opfamilies;

	// do we need to consider a hash distributed table as random distributed
	BOOL m_convert_hash_to_random;

	// indices of partition columns
	ULongPtrArray *m_partition_cols_array;

	// partition types
	CharPtrArray *m_str_part_types_array;

	// Child partition oids
	IMdIdArray *m_partition_oids;

	// array of key sets
	ULongPtr2dArray *m_keyset_array;

	// array of index info
	CMDIndexInfoArray *m_mdindex_info_array;

	// array of check constraint mdids
	IMdIdArray *m_mdid_check_constraint_array;

	// partition constraint
	CDXLNode *m_mdpart_constraint;

	// number of system columns
	ULONG m_system_columns;

	// oid of foreign server if this is a foreign relation
	IMDId *m_foreign_server;

	// mapping of column position to positions excluding dropped columns
	UlongToUlongMap *m_colpos_nondrop_colpos_map;

	// mapping of attribute number in the system catalog to the positions of
	// the non dropped column in the metadata object
	IntToUlongMap *m_attrno_nondrop_col_pos_map;

	// the original positions of all the non-dropped columns
	ULongPtrArray *m_nondrop_col_pos_array;

	// array of column widths including dropped columns
	CDoubleArray *m_col_width_array;

public:
	CMDRelationGPDB(const CMDRelationGPDB &) = delete;

	// ctor
	CMDRelationGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
					BOOL is_temp_table, Erelstoragetype rel_storage_type,
					Ereldistrpolicy rel_distr_policy,
					CMDColumnArray *mdcol_array, ULongPtrArray *distr_col_array,
					IMdIdArray *distr_opfamilies,
					ULongPtrArray *partition_cols_array,
					CharPtrArray *str_part_types_array,
					IMdIdArray *partition_oids, BOOL convert_hash_to_random,
					ULongPtr2dArray *keyset_array,
					CMDIndexInfoArray *md_index_info_array,
					IMdIdArray *mdid_check_constraint_array,
					CDXLNode *mdpart_constraint, IMDId *foreign_server);

	// dtor
	~CMDRelationGPDB() override;

	// accessors
	const CWStringDynamic *
	GetStrRepr() const override
	{
		return m_dxl_str;
	}

	// the metadata id
	IMDId *MDId() const override;

	// relation name
	CMDName Mdname() const override;

	// is this a temp relation
	BOOL IsTemporary() const override;

	// storage type (heap, appendonly, ...)
	Erelstoragetype RetrieveRelStorageType() const override;

	// distribution policy (none, hash, random)
	Ereldistrpolicy GetRelDistribution() const override;

	// number of columns
	ULONG ColumnCount() const override;

	// width of a column with regards to the position
	DOUBLE ColWidth(ULONG pos) const override;

	// does relation have dropped columns
	BOOL HasDroppedColumns() const override;

	// number of non-dropped columns
	ULONG NonDroppedColsCount() const override;

	// return the absolute position of the given attribute position excluding dropped columns
	ULONG NonDroppedColAt(ULONG pos) const override;

	// return the position of a column in the metadata object given the attribute number in the system catalog
	ULONG GetPosFromAttno(INT attno) const override;

	// return the original positions of all the non-dropped columns
	ULongPtrArray *NonDroppedColsArray() const override;

	// number of system columns
	ULONG SystemColumnsCount() const override;

	// retrieve the column at the given position
	const IMDColumn *GetMdCol(ULONG pos) const override;

	// number of key sets
	ULONG KeySetCount() const override;

	// key set at given position
	const ULongPtrArray *KeySetAt(ULONG pos) const override;

	// number of distribution columns
	ULONG DistrColumnCount() const override;

	// retrieve the column at the given position in the distribution columns list for the relation
	const IMDColumn *GetDistrColAt(ULONG pos) const override;

	IMDId *GetDistrOpfamilyAt(ULONG pos) const override;

	// return true if a hash distributed table needs to be considered as random
	BOOL ConvertHashToRandom() const override;

	// is this a partitioned table
	BOOL IsPartitioned() const override;

	// number of partition keys
	ULONG PartColumnCount() const override;

	// retrieve the partition key column at the given position
	const IMDColumn *PartColAt(ULONG pos) const override;

	// retrieve list of partition types
	CharPtrArray *GetPartitionTypes() const override;

	// retrieve the partition type of the given level
	CHAR PartTypeAtLevel(ULONG ulLevel) const override;

	// number of indices
	ULONG IndexCount() const override;

	// retrieve the id of the metadata cache index at the given position
	IMDId *IndexMDidAt(ULONG pos) const override;

	// serialize metadata relation in DXL format given a serializer object
	void Serialize(gpdxl::CXMLSerializer *) const override;

	// number of check constraints
	ULONG CheckConstraintCount() const override;

	// retrieve the id of the check constraint cache at the given position
	IMDId *CheckConstraintMDidAt(ULONG pos) const override;

	// part constraint
	CDXLNode *MDPartConstraint() const override;

	// child partition oids
	IMdIdArray *ChildPartitionMdids() const override;

	IMDId *ForeignServer() const override;

#ifdef GPOS_DEBUG
	// debug print of the metadata relation
	void DebugPrint(IOstream &os) const override;
#endif
};
}  // namespace gpmd



#endif	// !GPMD_CMDRelationGPDB_H

// EOF
