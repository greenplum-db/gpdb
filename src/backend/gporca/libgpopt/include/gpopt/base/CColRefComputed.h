//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CColRefComputed.h
//
//	@doc:
//		Column reference implementation for computed columns
//---------------------------------------------------------------------------
#ifndef GPOS_CColRefComputed_H
#define GPOS_CColRefComputed_H

#include "gpos/base.h"
#include "gpos/common/CList.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/metadata/CName.h"

#include "naucrates/md/IMDType.h"

namespace gpopt
{
	using namespace gpos;
	using namespace gpmd;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CColRefComputed
	//
	//	@doc:
	//
	//---------------------------------------------------------------------------
	class CColRefComputed : public CColRef
	{
		private:

			// private copy ctor
			CColRefComputed(const CColRefComputed &);
			
		public:
		
			// ctor
			CColRefComputed
				(
				const IMDType *pmdtype,
				INT type_modifier,
				ULONG id, 
				const CName *pname
				);

			// dtor
			virtual ~CColRefComputed();
			
			virtual
			CColRef::Ecolreftype Ecrt() const
			{
				return CColRef::EcrtComputed;
			}

			// is column a system column?
			BOOL IsSystemCol() const
			{
				// we cannot introduce system columns as computed column
				return false;
			}

			// is column a distribution column?
			BOOL IsDistCol() const
			{
				// we cannot introduce distribution columns as computed column
				return false;
			};


	}; // class CColRefComputed

}

#endif // !GPOS_CColRefComputed_H

// EOF
