//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2020 VMware, Inc.
//
//	@filename:
//		CXformIndexGet2IndexOnlyScan.h
//
//	@doc:
//		Transform Index Get to Index Only Scan
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformIndexGet2IndexOnlyScan_H
#define GPOPT_CXformIndexGet2IndexOnlyScan_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CXformIndexGet2IndexOnlyScan
	//
	//	@doc:
	//		Transform Index Get to Index Scan
	//
	//---------------------------------------------------------------------------
	class CXformIndexGet2IndexOnlyScan : public CXformImplementation
	{

		private:

			// private copy ctor
			CXformIndexGet2IndexOnlyScan(const CXformIndexGet2IndexOnlyScan &);

		public:

			// ctor
			explicit
			CXformIndexGet2IndexOnlyScan(CMemoryPool *);

			// dtor
			virtual
			~CXformIndexGet2IndexOnlyScan() {}

			// ident accessors
			virtual
			EXformId Exfid() const
			{
				return ExfIndexGet2IndexOnlyScan;
			}

			// xform name
			virtual
			const CHAR *SzId() const
			{
				return "CXformIndexGet2IndexOnlyScan";
			}

			// compute xform promise for a given expression handle
			virtual
			EXformPromise Exfp
				(
				CExpressionHandle &//exprhdl
				)
				const;

			// actual transform
			void Transform
				(
				CXformContext *pxfctxt,
				CXformResult *pxfres,
				CExpression *pexpr
				)
				const;

	}; // class CXformIndexGet2IndexOnlyScan

}

#endif // !GPOPT_CXformIndexGet2IndexOnlyScan_H

// EOF
