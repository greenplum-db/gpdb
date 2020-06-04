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

#include "naucrates/md/IMDFunction.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

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
			const CWStringDynamic *m_dxl_str;
			
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
			
			// function data access
			EFuncDataAcc m_func_data_access;

			// function strictness (i.e. whether func returns NULL on NULL input)
			BOOL m_is_strict;

			// function result has very similar number of distinct values as the
			// single function argument (used for cardinality estimation)
			BOOL m_is_ndv_preserving;

			// dxl token array for stability
			Edxltoken m_dxl_func_stability_array[EfsSentinel];

			// dxl token array for data access
			Edxltoken m_dxl_data_access_array[EfdaSentinel];
			
			// returns the string representation of the function stability
			const CWStringConst *GetFuncStabilityStr() const;

			// returns the string representation of the function data access
			const CWStringConst *GetFuncDataAccessStr() const;

			// serialize the array of output arg types into a comma-separated string
			CWStringDynamic *GetOutputArgTypeArrayStr() const;

			// initialize dxl token arrays
			void InitDXLTokenArrays();

			// private copy ctor
			CMDFunctionGPDB(const CMDFunctionGPDB &);
			
		public:
			// ctor/dtor
			CMDFunctionGPDB
				(
				CMemoryPool *mp,
				IMDId *mdid,
				CMDName *mdname,
				IMDId *result_type_mdid,
						IMdIdArray *mdid_array,
				BOOL ReturnsSet,
				EFuncStbl func_stability,
				EFuncDataAcc func_data_access,
				BOOL is_strict,
				BOOL is_ndv_preserving
				);
			
			virtual
			~CMDFunctionGPDB();
			
			// accessors
			virtual 
			const CWStringDynamic *GetStrRepr() const
			{
				return m_dxl_str;
			}
			
			// function id
			virtual 
			IMDId *MDId() const;
			
			// function name
			virtual 
			CMDName Mdname() const;
			
			// result type
			virtual 
			IMDId *GetResultTypeMdid() const;

			// output argument types
			virtual
			IMdIdArray *OutputArgTypesMdidArray() const;

			// does function return NULL on NULL input
			virtual 
			BOOL IsStrict() const
			{
				return m_is_strict;
			}
			
			virtual
			BOOL IsNDVPreserving() const
			{
				return m_is_ndv_preserving;
			}

			// function stability
			virtual
			EFuncStbl GetFuncStability() const
			{
				return m_func_stability;
			}

			// function data access
			virtual
			EFuncDataAcc GetFuncDataAccess() const
			{
				return m_func_data_access;
			}

			// does function return a set of values
			virtual 
			BOOL ReturnsSet() const;
			
			// serialize object in DXL format
			virtual 
			void Serialize(gpdxl::CXMLSerializer *xml_serializer) const;
			
#ifdef GPOS_DEBUG
			// debug print of the type in the provided stream
			virtual 
			void DebugPrint(IOstream &os) const;
#endif
	};
}

#endif // !GPMD_CMDFunctionGPDB_H

// EOF
