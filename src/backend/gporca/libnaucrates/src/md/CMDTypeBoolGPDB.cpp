//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDTypeBoolGPDB.cpp
//
//	@doc:
//		Implementation of the class for representing the GPDB bool types in the
//		MD cache
//---------------------------------------------------------------------------

#include "naucrates/md/CMDTypeBoolGPDB.h"

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/base/CDatumBoolGPDB.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLDatumBool.h"
#include "naucrates/dxl/operators/CDXLScalarConstValue.h"
#include "naucrates/md/CMDIdGPDB.h"

using namespace gpdxl;
using namespace gpmd;
using namespace gpnaucrates;

// static member initialization
CWStringConst CMDTypeBoolGPDB::m_str = CWStringConst(GPOS_WSZ_LIT("bool"));
CMDName CMDTypeBoolGPDB::m_mdname(&m_str);

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::CMDTypeBoolGPDB
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CMDTypeBoolGPDB::CMDTypeBoolGPDB(CMemoryPool *mp) : m_mp(mp)
{
	m_mdid = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_OID);
	if (GPOS_FTRACE(EopttraceConsiderOpfamiliesForDistribution))
	{
		m_distr_opfamily =
			GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_OPFAMILY);
		m_legacy_distr_opfamily = GPOS_NEW(mp)
			CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_LEGACY_OPFAMILY);
	}
	else
	{
		m_distr_opfamily = nullptr;
		m_legacy_distr_opfamily = nullptr;
	}
	m_mdid_op_eq = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_EQ_OP);
	m_mdid_op_neq =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_NEQ_OP);
	m_mdid_op_lt = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_LT_OP);
	m_mdid_op_leq =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_LEQ_OP);
	m_mdid_op_gt = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_GT_OP);
	m_mdid_op_geq =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_GEQ_OP);
	m_mdid_op_cmp =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_COMP_OP);
	m_mdid_type_array =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_ARRAY_TYPE);

	m_mdid_min = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_AGG_MIN);
	m_mdid_max = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_AGG_MAX);
	m_mdid_avg = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_AGG_AVG);
	m_mdid_sum = GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_AGG_SUM);
	m_mdid_count =
		GPOS_NEW(mp) CMDIdGPDB(IMDId::EmdidGeneral, GPDB_BOOL_AGG_COUNT);

	m_dxl_str = CDXLUtils::SerializeMDObj(
		m_mp, this, false /*fSerializeHeader*/, false /*indentation*/);

	m_mdid->AddRef();

	GPOS_ASSERT(GPDB_BOOL_OID == CMDIdGPDB::CastMdid(m_mdid)->Oid());
	m_datum_null = GPOS_NEW(mp)
		CDatumBoolGPDB(m_mdid, false /* value */, true /* is_null */);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::~CMDTypeBoolGPDB
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CMDTypeBoolGPDB::~CMDTypeBoolGPDB()
{
	m_mdid->Release();
	CRefCount::SafeRelease(m_distr_opfamily);
	CRefCount::SafeRelease(m_legacy_distr_opfamily);
	m_mdid_op_eq->Release();
	m_mdid_op_neq->Release();
	m_mdid_op_lt->Release();
	m_mdid_op_leq->Release();
	m_mdid_op_gt->Release();
	m_mdid_op_geq->Release();
	m_mdid_op_cmp->Release();
	m_mdid_type_array->Release();

	m_mdid_min->Release();
	m_mdid_max->Release();
	m_mdid_avg->Release();
	m_mdid_sum->Release();
	m_mdid_count->Release();
	m_datum_null->Release();
	GPOS_DELETE(m_dxl_str);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::GetMdidForCmpType
//
//	@doc:
//		Return mdid of specified comparison operator type
//
//---------------------------------------------------------------------------
IMDId *
CMDTypeBoolGPDB::GetMdidForCmpType(ECmpType cmp_type) const
{
	switch (cmp_type)
	{
		case EcmptEq:
			return m_mdid_op_eq;
		case EcmptNEq:
			return m_mdid_op_neq;
		case EcmptL:
			return m_mdid_op_lt;
		case EcmptLEq:
			return m_mdid_op_leq;
		case EcmptG:
			return m_mdid_op_gt;
		case EcmptGEq:
			return m_mdid_op_geq;
		default:
			GPOS_ASSERT(!"Invalid operator type");
			return nullptr;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::GetMdidForAggType
//
//	@doc:
//		Return mdid of specified aggregate type
//
//---------------------------------------------------------------------------
IMDId *
CMDTypeBoolGPDB::GetMdidForAggType(EAggType agg_type) const
{
	switch (agg_type)
	{
		case EaggMin:
			return m_mdid_min;
		case EaggMax:
			return m_mdid_max;
		case EaggAvg:
			return m_mdid_avg;
		case EaggSum:
			return m_mdid_sum;
		case EaggCount:
			return m_mdid_count;
		default:
			GPOS_ASSERT(!"Invalid aggregate type");
			return nullptr;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::GetDatum
//
//	@doc:
//		Factory function for creating BOOL datums
//
//---------------------------------------------------------------------------
IDatumBool *
CMDTypeBoolGPDB::CreateBoolDatum(CMemoryPool *mp, BOOL bool_val,
								 BOOL is_null) const
{
	return GPOS_NEW(mp) CDatumBoolGPDB(m_mdid->Sysid(), bool_val, is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::MDId
//
//	@doc:
//		Returns the metadata id of this type
//
//---------------------------------------------------------------------------
IMDId *
CMDTypeBoolGPDB::MDId() const
{
	return m_mdid;
}

IMDId *
CMDTypeBoolGPDB::GetDistrOpfamilyMdid() const
{
	if (GPOS_FTRACE(EopttraceUseLegacyOpfamilies))
	{
		return m_legacy_distr_opfamily;
	}
	else
	{
		return m_distr_opfamily;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::Mdname
//
//	@doc:
//		Returns the name of this type
//
//---------------------------------------------------------------------------
CMDName
CMDTypeBoolGPDB::Mdname() const
{
	return CMDTypeBoolGPDB::m_mdname;
	;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::Serialize
//
//	@doc:
//		Serialize relation metadata in DXL format
//
//---------------------------------------------------------------------------
void
CMDTypeBoolGPDB::Serialize(CXMLSerializer *xml_serializer) const
{
	CGPDBTypeHelper<CMDTypeBoolGPDB>::Serialize(xml_serializer, this);
}


//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::GetDatumForDXLConstVal
//
//	@doc:
//		Transformation function to generate bool datum from CDXLScalarConstValue
//
//---------------------------------------------------------------------------
IDatum *
CMDTypeBoolGPDB::GetDatumForDXLConstVal(
	const CDXLScalarConstValue *dxl_op) const
{
	CDXLDatumBool *dxl_datum =
		CDXLDatumBool::Cast(const_cast<CDXLDatum *>(dxl_op->GetDatumVal()));

	return GPOS_NEW(m_mp) CDatumBoolGPDB(m_mdid->Sysid(), dxl_datum->GetValue(),
										 dxl_datum->IsNull());
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::GetDatumForDXLDatum
//
//	@doc:
//		Construct a bool datum from a DXL datum
//
//---------------------------------------------------------------------------
IDatum *
CMDTypeBoolGPDB::GetDatumForDXLDatum(CMemoryPool *mp,
									 const CDXLDatum *dxl_datum) const
{
	CDXLDatumBool *dxl_datum_bool =
		CDXLDatumBool::Cast(const_cast<CDXLDatum *>(dxl_datum));
	BOOL value = dxl_datum_bool->GetValue();
	BOOL is_null = dxl_datum_bool->IsNull();

	return GPOS_NEW(mp) CDatumBoolGPDB(m_mdid->Sysid(), value, is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::GetDatumVal
//
//	@doc:
// 		Generate dxl datum
//
//---------------------------------------------------------------------------
CDXLDatum *
CMDTypeBoolGPDB::GetDatumVal(CMemoryPool *mp, IDatum *datum) const
{
	CDatumBoolGPDB *datum_bool = dynamic_cast<CDatumBoolGPDB *>(datum);
	m_mdid->AddRef();
	return GPOS_NEW(mp)
		CDXLDatumBool(mp, m_mdid, datum_bool->IsNull(), datum_bool->GetValue());
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::GetDXLOpScConst
//
//	@doc:
// 		Generate a dxl scalar constant from a datum
//
//---------------------------------------------------------------------------
CDXLScalarConstValue *
CMDTypeBoolGPDB::GetDXLOpScConst(CMemoryPool *mp, IDatum *datum) const
{
	CDatumBoolGPDB *datum_bool_gpdb = dynamic_cast<CDatumBoolGPDB *>(datum);

	m_mdid->AddRef();
	CDXLDatumBool *dxl_datum = GPOS_NEW(mp) CDXLDatumBool(
		mp, m_mdid, datum_bool_gpdb->IsNull(), datum_bool_gpdb->GetValue());

	return GPOS_NEW(mp) CDXLScalarConstValue(mp, dxl_datum);
}

//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::GetDXLDatumNull
//
//	@doc:
// 		Generate dxl datum representing a null value
//
//---------------------------------------------------------------------------
CDXLDatum *
CMDTypeBoolGPDB::GetDXLDatumNull(CMemoryPool *mp) const
{
	m_mdid->AddRef();

	return GPOS_NEW(mp) CDXLDatumBool(mp, m_mdid, true /*is_null*/, false);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CMDTypeBoolGPDB::DebugPrint
//
//	@doc:
//		Prints a metadata cache relation to the provided output
//
//---------------------------------------------------------------------------
void
CMDTypeBoolGPDB::DebugPrint(IOstream &os) const
{
	CGPDBTypeHelper<CMDTypeBoolGPDB>::DebugPrint(os, this);
}

#endif	// GPOS_DEBUG

// EOF
