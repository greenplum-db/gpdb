//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		IMDFunction.h
//
//	@doc:
//		Interface for functions in the metadata cache
//---------------------------------------------------------------------------



#ifndef GPMD_IMDFunction_H
#define GPMD_IMDFunction_H

#include "gpos/base.h"

#include "naucrates/md/IMDCacheObject.h"

namespace gpmd
{
	using namespace gpos;

	
	//---------------------------------------------------------------------------
	//	@class:
	//		IMDFunction
	//
	//	@doc:
	//		Interface for functions in the metadata cache
	//
	//---------------------------------------------------------------------------
	class IMDFunction : public IMDCacheObject
	{	
		public:

			// function stability property
			enum EFuncStbl
			{
				EfsImmutable, /* never changes for given input */
				EfsStable, /* does not change within a scan */
				EfsVolatile, /* can change even within a scan */
				EfsSentinel
			};

			// function data access property
			enum EFuncDataAcc
			{
				EfdaNoSQL,
				EfdaContainsSQL,
				EfdaReadsSQLData,
				EfdaModifiesSQLData,
				EfdaSentinel
			};

			// object type
			virtual
			Emdtype MDType() const
			{
				return EmdtFunc;
			}

			// does function return NULL on NULL input
			virtual 
			BOOL IsStrict() const = 0;
			
			// does function preserve NDVs of input (for cardinality estimation)
			virtual
			BOOL IsNDVPreserving() const = 0;

			// does function return a set of values
			virtual 
			BOOL ReturnsSet() const = 0;

			// is this function allowed for Partition selection
			virtual
			BOOL IsAllowedForPS() const = 0;

			// function stability
			virtual
			EFuncStbl GetFuncStability() const = 0;

			// function data access
			virtual
			EFuncDataAcc GetFuncDataAccess() const = 0;

			// result type
			virtual 
			IMDId *GetResultTypeMdid() const = 0;
			
			// output argument types
			virtual
			IMdIdArray *OutputArgTypesMdidArray() const = 0;
	};
		
}

#endif // !GPMD_IMDFunction_H

// EOF
