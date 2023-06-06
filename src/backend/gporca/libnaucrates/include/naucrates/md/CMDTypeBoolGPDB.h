//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDTypeBoolGPDB.h
//
//	@doc:
//		Class for representing BOOL types in GPDB
//---------------------------------------------------------------------------



#ifndef GPMD_CMDTypeBoolGPDB_H
#define GPMD_CMDTypeBoolGPDB_H

#include "gpos/base.h"

#include "naucrates/base/IDatumBool.h"
#include "naucrates/md/CGPDBTypeHelper.h"
#include "naucrates/md/IMDTypeBool.h"

#define GPDB_BOOL_OID OID(16)
#define GPDB_BOOL_OPFAMILY OID(2222)
#define GPDB_BOOL_LEGACY_OPFAMILY OID(7124)
#define GPDB_BOOL_PART_OPFAMILY OID(424)
#define GPDB_BOOL_LENGTH 1
#define GPDB_BOOL_EQ_OP OID(91)
#define GPDB_BOOL_NEQ_OP OID(85)
#define GPDB_BOOL_LT_OP OID(58)
#define GPDB_BOOL_LEQ_OP OID(1694)
#define GPDB_BOOL_GT_OP OID(59)
#define GPDB_BOOL_GEQ_OP OID(1695)
#define GPDB_BOOL_COMP_OP OID(1693)
#define GPDB_BOOL_ARRAY_TYPE OID(1000)
#define GPDB_BOOL_AGG_MIN OID(0)
#define GPDB_BOOL_AGG_MAX OID(0)
#define GPDB_BOOL_AGG_AVG OID(0)
#define GPDB_BOOL_AGG_SUM OID(0)
#define GPDB_BOOL_AGG_COUNT OID(2147)

// fwd decl
namespace gpdxl
{
class CXMLSerializer;
}

namespace gpmd
{
using namespace gpos;
using namespace gpnaucrates;


//---------------------------------------------------------------------------
//	@class:
//		CMDTypeBoolGPDB
//
//	@doc:
//		Class for representing BOOL types in GPDB
//
//---------------------------------------------------------------------------
class CMDTypeBoolGPDB : public IMDTypeBool
{
	friend class CGPDBTypeHelper<CMDTypeBoolGPDB>;

private:
	// memory pool
	CMemoryPool *m_mp;

	// type id
	IMDId *m_mdid;
	IMDId *m_distr_opfamily;
	IMDId *m_legacy_distr_opfamily;
	IMDId *m_part_opfamily;

	// mdids of different operators
	IMDId *m_mdid_op_eq;
	IMDId *m_mdid_op_neq;
	IMDId *m_mdid_op_lt;
	IMDId *m_mdid_op_leq;
	IMDId *m_mdid_op_gt;
	IMDId *m_mdid_op_geq;
	IMDId *m_mdid_op_cmp;
	IMDId *m_mdid_type_array;

	// min aggregate
	IMDId *m_mdid_min;

	// max aggregate
	IMDId *m_mdid_max;

	// avg aggregate
	IMDId *m_mdid_avg;

	// sum aggregate
	IMDId *m_mdid_sum;

	// count aggregate
	IMDId *m_mdid_count;

	// DXL for object
	const CWStringDynamic *m_dxl_str = nullptr;

	// type name and id
	static CWStringConst m_str;
	static CMDName m_mdname;

	// a null datum of this type (used for statistics comparison)
	IDatum *m_datum_null;

public:
	CMDTypeBoolGPDB(const CMDTypeBoolGPDB &) = delete;

	// ctor
	explicit CMDTypeBoolGPDB(CMemoryPool *mp);

	// dtor
	~CMDTypeBoolGPDB() override;

	// accessors
	const CWStringDynamic *GetStrRepr() override;

	// type id
	IMDId *MDId() const override;

	IMDId *GetDistrOpfamilyMdid() const override;

	IMDId *GetPartOpfamilyMdid() const override;

	// type name
	CMDName Mdname() const override;

	// is type redistributable
	BOOL
	IsRedistributable() const override
	{
		return true;
	}

	// is type fixed length
	BOOL
	IsFixedLength() const override
	{
		return true;
	}

	// is type composite
	BOOL
	IsComposite() const override
	{
		return false;
	}

	// type length
	ULONG
	Length() const override
	{
		return GPDB_BOOL_LENGTH;
	}

	// return the GPDB length
	virtual INT
	GetGPDBLength() const
	{
		return GPDB_BOOL_LENGTH;
	}

	// is type passed by value
	BOOL
	IsPassedByValue() const override
	{
		return true;
	}

	// id of specified comparison operator type
	IMDId *GetMdidForCmpType(ECmpType cmp_type) const override;

	const IMDId *
	CmpOpMdid() const override
	{
		return m_mdid_op_cmp;
	}

	// id of specified specified aggregate type
	IMDId *GetMdidForAggType(EAggType agg_type) const override;

	// is type hashable
	BOOL
	IsHashable() const override
	{
		return true;
	}

	// is type merge joinable
	BOOL
	IsMergeJoinable() const override
	{
		return true;
	}

	// array type id
	IMDId *
	GetArrayTypeMdid() const override
	{
		return m_mdid_type_array;
	}

	// id of the relation corresponding to a composite type
	IMDId *
	GetBaseRelMdid() const override
	{
		return nullptr;
	}

	// return the null constant for this type
	IDatum *
	DatumNull() const override
	{
		return m_datum_null;
	}

	// factory method for creating constants
	IDatumBool *CreateBoolDatum(CMemoryPool *mp, BOOL fValue,
								BOOL is_null) const override;

	// create typed datum from DXL datum
	IDatum *GetDatumForDXLDatum(CMemoryPool *mp,
								const CDXLDatum *dxl_datum) const override;

	// serialize object in DXL format
	void Serialize(gpdxl::CXMLSerializer *xml_serializer) const override;

	// transformation function to generate datum from CDXLScalarConstValue
	IDatum *GetDatumForDXLConstVal(
		const CDXLScalarConstValue *dxl_op) const override;

	// generate the DXL datum from IDatum
	CDXLDatum *GetDatumVal(CMemoryPool *mp, IDatum *datum) const override;

	// generate the DXL scalar constant from IDatum
	CDXLScalarConstValue *GetDXLOpScConst(CMemoryPool *mp,
										  IDatum *datum) const override;

	// generate the DXL datum representing null value
	CDXLDatum *GetDXLDatumNull(CMemoryPool *mp) const override;

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	void DebugPrint(IOstream &os) const override;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDTypeBoolGPDB_H

// EOF
