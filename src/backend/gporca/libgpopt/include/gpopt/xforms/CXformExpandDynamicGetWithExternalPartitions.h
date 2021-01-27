//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformExpandDynamicGetWithExternalPartitions.h
//
//	@doc:
//  	Transform DynamicGet to a UNION ALL of a DynamicGet without External
//  	partitions and a MultiExternalGet that encapsulates all the external
//  	partitions.
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformExpandDynamicGetWithExternalPartitions_H
#define GPOPT_CXformExpandDynamicGetWithExternalPartitions_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalDynamicGet.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
using namespace gpos;

//---------------------------------------------------------------------------
//	@class:
//		CXformExpandDynamicGetWithExternalPartitions
//
//	@doc:
//  	Transform DynamicGet to a UNION ALL of a DynamicGet without External
//  	partitions and a MultiExternalGet that encapsulates all the external
//  	partitions.
//---------------------------------------------------------------------------
class CXformExpandDynamicGetWithExternalPartitions : public CXformExploration
{
private:
	// private copy ctor
	CXformExpandDynamicGetWithExternalPartitions(
		const CXformExpandDynamicGetWithExternalPartitions &);

public:
	// ctor
	explicit CXformExpandDynamicGetWithExternalPartitions(CMemoryPool *mp);

	// dtor
	virtual ~CXformExpandDynamicGetWithExternalPartitions()
	{
	}

	// ident accessors
	virtual EXformId
	Exfid() const
	{
		return ExfExpandDynamicGetWithExternalPartitions;
	}

	// return a string for xform name
	virtual const CHAR *
	SzId() const
	{
		return "CXformExpandDynamicGetWithExternalPartitions";
	}

	// compute xform promise for a given expression handle
	virtual EXformPromise
	Exfp(CExpressionHandle &exprhdl) const
	{
		CLogicalDynamicGet *popGet =
			CLogicalDynamicGet::PopConvert(exprhdl.Pop());
		CTableDescriptor *ptabdesc = popGet->Ptabdesc();
		CMDAccessor *mda = COptCtxt::PoctxtFromTLS()->Pmda();

		const IMDRelation *relation = mda->RetrieveRel(ptabdesc->MDId());
		if (relation->HasExternalPartitions())
		{
			if (popGet->IsPartial())
			{
				// Prevent unneccesary re-execution of this xform once a
				// DynamicGet has already been split. In such a case, a
				// Partitial DynamicGet is produced, and any indexes or external
				// partitions are handled separately.
				return CXform::ExfpNone;
			}

			// Run the xform only on a non-partial DynamicGet with external
			// partitions
			return CXform::ExfpHigh;
		}

		// No need to run this xform if the relation being scanned does not
		// contain external partitions
		return CXform::ExfpNone;
	}

	// actual transform
	void Transform(CXformContext *pxfctxt, CXformResult *pxfres,
				   CExpression *pexpr) const;

};	// class CXformExpandDynamicGetWithExternalPartitions

}  // namespace gpopt


#endif	// !GPOPT_CXformExpandDynamicGetWithExternalPartitions_H

// EOF
