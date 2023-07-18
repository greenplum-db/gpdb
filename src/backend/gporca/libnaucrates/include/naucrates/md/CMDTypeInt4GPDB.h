//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDTypeInt4GPDB.h
//
//	@doc:
//		Class for representing INT4 types in GPDB
//---------------------------------------------------------------------------



#ifndef GPMD_CMDTypeInt4GPDB_H
#define GPMD_CMDTypeInt4GPDB_H

#include "gpos/base.h"

#include "naucrates/md/CGPDBTypeHelper.h"
#include "naucrates/md/IMDTypeInt4.h"

#define GPDB_INT4_OID OID(23)
#define GPDB_INT4_OPFAMILY OID(1977)
#define GPDB_INT4_LEGACY_OPFAMILY OID(7100)
#define GPDB_INT4_PART_OPFAMILY OID(1976)
#define GPDB_INT4_LENGTH 4
#define GPDB_INT4_EQ_OP OID(96)
#define GPDB_INT4_NEQ_OP OID(518)
#define GPDB_INT4_LT_OP OID(97)
#define GPDB_INT4_LEQ_OP OID(523)
#define GPDB_INT4_GT_OP OID(521)
#define GPDB_INT4_GEQ_OP OID(525)
#define GPDB_INT4_COMP_OP OID(351)
#define GPDB_INT4_EQ_FUNC OID(65)
#define GPDB_INT4_ARRAY_TYPE OID(1007)

#define GPDB_INT4_AGG_MIN OID(2132)
#define GPDB_INT4_AGG_MAX OID(2116)
#define GPDB_INT4_AGG_AVG OID(2101)
#define GPDB_INT4_AGG_SUM OID(2108)
#define GPDB_INT4_AGG_COUNT OID(2147)

// fwd decl
namespace gpdxl
{
class CXMLSerializer;
}

namespace gpnaucrates
{
class IDatumInt4;
}

namespace gpmd
{
using namespace gpos;
using namespace gpnaucrates;

//---------------------------------------------------------------------------
//	@class:
//		CMDTypeInt4GPDB
//
//	@doc:
//		Class for representing INT4 types in GPDB
//
//---------------------------------------------------------------------------
class CMDTypeInt4GPDB : public IMDTypeInt4
{
	friend class CGPDBTypeHelper<CMDTypeInt4GPDB>;

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

	// type name and type
	static CWStringConst m_str;
	static CMDName m_mdname;

	// a null datum of this type (used for statistics comparison)
	IDatum *m_datum_null;

public:
	CMDTypeInt4GPDB(const CMDTypeInt4GPDB &) = delete;

	// ctor
	explicit CMDTypeInt4GPDB(CMemoryPool *mp);

	//dtor
	~CMDTypeInt4GPDB() override;

	// factory method for creating INT4 datums
	IDatumInt4 *CreateInt4Datum(CMemoryPool *mp, INT iValue,
								BOOL is_null) const override;

	// accessors
	const CWStringDynamic *GetStrRepr() override;

	IMDId *MDId() const override;

	IMDId *GetDistrOpfamilyMdid() const override;

	IMDId *GetPartOpfamilyMdid() const override;

	CMDName Mdname() const override;

	// id of specified comparison operator type
	IMDId *GetMdidForCmpType(ECmpType cmp_type) const override;

	// id of specified specified aggregate type
	IMDId *GetMdidForAggType(EAggType agg_type) const override;

	BOOL
	IsRedistributable() const override
	{
		return true;
	}

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

	ULONG
	Length() const override
	{
		return GPDB_INT4_LENGTH;
	}

	// return the GPDB length
	virtual INT
	GetGPDBLength() const
	{
		return GPDB_INT4_LENGTH;
	}

	BOOL
	IsPassedByValue() const override
	{
		return true;
	}

	const IMDId *
	CmpOpMdid() const override
	{
		return m_mdid_op_cmp;
	}

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

	// serialize object in DXL format
	void Serialize(gpdxl::CXMLSerializer *xml_serializer) const override;

	// return the null constant for this type
	IDatum *
	DatumNull() const override
	{
		return m_datum_null;
	}

	// transformation method for generating datum from CDXLScalarConstValue
	IDatum *GetDatumForDXLConstVal(
		const CDXLScalarConstValue *dxl_op) const override;

	// create typed datum from DXL datum
	IDatum *GetDatumForDXLDatum(CMemoryPool *mp,
								const CDXLDatum *dxl_datum) const override;

	// generate the DXL datum from IDatum
	CDXLDatum *GetDatumVal(CMemoryPool *mp, IDatum *datum) const override;

	// generate the DXL datum representing null value
	CDXLDatum *GetDXLDatumNull(CMemoryPool *mp) const override;

	// generate the DXL scalar constant from IDatum
	CDXLScalarConstValue *GetDXLOpScConst(CMemoryPool *mp,
										  IDatum *datum) const override;

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	void DebugPrint(IOstream &os) const override;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDTypeInt4GPDB_H

// EOF
