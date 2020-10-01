//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDTypeGenericGPDB.h
//
//	@doc:
//		Class representing GPDB generic types
//---------------------------------------------------------------------------

#ifndef GPMD_CMDTypeGenericGPDB_H
#define GPMD_CMDTypeGenericGPDB_H

#include "gpos/base.h"

#include "naucrates/dxl/gpdb_types.h"

#include "naucrates/md/IMDTypeGeneric.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/CGPDBTypeHelper.h"

// some metadata ids for types that don't have their specific header files (yet)
// keep this in sync with Postgres file pg_operator.h
#define GPDB_TEXT_EQ_OP OID(98)
#define GPDB_TEXT_NEQ_OP OID(531)
#define GPDB_TEXT_LT_OP OID(664)
#define GPDB_TEXT_LEQ_OP OID(665)
#define GPDB_TEXT_GT_OP OID(666)
#define GPDB_TEXT_GEQ_OP OID(667)


// fwd decl
namespace gpdxl
{
class CXMLSerializer;
}

namespace gpmd
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CMDTypeGenericGPDB
//
//	@doc:
//		Class representing GPDB generic types
//
//---------------------------------------------------------------------------
class CMDTypeGenericGPDB : public IMDTypeGeneric
{
	friend class CGPDBTypeHelper<CMDTypeGenericGPDB>;

private:
	// memory pool
	CMemoryPool *m_mp;

	// DXL for object
	const CWStringDynamic *m_dxl_str;

	// metadata id
	IMDId *m_mdid;

	// type name
	CMDName *m_mdname;

	// can type be redistributed based on non-legacy distr opfamily
	BOOL m_is_redistributable;

	// is this a fixed-length type
	BOOL m_is_fixed_length;

	// type length in number of bytes for fixed-length types, 0 otherwise
	ULONG m_length;

	// is type passed by value or by reference
	BOOL m_is_passed_by_value;

	IMDId *m_distr_opfamily;

	IMDId *m_legacy_distr_opfamily;

	// id of equality operator for type
	IMDId *m_mdid_op_eq;

	// id of inequality operator for type
	IMDId *m_mdid_op_neq;

	// id of less than operator for type
	IMDId *m_mdid_op_lt;

	// id of less than equals operator for type
	IMDId *m_mdid_op_leq;

	// id of greater than operator for type
	IMDId *m_mdid_op_gt;

	// id of greater than equals operator for type
	IMDId *m_mdid_op_geq;

	// id of comparison operator for type used in btree lookups
	IMDId *m_mdid_op_cmp;

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

	// is type hashable
	BOOL m_is_hashable;

	// is type merge joinable using '='
	BOOL m_is_merge_joinable;

	// is type composite
	BOOL m_is_composite_type;

	// is type text related
	BOOL m_is_text_related;

	// id of the relation corresponding to a composite type
	IMDId *m_mdid_base_relation;

	// id of array type for type
	IMDId *m_mdid_type_array;

	// GPDB specific length
	INT m_gpdb_length;

	// a null datum of this type (used for statistics comparison)
	IDatum *m_datum_null;

	// private copy ctor
	CMDTypeGenericGPDB(const CMDTypeGenericGPDB &);

public:
	// ctor
	CMDTypeGenericGPDB(
		CMemoryPool *mp, IMDId *mdid, CMDName *mdname, BOOL is_redistributable,
		BOOL is_fixed_length, ULONG length, BOOL is_passed_by_value,
		IMDId *mdid_distr_opfamily, IMDId *mdid_legacy_distr_opfamily,
		IMDId *mdid_op_eq, IMDId *mdid_op_neq, IMDId *mdid_op_lt,
		IMDId *mdid_op_leq, IMDId *mdid_op_gt, IMDId *mdid_op_geq,
		IMDId *mdid_op_cmp, IMDId *pmdidMin, IMDId *pmdidMax, IMDId *pmdidAvg,
		IMDId *pmdidSum, IMDId *pmdidCount, BOOL is_hashable,
		BOOL is_merge_joinable, BOOL is_composite_type, BOOL is_text_related,
		IMDId *mdid_base_relation, IMDId *mdid_type_array, INT gpdb_length);

	// dtor
	virtual ~CMDTypeGenericGPDB();

	// accessors
	virtual const CWStringDynamic *
	GetStrRepr() const
	{
		return m_dxl_str;
	}

