//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDScalarOpGPDB.h
//
//	@doc:
//		Class for representing GPDB-specific scalar operators in the MD cache
//---------------------------------------------------------------------------



#ifndef GPMD_CMDScalarOpGPDB_H
#define GPMD_CMDScalarOpGPDB_H

#include "gpos/base.h"

#include "naucrates/md/IMDScalarOp.h"

namespace gpmd
{
	using namespace gpos;

	
	//---------------------------------------------------------------------------
	//	@class:
	//		CMDScalarOpGPDB
	//
	//	@doc:
	//		Class for representing GPDB-specific scalar operators in the MD cache
	//
	//---------------------------------------------------------------------------
	class CMDScalarOpGPDB : public IMDScalarOp
	{		
		
		private:
		
			// memory pool
			CMemoryPool *m_mp;
			
			// DXL for object
			const CWStringDynamic *m_dxl_str;
			
			// operator id
			IMDId *m_mdid;
			
			// operator name
			CMDName *m_mdname;
			
			// type of left operand
			IMDId *m_mdid_type_left;
			
			// type of right operand
			IMDId *m_mdid_type_right;

			// type of result operand
			IMDId *m_mdid_type_result;
			
			// id of function which implements the operator
			IMDId *m_func_mdid;
			
			// id of commute operator
			IMDId *m_mdid_commute_opr;
			
			// id of inverse operator
			IMDId *m_mdid_inverse_opr;
			
			// comparison type for comparison operators
			IMDType::ECmpType m_comparision_type;
			
			// does operator return NULL when all inputs are NULL?
			BOOL m_returns_null_on_null_input;

			// operator classes this operator belongs to
			IMdIdArray *m_mdid_opfamilies_array;

			// compatible hash op family
			IMDId *m_mdid_hash_opfamily;

			// compatible legacy hash op family using legacy (cdbhash) opclass
			IMDId *m_mdid_legacy_hash_opfamily;

			// does operator preserve the NDV of its input(s)
			// (used for cardinality estimation)
			BOOL m_is_ndv_preserving;

			CMDScalarOpGPDB(const CMDScalarOpGPDB &);
			
		public:
			
			// ctor/dtor
			CMDScalarOpGPDB
				(
				CMemoryPool *mp,
				IMDId *mdid,
				CMDName *mdname,
				IMDId *mdid_type_left,
				IMDId *mdid_type_right,
				IMDId *result_type_mdid,
				IMDId *mdid_func,
				IMDId *mdid_commute_opr,
				IMDId *m_mdid_inverse_opr,
				IMDType::ECmpType cmp_type,
				BOOL returns_null_on_null_input,
				IMdIdArray *mdid_opfamilies_array,
				IMDId *m_mdid_hash_opfamily,
				IMDId *mdid_legacy_hash_opfamily,
				BOOL is_ndv_preserving
				);
			
			~CMDScalarOpGPDB();
			
			// accessors
			virtual 
			const CWStringDynamic *GetStrRepr() const
			{
				return m_dxl_str;
			}
			
			// operator id
			virtual
			IMDId *MDId() const;
			
			// operator name
			virtual
			CMDName Mdname() const;

			// left operand type id
			virtual
			IMDId *GetLeftMdid() const;
			
			// right operand type id
			virtual
			IMDId *GetRightMdid() const;

			// resulttype id
			virtual
			IMDId *GetResultTypeMdid() const;

			// implementer function id
			virtual
			IMDId *FuncMdId() const;

			// commutor id
			virtual
			IMDId *GetCommuteOpMdid() const;

			// inverse operator id
			virtual
			IMDId *GetInverseOpMdid() const;

			// is this an equality operator
			virtual
			BOOL IsEqualityOp() const;

			// does operator return NULL when all inputs are NULL?
			// STRICT implies NULL-returning, but the opposite is not always true,
			// the implementation in GPDB returns what STRICT property states
			virtual
			BOOL ReturnsNullOnNullInput() const;

			// preserves NDVs of its inputs?
			virtual
			BOOL IsNDVPreserving() const;

			// comparison type
			virtual
			IMDType::ECmpType ParseCmpType() const;
			
			// serialize object in DXL format
			virtual
			void Serialize(gpdxl::CXMLSerializer *xml_serializer) const;
			
			// number of classes this operator belongs to
			virtual
			ULONG OpfamiliesCount() const;
			
			// operator class at given position
			virtual
			IMDId *OpfamilyMdidAt(ULONG pos) const;

			// compatible hash opfamily
			virtual
			IMDId *HashOpfamilyMdid() const;
			
#ifdef GPOS_DEBUG
			// debug print of the type in the provided stream
			virtual
			void DebugPrint(IOstream &os) const;
#endif
			
	};
}

#endif // !GPMD_CMDScalarOpGPDB_H

// EOF
