//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CMappingParamIdScalarId.h
//
//	@doc:
//		ParamId to ScalarId mapping during PlStmt->DXL translation
//
//	@test:
//
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CMappingParamIdScalarId_H
#define GPDXL_CMappingParamIdScalarId_H

#include "gpos/base.h"
#include "gpos/common/CHashMap.h"
#include "nodes/primnodes.h"
#include "naucrates/dxl/operators/CDXLScalarIdent.h"

namespace gpdxl
{

	using namespace gpos;

	// hash maps mapping ULONG -> ScalarId
	typedef CHashMap<ULONG, CDXLScalarIdent, gpos::UlHash<ULONG>, gpos::FEqual<ULONG>,
			CleanupDelete<ULONG>, CleanupRelease<CDXLScalarIdent> > HMParamScalar;

	//---------------------------------------------------------------------------
	//	@class:
	//		CMappingParamIdScalarId
	//
	//	@doc:
	//		ParamId to ScalarId mapping during scalar operator translation during
	//		PlStmt->DXL translation
	//
	//---------------------------------------------------------------------------
	class CMappingParamIdScalarId
	{
		private:
			// memory pool
			IMemoryPool *m_pmp;

			// map from param id to scalar id
			HMParamScalar *m_phmps;

			// private copy constructor
			CMappingParamIdScalarId(const CMappingParamIdScalarId &);

		public:

			// ctor
			explicit
			CMappingParamIdScalarId(IMemoryPool *);

			// dtor
			virtual
			~CMappingParamIdScalarId()
			{
				m_phmps->Release();
			}

			// return the scalarident corresponding to the given ParamId
			CDXLScalarIdent *Pscid(ULONG ulParmId) const;

			// store the mapping of the given param id and scalar id
			BOOL FInsertMapping(ULONG ulParamId, CDXLScalarIdent *pstrid);
	};
}

#endif //GPDXL_CMappingParamIdScalarId_H

// EOF
