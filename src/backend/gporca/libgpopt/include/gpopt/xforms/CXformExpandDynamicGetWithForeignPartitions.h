//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2023 VMware Inc.
//
//	@filename:
//		CXformExpandDynamicGetWithForeignPartitions.h
//
//	@doc:
//  	Transform DynamicGet to a UNION ALL of a DynamicGet without Foreign
//  	partitions and a DynamicForeignGet that encapsulates all the foreign
//  	partitions.
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformExpandDynamicGetWithForeignPartitions_H
#define GPOPT_CXformExpandDynamicGetWithForeignPartitions_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalDynamicGet.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

struct SForeignServer
{
	ULONG m_foreign_server_oid;

	BOOL m_is_master_only;

	SForeignServer(ULONG foreign_server_oid, BOOL is_master_only)
		: m_foreign_server_oid(foreign_server_oid),
		  m_is_master_only(is_master_only)
	{
	}
};

static ULONG
UlHashSForeignServer(const SForeignServer *fs)
{
	return fs->m_foreign_server_oid;
}

static BOOL
FEqualSForeignServer(const SForeignServer *fs1, const SForeignServer *fs2)
{
	return fs1->m_foreign_server_oid == fs2->m_foreign_server_oid &&
		   fs1->m_is_master_only == fs2->m_is_master_only;
}

// hash maps ULONG -> array of ULONGs
using SForeignServerToIMdIdArrayMap =
	CHashMap<SForeignServer, IMdIdArray, UlHashSForeignServer,
			 FEqualSForeignServer, CleanupDelete<SForeignServer>,
			 CleanupRelease<IMdIdArray>>;

// iterator
using SForeignServerToIMdIdArrayMapIter =
	CHashMapIter<SForeignServer, IMdIdArray, UlHashSForeignServer,
				 FEqualSForeignServer, CleanupDelete<SForeignServer>,
				 CleanupRelease<IMdIdArray>>;

//---------------------------------------------------------------------------
//	@class:
//		CXformExpandDynamicGetWithForeignPartitions
//
//	@doc:
//  	Transform DynamicGet to a UNION ALL of a DynamicGet without Foreign
//  	partitions and a DynamicForeignGet that encapsulates all the foreign
//  	partitions.
//---------------------------------------------------------------------------
class CXformExpandDynamicGetWithForeignPartitions : public CXformExploration
{
private:
public:
	CXformExpandDynamicGetWithForeignPartitions(
		const CXformExpandDynamicGetWithForeignPartitions &) = delete;

	// ctor
	explicit CXformExpandDynamicGetWithForeignPartitions(CMemoryPool *);

	// dtor
	~CXformExpandDynamicGetWithForeignPartitions() override = default;

	// ident accessors
	EXformId
	Exfid() const override
	{
		return ExfExpandDynamicGetWithForeignPartitions;
	}

	// return a string for xform name
	const CHAR *
	SzId() const override
	{
		return "CXformExpandDynamicGetWithForeignPartitions";
	}

	// compute xform promise for a given expression handle
	EXformPromise Exfp(CExpressionHandle &exprhdl) const override;

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const override;

};	// class CXformExpandDynamicGetWithForeignPartitions

}  // namespace gpopt


#endif	// !GPOPT_CXformExpandDynamicGetWithForeignPartitions_H

// EOF
