//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CMDTypeInt2GPDB.h
//
//	@doc:
//		Class for representing INT2 types in GPDB
//---------------------------------------------------------------------------



#ifndef GPMD_CMDTypeInt2GPDB_H
#define GPMD_CMDTypeInt2GPDB_H

#include "gpos/base.h"

#include "naucrates/md/IMDTypeInt2.h"
#include "naucrates/md/CGPDBTypeHelper.h"

#define GPDB_INT2_OID OID(21)
#define GPDB_INT2_OPFAMILY OID(1977)
#define GPDB_INT2_LEGACY_OPFAMILY OID(7100)
#define GPDB_INT2_LENGTH 2
#define GPDB_INT2_EQ_OP OID(94)
#define GPDB_INT2_NEQ_OP OID(519)
#define GPDB_INT2_LT_OP OID(95)
#define GPDB_INT2_LEQ_OP OID(522)
#define GPDB_INT2_GT_OP OID(520)
#define GPDB_INT2_GEQ_OP OID(524)
#define GPDB_INT2_COMP_OP OID(350)
#define GPDB_INT2_EQ_FUNC OID(63)
#define GPDB_INT2_ARRAY_TYPE OID(1005)

#define GPDB_INT2_AGG_MIN OID(2133)
#define GPDB_INT2_AGG_MAX OID(2117)
#define GPDB_INT2_AGG_AVG OID(2102)
#define GPDB_INT2_AGG_SUM OID(2109)
#define GPDB_INT2_AGG_COUNT OID(2147)

// fwd decl
namespace gpdxl
{
class CXMLSerializer;
}

namespace gpnaucrates
{
class IDatumInt2;
}

namespace gpmd
{
using namespace gpos;
using namespace gpnaucrates;

//---------------------------------------------------------------------------
//	@class:
//		CMDTypeInt2GPDB
//
//	@doc:
//		Class for representing INT2 type in GPDB
//
//---------------------------------------------------------------------------
class CMDTypeInt2GPDB : public IMDTypeInt2
{
	friend class CGPDBTypeHelper<CMDTypeInt2GPDB>;

private:
	// memory pool
	CMemoryPool *m_mp;

	// type id
	IMDId *m_mdid;
	IMDId *m_distr_opfamily;
	IMDId *m_legacy_distr_opfamily;

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
	const CWStringDynamic *m_dxl_str;

	// type name and type
	static CWStringConst m_str;
	static CMDName m_mdname;

	// a null datum of this type (used for statistics comparison)
	IDatum *m_datum_null;

	// private copy ctor
	CMDTypeInt2GPDB(const CMDTypeInt2GPDB &);

public:
	// ctor
	explicit CMDTypeInt2GPDB(CMemoryPool *mp);

	// dtor
	virtual ~CMDTypeInt2GPDB();

	// factory method for creating INT2 datums
	virtual IDatumInt2 *CreateInt2Datum(CMemoryPool *mp, SINT value,
										BOOL is_null) const;

	// accessors
	virtual const CWStringDynamic *
	GetStrRepr() const
	{
		return m_dxl_str;
	}

	// accessor of metadata id
	virtual IMDId *MDId() const;

	virtual IMDId *GetDistrOpfamilyMdid() const;

	// accessor of type name
	virtual CMDName Mdname() const;

	// id of specified comparison operator type
	virtual IMDId *GetMdidForCmpType(ECmpType cmp_type) const;

	// id of specified aggregate type
	virtual IMDId *GetMdidForAggType(EAggType agg_type) const;

	// is type redistributable
	virtual BOOL
	IsRedistributable() const
	{
		return true;
	}

	// is type has fixed length
	virtual BOOL
	IsFixedLength() const
	{
		return true;
	}

	// is type composite
	virtual BOOL
	IsComposite() const
	{
		return false;
	}

	// size of type
	virtual ULONG
	Length() const
	{
		return GPDB_INT2_LENGTH;
	}

	// return the GPDB length
	virtual INT
	GetGPDBLength() const
	{
		return GPDB_INT2_LENGTH;
	}

	// is type passed by value
	virtual BOOL
	IsPassedByValue() const
	{
		return true;
	}

	// metadata id of b-tree lookup operator
	virtual const IMDId *
	CmpOpMdid() const
	{
		return m_mdid_op_cmp;
	}

	// is type hashable
	virtual BOOL
	IsHashable() const
	{
		return true;
	}

	// is type merge joinable
	virtual BOOL
	IsMergeJoinable() const
	{
		return true;
	}

	// metadata id of array type
	virtual IMDId *
	GetArrayTypeMdid() const
	{
		return m_mdid_type_array;
	}

	// id of the relation corresponding to a composite type
	virtual IMDId *
	GetBaseRelMdid() const
	{
		return NULL;
	}

	// serialize object in DXL format
	virtual void Serialize(gpdxl::CXMLSerializer *xml_serializer) const;

	// return the null constant for this type
	virtual IDatum *
	DatumNull() const
	{
		return m_datum_null;
	}

	// transformation method for generating datum from CDXLScalarConstValue
	virtual IDatum *GetDatumForDXLConstVal(
		const CDXLScalarConstValue *dxl_op) const;

	// create typed datum from DXL datum
	virtual IDatum *GetDatumForDXLDatum(CMemoryPool *mp,
										const CDXLDatum *dxl_datum) const;

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
};
}  // namespace gpmd

#endif	// !GPMD_CMDTypeInt2GPDB_H

// EOF