	virtual IMDId *MDId() const;

	virtual CMDName Mdname() const;

	virtual BOOL IsRedistributable() const;

	virtual BOOL
	IsFixedLength() const
	{
		return m_is_fixed_length;
	}

	// is type composite
	virtual BOOL
	IsComposite() const
	{
		return m_is_composite_type;
	}

	virtual ULONG
	Length() const
	{
		return m_gpdb_length;
	}

	virtual BOOL
	IsPassedByValue() const
	{
		return m_is_passed_by_value;
	}

	// id of specified comparison operator type
	virtual IMDId *GetMdidForCmpType(ECmpType ecmpt) const;

	// id of specified specified aggregate type
	virtual IMDId *GetMdidForAggType(EAggType agg_type) const;

	virtual const IMDId *
	CmpOpMdid() const
	{
		return m_mdid_op_cmp;
	}

	// is type hashable
	virtual BOOL
	IsHashable() const
	{
		return m_is_hashable;
	}

	virtual BOOL
	IsTextRelated() const
	{
		return m_is_text_related;
	}

	// is type merge joinable on '='
	virtual BOOL
	IsMergeJoinable() const
	{
		return m_is_merge_joinable;
	}

	// id of the relation corresponding to a composite type
	virtual IMDId *
	GetBaseRelMdid() const
	{
		return m_mdid_base_relation;
	}

	virtual IMDId *
	GetArrayTypeMdid() const
	{
		return m_mdid_type_array;
	}

	virtual IMDId *GetDistrOpfamilyMdid() const;

	// serialize object in DXL format
	virtual void Serialize(gpdxl::CXMLSerializer *xml_serializer) const;

	// factory method for generating generic datum from CDXLScalarConstValue
	virtual IDatum *GetDatumForDXLConstVal(
		const CDXLScalarConstValue *dxl_op) const;

	// create typed datum from DXL datum
	virtual IDatum *GetDatumForDXLDatum(CMemoryPool *mp,
										const CDXLDatum *dxl_datum) const;

	// return the GPDB length
	virtual INT
	GetGPDBLength() const
	{
		return m_gpdb_length;
	}

	// return the null constant for this type
	virtual IDatum *
	DatumNull() const
	{
		return m_datum_null;
	}

	// generate the DXL datum from IDatum
	virtual CDXLDatum *GetDatumVal(CMemoryPool *mp, IDatum *datum) const;

	// generate the DXL datum representing null value
	virtual CDXLDatum *GetDXLDatumNull(CMemoryPool *mp) const;

	// generate the DXL scalar constant from IDatum
	virtual CDXLScalarConstValue *GetDXLOpScConst(CMemoryPool *mp,
												  IDatum *datum) const;

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	virtual void DebugPrint(IOstream &os) const;
#endif

	// is type an ambiguous one? e.g., AnyElement in GPDB
	virtual BOOL IsAmbiguous() const;

	// create a dxl datum
	static CDXLDatum *CreateDXLDatumVal(CMemoryPool *mp, IMDId *mdid,
										const IMDType *md_type,
										INT type_modifier, BOOL is_null,
										BYTE *byte_array, ULONG length,
										LINT lint_Value, CDouble double_Value);

	// create a dxl datum of types having double mapping
	static CDXLDatum *CreateDXLDatumStatsDoubleMappable(
		CMemoryPool *mp, IMDId *mdid, INT type_modifier, BOOL is_null,
		BYTE *byte_array, ULONG length, LINT lint_Value, CDouble double_Value);

	// create a dxl datum of types having lint mapping
	static CDXLDatum *CreateDXLDatumStatsIntMappable(
		CMemoryPool *mp, IMDId *mdid, INT type_modifier, BOOL is_null,
		BYTE *byte_array, ULONG length, LINT lint_Value, CDouble double_Value);

	// does a datum of this type need bytea to Lint mapping for statistics computation
	static BOOL HasByte2IntMapping(const IMDType *mdtype);

	// does a datum of this type need bytea to double mapping for statistics computation
	static BOOL HasByte2DoubleMapping(const IMDId *mdid);

	// is this a time-related type
	static BOOL IsTimeRelatedType(const IMDId *mdid);

	// is this a network-related type
	static BOOL IsNetworkRelatedType(const IMDId *mdid);
};
}  // namespace gpmd

#endif	// !GPMD_CMDTypeGenericGPDB_H

// EOF
