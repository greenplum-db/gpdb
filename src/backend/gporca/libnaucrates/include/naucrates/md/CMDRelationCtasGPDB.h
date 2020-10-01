//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CMDRelationCtasGPDB.h
//
//	@doc:
//		Class representing MD CTAS relations
//---------------------------------------------------------------------------

#ifndef GPMD_CMDRelationCTASGPDB_H
#define GPMD_CMDRelationCTASGPDB_H

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/md/IMDRelationCtas.h"
#include "naucrates/md/IMDColumn.h"
#include "naucrates/md/CMDColumn.h"
#include "naucrates/md/CMDName.h"

namespace gpdxl
{
// fwd decl
class CXMLSerializer;
class CDXLCtasStorageOptions;
}  // namespace gpdxl

namespace gpmd
{
using namespace gpos;
using namespace gpdxl;


//---------------------------------------------------------------------------
//	@class:
//		CMDRelationCtasGPDB
//
//	@doc:
//		Class representing MD CTAS relations
//
//---------------------------------------------------------------------------
class CMDRelationCtasGPDB : public IMDRelationCtas
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// DXL for object
	const CWStringDynamic *m_dxl_str;

	// relation mdid
	IMDId *m_mdid;

	// schema name
	CMDName *m_mdname_schema;

	// table name
	CMDName *m_mdname;

	// is this a temporary relation
	BOOL m_is_temp_table;

	// does this table have oids
	BOOL m_has_oids;

	// storage type
	Erelstoragetype m_rel_storage_type;

	// distribution policy
	Ereldistrpolicy m_rel_distr_policy;

	// columns
	CMDColumnArray *m_md_col_array;

	// indices of distribution columns
	ULongPtrArray *m_distr_col_array;

	// distribution opfamilies
	IMdIdArray *m_distr_opfamilies;

	// distribution opclasses
	IMdIdArray *m_distr_opclasses;

	// array of key sets
	ULongPtr2dArray *m_keyset_array;

	// number of system columns
	ULONG m_system_columns;

	// mapping of attribute number in the system catalog to the positions of
	// the non dropped column in the metadata object
	IntToUlongMap *m_attrno_nondrop_col_pos_map;

	// the original positions of all the non-dropped columns
	ULongPtrArray *m_nondrop_col_pos_array;

	// storage options
	CDXLCtasStorageOptions *m_dxl_ctas_storage_option;

	// vartypemod list
	IntPtrArray *m_vartypemod_array;

	// array of column widths
	CDoubleArray *m_col_width_array;

	// private copy ctor
	CMDRelationCtasGPDB(const CMDRelationCtasGPDB &);

public:
	// ctor
	CMDRelationCtasGPDB(
		CMemoryPool *mp, IMDId *mdid, CMDName *mdname_schema, CMDName *mdname,
		BOOL fTemporary, BOOL fHasOids, Erelstoragetype rel_storage_type,
		Ereldistrpolicy rel_distr_policy, CMDColumnArray *mdcol_array,
		ULongPtrArray *distr_col_array, IMdIdArray *distr_opfamilies,
		IMdIdArray *distr_opclasses, ULongPtr2dArray *keyset_array,
		CDXLCtasStorageOptions *dxl_ctas_storage_options,
		IntPtrArray *vartypemod_array);

	// dtor
	virtual ~CMDRelationCtasGPDB();

	// accessors
	virtual const CWStringDynamic *
	GetStrRepr() const
	{
		return m_dxl_str;
	}

	// the metadata id
	virtual IMDId *MDId() const;

	// schema name
	virtual CMDName *GetMdNameSchema() const;

	// relation name
	virtual CMDName Mdname() const;

	// distribution policy (none, hash, random)
	virtual Ereldistrpolicy GetRelDistribution() const;

	// does this table have oids
	virtual BOOL
	HasOids() const
	{
		return m_has_oids;
	}

	// is this a temp relation
	virtual BOOL
	IsTemporary() const
	{
		return m_is_temp_table;
	}

	// storage type
	virtual Erelstoragetype
	RetrieveRelStorageType() const
	{
		return m_rel_storage_type;
	}

	// CTAS storage options
	virtual CDXLCtasStorageOptions *
	GetDxlCtasStorageOption() const
	{
		return m_dxl_ctas_storage_option;
	}

	// number of columns
	virtual ULONG ColumnCount() const;

	// width of a column with regards to the position
	virtual DOUBLE ColWidth(ULONG pos) const;

	// does relation have dropped columns
	virtual BOOL
	HasDroppedColumns() const
	{
		return false;
	}

	// number of non-dropped columns
	virtual ULONG
	NonDroppedColsCount() const
	{
		return ColumnCount();
	}

	// return the original positions of all the non-dropped columns
	virtual ULongPtrArray *
	NonDroppedColsArray() const
	{
		return m_nondrop_col_pos_array;
	}

	// number of system columns
	virtual ULONG SystemColumnsCount() const;

	// retrieve the column at the given position
	virtual const IMDColumn *GetMdCol(ULONG pos) const;

	// number of distribution columns
	virtual ULONG DistrColumnCount() const;

	// retrieve the column at the given position in the distribution columns list for the relation
	virtual const IMDColumn *GetDistrColAt(ULONG pos) const;

	virtual IMDId *GetDistrOpfamilyAt(ULONG pos) const;

	// number of indices
	virtual ULONG
	IndexCount() const
	{
		return 0;
	}

	// number of triggers
	virtual ULONG
	TriggerCount() const
	{
		return 0;
	}

	// return the absolute position of the given attribute position excluding dropped columns
	virtual ULONG
	NonDroppedColAt(ULONG pos) const
	{
		return pos;
	}

	// return the position of a column in the metadata object given the attribute number in the system catalog
	virtual ULONG GetPosFromAttno(INT attno) const;

	virtual IMdIdArray *
	GetDistrOpClasses() const
	{
		return m_distr_opclasses;
	}

	// retrieve the id of the metadata cache index at the given position
	virtual IMDId *IndexMDidAt(ULONG  // pos
	) const
	{
		GPOS_ASSERT("CTAS tables have no indexes");
		return 0;
	}

	// retrieve the id of the metadata cache trigger at the given position
	virtual IMDId *TriggerMDidAt(ULONG	// pos
	) const
	{
		GPOS_ASSERT("CTAS tables have no triggers");
		return 0;
	}

	// serialize metadata relation in DXL format given a serializer object
	virtual void Serialize(gpdxl::CXMLSerializer *) const;

	// number of check constraints
	virtual ULONG
	CheckConstraintCount() const
	{
		return 0;
	}

	// retrieve the id of the check constraint cache at the given position
	virtual IMDId *CheckConstraintMDidAt(ULONG	// pos
	) const
	{
		GPOS_ASSERT("CTAS tables have no constraints");
		return 0;
	}

	// list of vartypmod for target expressions
	IntPtrArray *
	GetVarTypeModArray() const
	{
		return m_vartypemod_array;
	}

#ifdef GPOS_DEBUG
	// debug print of the metadata relation
	virtual void DebugPrint(IOstream &os) const;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDRelationCTASGPDB_H

// EOF
