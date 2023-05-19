//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDFunctionGPDB.h
//
//	@doc:
//		Implementation of GPDB-specific functions in the metadata cache
//---------------------------------------------------------------------------


#ifndef GPMD_CMDFunctionGPDB_H
#define GPMD_CMDFunctionGPDB_H

#include "gpos/base.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/md/IMDFunction.h"

namespace gpmd
{
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@class:
//		CMDFunctionGPDB
//
//	@doc:
//		Implementation for GPDB-specific functions in the metadata cache
//
//---------------------------------------------------------------------------
class CMDFunctionGPDB : public IMDFunction
{
private:
	// memory pool
	CMemoryPool *m_mp;

	// DXL for object
	const CWStringDynamic *m_dxl_str = nullptr;

	// func id
	IMDId *m_mdid;

	// func name
	CMDName *m_mdname;

	// result type
	IMDId *m_mdid_type_result;

	// output argument types
	IMdIdArray *m_mdid_types_array;

	// whether function returns a set of values
	BOOL m_returns_set;

	// function stability
	EFuncStbl m_func_stability;

	// function strictness (i.e. whether func returns NULL on NULL input)
	BOOL m_is_strict;

	// function result has very similar number of distinct values as the
	// single function argument (used for cardinality estimation)
	BOOL m_is_ndv_preserving;

	// is an increasing function (non-implicit/lossy cast) which can be used for partition selection
	BOOL m_is_allowed_for_PS;

	// dxl token array for stability
	Edxltoken m_dxl_func_stability_array[EfsSentinel];

	// returns the string representation of the function stability
	const CWStringConst *GetFuncStabilityStr() const;

	// serialize the array of output arg types into a comma-separated string
	CWStringDynamic *GetOutputArgTypeArrayStr() const;

	// initialize dxl token arrays
	void InitDXLTokenArrays();

public:
	CMDFunctionGPDB(const CMDFunctionGPDB &) = delete;

	// ctor/dtor
	CMDFunctionGPDB(CMemoryPool *mp, IMDId *mdid, CMDName *mdname,
					IMDId *result_type_mdid, IMdIdArray *mdid_array,
					BOOL ReturnsSet, EFuncStbl func_stability, BOOL is_strict,
					BOOL is_ndv_preserving, BOOL is_allowed_for_PS);

	~CMDFunctionGPDB() override;

	// accessors
	const CWStringDynamic *GetStrRepr() override;

	// function id
	IMDId *MDId() const override;

	// function name
	CMDName Mdname() const override;

	// result type
	IMDId *GetResultTypeMdid() const override;

	// output argument types
	IMdIdArray *OutputArgTypesMdidArray() const override;

	// does function return NULL on NULL input
	BOOL
	IsStrict() const override
	{
		return m_is_strict;
	}

	BOOL
	IsNDVPreserving() const override
	{
		return m_is_ndv_preserving;
	}

	// is this function a lossy cast allowed for Partition selection
	BOOL
	IsAllowedForPS() const override
	{
		return m_is_allowed_for_PS;
	}

	// function stability
	EFuncStbl
	GetFuncStability() const override
	{
		return m_func_stability;
	}

	// does function return a set of values
	BOOL ReturnsSet() const override;

	// serialize object in DXL format
	void Serialize(gpdxl::CXMLSerializer *xml_serializer) const override;

#ifdef GPOS_DEBUG
	// debug print of the type in the provided stream
	void DebugPrint(IOstream &os) const override;
#endif
};
}  // namespace gpmd

#endif	// !GPMD_CMDFunctionGPDB_H

// EOF
