//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CDXLOperatorCost.h
//
//	@doc:
//		Representation of physical operator costs
//---------------------------------------------------------------------------
#ifndef GPDXL_CDXLOperatorCost_H
#define GPDXL_CDXLOperatorCost_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/string/CWStringDynamic.h"

namespace gpdxl
{
using namespace gpos;

// fwd decl
class CXMLSerializer;

//---------------------------------------------------------------------------
//	@class:
//		CDXLOperatorCost
//
//	@doc:
//		Class for representing costs of physical operators in the DXL tree
//
//---------------------------------------------------------------------------
class CDXLOperatorCost : public CRefCount
{
private:
	// cost expended before fetching any tuples
	CWStringDynamic *m_startup_cost_str;

	// total cost (assuming all tuples fetched)
	CWStringDynamic *m_total_cost_str;

	// number of rows plan is expected to emit
	CWStringDynamic *m_rows_out_str;

	// average row width in bytes
	CWStringDynamic *m_width_str;

	// private copy ctor
	CDXLOperatorCost(const CDXLOperatorCost &);

public:
	// ctor/dtor
	CDXLOperatorCost(CWStringDynamic *startup_cost_str,
					 CWStringDynamic *total_cost_str,
					 CWStringDynamic *rows_out_str, CWStringDynamic *width_str);

	virtual ~CDXLOperatorCost();

	// serialize operator in DXL format
	void SerializeToDXL(CXMLSerializer *xml_serializer) const;

	// accessors
	const CWStringDynamic *GetStartUpCostStr() const;
	const CWStringDynamic *GetTotalCostStr() const;
	const CWStringDynamic *GetRowsOutStr() const;
	const CWStringDynamic *GetWidthStr() const;

	// set the number of rows
	void SetRows(CWStringDynamic *str);

	// set the total cost
	void SetCost(CWStringDynamic *str);
};
}  // namespace gpdxl


#endif	// !GPDXL_CDXLOperatorCost_H

// EOF
