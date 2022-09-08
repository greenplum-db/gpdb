//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMDTypeOidGPDB.h
//
//	@doc:
//		Class for representing OID types in GPDB
//---------------------------------------------------------------------------

#ifndef GPMD_CMDTypeOidGPDB_H
#define GPMD_CMDTypeOidGPDB_H

#include "gpos/base.h"

#include "naucrates/md/CGPDBTypeHelper.h"
#include "naucrates/md/IMDTypeOid.h"

#define GPDB_OID_OID OID(26)
#define GPDB_OID_OPFAMILY OID(1990)
#define GPDB_OID_LEGACY_OPFAMILY OID(7109)
#define GPDB_OID_LENGTH 4
#define GPDB_OID_EQ_OP OID(607)
#define GPDB_OID_NEQ_OP OID(608)
#define GPDB_OID_LT_OP OID(609)
#define GPDB_OID_LEQ_OP OID(611)
#define GPDB_OID_GT_OP OID(610)
#define GPDB_OID_GEQ_OP OID(612)
#define GPDB_OID_EQ_FUNC OID(184)
#define GPDB_OID_ARRAY_TYPE OID(1028)
#define GPDB_OID_COMP_OP OID(356)
#define GPDB_OID_HASH_OP OID(0)

#define GPDB_OID_AGG_MIN OID(2134)
#define GPDB_OID_AGG_MAX OID(2118)
#define GPDB_OID_AGG_AVG OID(0)
#define GPDB_OID_AGG_SUM OID(0)
#define GPDB_OID_AGG_COUNT OID(2147)

// fwd decl
namespace gpdxl
{
class CXMLSerializer;
}

namespace gpnaucrates
{
class IDatumOid;
}

namespace gpmd
{
using namespace gpos;
using namespace gpnaucrates;

//---------------------------------------------------------------------------
//	@class:
//		CMDTypeOidGPDB
//
//	@doc:
//		Class for representing OID types in GPDB
//
//---------------------------------------------------------------------------
class CMDTypeOidGPDB : public IMDTypeOid
{
	friend class CGPDBTypeHelper<CMDTypeOidGPDB>;

private:
	// memory pool
	CMemoryPool *m_mp;

	// type id
	IMDId *m_mdid;
	IMDId *m_distr_opfamily;
	IMDId *m_legacy_distr_opfamily;

	// mdids of different comparison operators
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
	CMDTypeOidGPDB(const CMDTypeOidGPDB &);

public:
	// ctor/dtor
	explicit CMDTypeOidGPDB(CMemoryPool *mp);

	virtual ~CMDTypeOidGPDB();

	// factory method for creating OID datums
	virtual IDatumOid *CreateOidDatum(CMemoryPool *mp, OID oValue,
									  BOOL is_null) const;

	// accessors
	virtual const CWStringDynamic *
	GetStrRepr() const
	{
		return m_dxl_str;
	}

	virtual IMDId *MDId() const;

	IMDId *GetDistrOpfamilyMdid() const;

	virtual CMDName Mdname() const;

	// id of specified comparison operator type
	virtual IMDId *GetMdidForCmpType(ECmpType cmp_type) const;

	// id of specified specified aggregate type
	virtual IMDId *GetMdidForAggType(EAggType agg_type) const;

	virtual BOOL
	IsRedistributable() const
	{
		return true;
	}

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

	virtual ULONG
	Length() const
	{
		return GPDB_OID_LENGTH;
	}

	virtual BOOL
	IsPassedByValue() const
	{
		return true;
	}

	// return the GPDB length
	virtual INT
	GetGPDBLength() const
	{
		return GPDB_OID_LENGTH;
	}

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

#endif	// !GPMD_CMDTypeOidGPDB_H

// EOF
