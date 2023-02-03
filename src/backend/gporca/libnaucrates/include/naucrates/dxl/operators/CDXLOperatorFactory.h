//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLOperatorFactory.h
//
//	@doc:
//		Factory for creating DXL tree elements out of parsed XML attributes
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLOperatorFactory_H
#define GPDXL_CDXLOperatorFactory_H

#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/XMLStringTokenizer.hpp>
#include <xercesc/util/XMLUniDefs.hpp>

#include "gpos/base.h"
#include "gpos/common/CDouble.h"

#include "naucrates/base/IDatum.h"
#include "naucrates/dxl/operators/CDXLColRef.h"
#include "naucrates/dxl/operators/CDXLCtasStorageOptions.h"
#include "naucrates/dxl/operators/CDXLIndexDescr.h"
#include "naucrates/dxl/operators/CDXLLogical.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/operators/CDXLPhysicalMotion.h"
#include "naucrates/dxl/operators/CDXLScalarBoolExpr.h"
#include "naucrates/dxl/operators/CDXLScalarBooleanTest.h"
#include "naucrates/dxl/operators/CDXLScalarSubPlan.h"
#include "naucrates/dxl/operators/CDXLScalarWindowFrameEdge.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"
#include "naucrates/dxl/operators/CDXLWindowFrame.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/md/CMDFunctionGPDB.h"
#include "naucrates/md/CMDRelationGPDB.h"
#include "naucrates/md/IMDId.h"
#include "naucrates/md/IMDIndex.h"

// dynamic array of XML strings
using XMLChArray = CDynamicPtrArray<XMLCh, CleanupNULL>;

// fwd decl
namespace gpmd
{
class CMDIdGPDB;
class CMDIdColStats;
class CMDIdRelStats;
class CMDIdCast;
class CMDIdScCmp;
}  // namespace gpmd

namespace gpdxl
{
using namespace gpos;
using namespace gpmd;
using namespace gpnaucrates;

XERCES_CPP_NAMESPACE_USE

//fwd decl
class CDXLMemoryManager;
class CDXLDatum;

//---------------------------------------------------------------------------
//	@class:
//		CDXLOperatorFactory
//
//	@doc:
//		Factory class containing static methods for creating DXL objects
//		from parsed DXL information such as XML element's attributes
//
//---------------------------------------------------------------------------
class CDXLOperatorFactory
{
private:
	// return the LINT value of byte array
	static LINT Value(CDXLMemoryManager *dxl_memory_manager,
					  const Attributes &attrs, Edxltoken target_elem,
					  const BYTE *data);

	// parses a byte array representation of the datum
	static BYTE *GetByteArray(CDXLMemoryManager *dxl_memory_manager,
							  const Attributes &attrs, Edxltoken target_elem,
							  ULONG *length);

public:
	static CDXLDatum *GetDatumOid(CDXLMemoryManager *dxl_memory_manager,
								  const Attributes &attrs,
								  Edxltoken target_elem, IMDId *mdid,
								  BOOL is_const_null);

	static CDXLDatum *GetDatumInt2(CDXLMemoryManager *dxl_memory_manager,
								   const Attributes &attrs,
								   Edxltoken target_elem, IMDId *mdid,
								   BOOL is_const_null);

	static CDXLDatum *GetDatumInt4(CDXLMemoryManager *dxl_memory_manager,
								   const Attributes &attrs,
								   Edxltoken target_elem, IMDId *mdid,
								   BOOL is_const_null);

	static CDXLDatum *GetDatumInt8(CDXLMemoryManager *dxl_memory_manager,
								   const Attributes &attrs,
								   Edxltoken target_elem, IMDId *mdid,
								   BOOL is_const_null);

	static CDXLDatum *GetDatumBool(CDXLMemoryManager *dxl_memory_manager,
								   const Attributes &attrs,
								   Edxltoken target_elem, IMDId *mdid,
								   BOOL is_const_null);

	// parse a dxl datum of type generic
	static CDXLDatum *GetDatumGeneric(CDXLMemoryManager *dxl_memory_manager,
									  const Attributes &attrs,
									  Edxltoken target_elem, IMDId *mdid,
									  BOOL is_const_null);

	// parse a dxl datum of types that need double mapping
	static CDXLDatum *GetDatumStatsDoubleMappable(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs,
		Edxltoken target_elem, IMDId *mdid, BOOL is_const_null);

	// parse a dxl datum of types that need lint mapping
	static CDXLDatum *GetDatumStatsLintMappable(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs,
		Edxltoken target_elem, IMDId *mdid, BOOL is_const_null);

	// create a table scan operator
	static CDXLPhysical *MakeDXLTblScan(CDXLMemoryManager *dxl_memory_manager,
										const Attributes &attrs);

	// create a subquery scan operator
	static CDXLPhysical *MakeDXLSubqScan(CDXLMemoryManager *dxl_memory_manager,
										 const Attributes &attrs);

	// create a result operator
	static CDXLPhysical *MakeDXLResult(CDXLMemoryManager *dxl_memory_manager);

	// create a hashjoin operator
	static CDXLPhysical *MakeDXLHashJoin(CDXLMemoryManager *dxl_memory_manager,
										 const Attributes &attrs);

	// create a nested loop join operator
	static CDXLPhysical *MakeDXLNLJoin(CDXLMemoryManager *dxl_memory_manager,
									   const Attributes &attrs);

	// create a merge join operator
	static CDXLPhysical *MakeDXLMergeJoin(CDXLMemoryManager *dxl_memory_manager,
										  const Attributes &attrs);

	// create a gather motion operator
	static CDXLPhysical *MakeDXLGatherMotion(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs);

	// create a broadcast motion operator
	static CDXLPhysical *MakeDXLBroadcastMotion(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs);

	// create a redistribute motion operator
	static CDXLPhysical *MakeDXLRedistributeMotion(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs);

	// create a routed motion operator
	static CDXLPhysical *MakeDXLRoutedMotion(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs);

	// create a random motion operator
	static CDXLPhysical *MakeDXLRandomMotion(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs);

	// create an append operator
	static CDXLPhysical *MakeDXLAppend(CDXLMemoryManager *dxl_memory_manager,
									   const Attributes &attrs);

	// create a limit operator
	static CDXLPhysical *MakeDXLLimit(CDXLMemoryManager *dxl_memory_manager,
									  const Attributes &attrs);

	// create an aggregation operator
	static CDXLPhysical *MakeDXLAgg(CDXLMemoryManager *dxl_memory_manager,
									const Attributes &attrs);

	// create a sort operator
	static CDXLPhysical *MakeDXLSort(CDXLMemoryManager *dxl_memory_manager,
									 const Attributes &attrs);

	// create a materialize operator
	static CDXLPhysical *MakeDXLMaterialize(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs);

	// create a limit count operator
	static CDXLScalar *MakeDXLLimitCount(CDXLMemoryManager *dxl_memory_manager,
										 const Attributes &attrs);

	// create a limit offset operator
	static CDXLScalar *MakeDXLLimitOffset(CDXLMemoryManager *dxl_memory_manager,
										  const Attributes &attrs);

	// create a scalar comparison operator
	static CDXLScalar *MakeDXLScalarCmp(CDXLMemoryManager *dxl_memory_manager,
										const Attributes &attrs);

	// create a distinct comparison operator
	static CDXLScalar *MakeDXLDistinctCmp(CDXLMemoryManager *dxl_memory_manager,
										  const Attributes &attrs);

	// create a scalar OpExpr
	static CDXLScalar *MakeDXLOpExpr(CDXLMemoryManager *dxl_memory_manager,
									 const Attributes &attrs);

	// create a scalar ArrayComp
	static CDXLScalar *MakeDXLArrayComp(CDXLMemoryManager *dxl_memory_manager,
										const Attributes &attrs);

	// create a BoolExpr
	static CDXLScalar *MakeDXLBoolExpr(CDXLMemoryManager *dxl_memory_manager,
									   const EdxlBoolExprType);

	// create a boolean test
	static CDXLScalar *MakeDXLBooleanTest(CDXLMemoryManager *dxl_memory_manager,
										  const EdxlBooleanTestType);

	// create a subplan operator
	static CDXLScalar *MakeDXLSubPlan(CDXLMemoryManager *dxl_memory_manager,
									  IMDId *mdid,
									  CDXLColRefArray *dxl_colref_array,
									  EdxlSubPlanType dxl_subplan_type,
									  CDXLNode *dxlnode_test_expr);

	// create a NullTest
	static CDXLScalar *MakeDXLNullTest(CDXLMemoryManager *dxl_memory_manager,
									   const BOOL);

	// create a cast
	static CDXLScalar *MakeDXLCast(CDXLMemoryManager *dxl_memory_manager,
								   const Attributes &attrs);

	// create a coerce
	static CDXLScalar *MakeDXLCoerceToDomain(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs);

	// create a CoerceViaIo
	static CDXLScalar *MakeDXLCoerceViaIO(CDXLMemoryManager *dxl_memory_manager,
										  const Attributes &attrs);

	// create a ArrayCoerceExpr
	static CDXLScalar *MakeDXLArrayCoerceExpr(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs);

	// create a scalar identifier operator
	static CDXLScalar *MakeDXLScalarIdent(CDXLMemoryManager *dxl_memory_manager,
										  const Attributes &attrs);

	// create a scalar Const
	static CDXLScalar *MakeDXLConstValue(CDXLMemoryManager *dxl_memory_manager,
										 const Attributes &attrs);

	// create a CaseStmt
	static CDXLScalar *MakeDXLIfStmt(CDXLMemoryManager *dxl_memory_manager,
									 const Attributes &attrs);

	// create a FuncExpr
	static CDXLScalar *MakeDXLFuncExpr(CDXLMemoryManager *dxl_memory_manager,
									   const Attributes &attrs);

	// create a AggRef
	static CDXLScalar *MakeDXLAggFunc(CDXLMemoryManager *dxl_memory_manager,
									  const Attributes &attrs);

	// create a scalar window function (WindowRef)
	static CDXLScalar *MakeWindowRef(CDXLMemoryManager *dxl_memory_manager,
									 const Attributes &attrs);

	// create an array
	static CDXLScalar *MakeDXLArray(CDXLMemoryManager *dxl_memory_manager,
									const Attributes &attr);

	// create a proj elem
	static CDXLScalar *MakeDXLProjElem(CDXLMemoryManager *dxl_memory_manager,
									   const Attributes &attrs);

	// create a hash expr
	static CDXLScalar *MakeDXLHashExpr(CDXLMemoryManager *dxl_memory_manager,
									   const Attributes &attrs);

	// create a sort col
	static CDXLScalar *MakeDXLSortCol(CDXLMemoryManager *dxl_memory_manager,
									  const Attributes &attrs);

	// create an object representing cost estimates of a physical operator
	// from the parsed XML attributes
	static CDXLOperatorCost *MakeDXLOperatorCost(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs);

	// create a table descriptor element
	static CDXLTableDescr *MakeDXLTableDescr(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs);

	// create an index descriptor
	static CDXLIndexDescr *MakeDXLIndexDescr(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attrs);

	// create a column descriptor object
	static CDXLColDescr *MakeColumnDescr(CDXLMemoryManager *dxl_memory_manager,
										 const Attributes &attrs);

	// create a column reference object
	static CDXLColRef *MakeDXLColRef(CDXLMemoryManager *dxl_memory_manager,
									 const Attributes &, Edxltoken);

	// create a logical join
	static CDXLLogical *MakeLogicalJoin(CDXLMemoryManager *dxl_memory_manager,
										const Attributes &attrs);

	// parse an output segment index
	static INT ParseOutputSegId(CDXLMemoryManager *dxl_memory_manager,
								const Attributes &attrs);

	// parse a grouping column id
	static ULONG ParseGroupingColId(CDXLMemoryManager *dxl_memory_manager,
									const Attributes &attrs);

	// extracts the value for the given attribute.
	// if there is no such attribute defined, and the given optional
	// flag is set to false then it will raise an exception
	static const XMLCh *ExtractAttrValue(const Attributes &,
										 Edxltoken target_attr,
										 Edxltoken target_elem,
										 BOOL is_optional = false);

	// extracts the boolean value for the given attribute
	// will raise an exception if value cannot be converted to a boolean
	static BOOL ConvertAttrValueToBool(CDXLMemoryManager *dxl_memory_manager,
									   const XMLCh *xml_val,
									   Edxltoken target_attr,
									   Edxltoken target_elem);

	// converts the XMLCh into LINT
	static LINT ConvertAttrValueToLint(CDXLMemoryManager *dxl_memory_manager,
									   const XMLCh *xml_val,
									   Edxltoken target_attr,
									   Edxltoken target_elem);

	// extracts the LINT value for the given attribute
	static LINT ExtractConvertAttrValueToLint(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem, BOOL is_optional = false,
		LINT default_value = 0);

	// converts the XMLCh into CDouble
	static CDouble ConvertAttrValueToDouble(
		CDXLMemoryManager *dxl_memory_manager, const XMLCh *xml_val,
		Edxltoken target_attr, Edxltoken target_elem);

	// cxtracts the double value for the given attribute
	static CDouble ExtractConvertAttrValueToDouble(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem);

	// converts the XMLCh into ULONG. Will raise an exception if the
	// argument cannot be converted to ULONG
	static ULONG ConvertAttrValueToUlong(CDXLMemoryManager *dxl_memory_manager,
										 const XMLCh *xml_val,
										 Edxltoken target_attr,
										 Edxltoken target_elem);

	// converts the XMLCh into ULLONG. Will raise an exception if the
	// argument cannot be converted to ULLONG
	static ULLONG ConvertAttrValueToUllong(
		CDXLMemoryManager *dxl_memory_manager, const XMLCh *xml_val,
		Edxltoken target_attr, Edxltoken target_elem);

	// converts the XMLCh into INT. Will raise an exception if the
	// argument cannot be converted to INT
	static INT ConvertAttrValueToInt(CDXLMemoryManager *dxl_memory_manager,
									 const XMLCh *xml_val,
									 Edxltoken target_attr,
									 Edxltoken target_elem);

	// parse a INT value from the value for a given attribute
	// will raise an exception if the argument cannot be converted to INT
	static INT ExtractConvertAttrValueToInt(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem, BOOL is_optional = false,
		INT default_val = 0);

	// converts the XMLCh into short int. Will raise an exception if the
	// argument cannot be converted to short int
	static SINT ConvertAttrValueToShortInt(
		CDXLMemoryManager *dxl_memory_manager, const XMLCh *xml_val,
		Edxltoken target_attr, Edxltoken target_elem);

	// parse a short int value from the value for a given attribute
	// will raise an exception if the argument cannot be converted to short int
	static SINT ExtractConvertAttrValueToShortInt(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem, BOOL is_optional = false,
		SINT default_val = 0);

	// converts the XMLCh into char. Will raise an exception if the
	// argument cannot be converted to char
	static CHAR ConvertAttrValueToChar(CDXLMemoryManager *dxl_memory_manager,
									   const XMLCh *xml_val,
									   Edxltoken target_attr,
									   Edxltoken target_elem);

	// converts the XMLCh into oid. Will raise an exception if the
	// argument cannot be converted to OID
	static OID ConvertAttrValueToOid(CDXLMemoryManager *dxl_memory_manager,
									 const XMLCh *xml_val,
									 Edxltoken target_attr,
									 Edxltoken target_elem);

	// parse an oid value from the value for a given attribute
	// will raise an exception if the argument cannot be converted to OID
	static OID ExtractConvertAttrValueToOid(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem, BOOL is_optional = false,
		OID OidDefaultValue = 0);

	// parse a bool value from the value for a given attribute
	static BOOL ExtractConvertAttrValueToBool(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem, BOOL is_optional = false,
		BOOL default_value = false);

	// parse a string value from the value for a given attribute
	static CHAR *ExtractConvertAttrValueToSz(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem, BOOL is_optional = false,
		CHAR *default_value = nullptr);

	// parse a string value from the value for a given attribute
	static CHAR *ConvertAttrValueToSz(CDXLMemoryManager *dxl_memory_manager,
									  const XMLCh *xml_val,
									  Edxltoken target_attr,
									  Edxltoken target_elem);

	// parse a string value from the value for a given attribute
	static CWStringDynamic *ExtractConvertAttrValueToStr(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem);

	// parse a ULONG value from the value for a given attribute
	// will raise an exception if the argument cannot be converted to ULONG
	static ULONG ExtractConvertAttrValueToUlong(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem, BOOL is_optional = false,
		ULONG default_value = 0);

	// parse a ULLONG value from the value for a given attribute
	// will raise an exception if the argument cannot be converted to ULLONG
	static ULLONG ExtractConvertAttrValueToUllong(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem, BOOL is_optional = false,
		ULLONG default_value = 0);

	// parse an mdid object from the given attributes
	static IMDId *ExtractConvertAttrValueToMdId(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem, BOOL is_optional = false,
		IMDId *default_val = nullptr);

	// parse an mdid object from an XMLCh
	static IMDId *MakeMdIdFromStr(CDXLMemoryManager *dxl_memory_manager,
								  const XMLCh *mdid_xml, Edxltoken target_attr,
								  Edxltoken target_elem);

	// parse a GPDB mdid object from an array of its components
	static CMDIdGPDB *GetGPDBMdId(
		CDXLMemoryManager *dxl_memory_manager, XMLChArray *remaining_tokens,
		Edxltoken target_attr, Edxltoken target_elem,
		IMDId::EMDIdType mdidType = IMDId::EmdidGeneral);

	// parse a GPDB CTAS mdid object from an array of its components
	static CMDIdGPDB *GetGPDBCTASMdId(CDXLMemoryManager *dxl_memory_manager,
									  XMLChArray *remaining_tokens,
									  Edxltoken target_attr,
									  Edxltoken target_elem);

	// parse a column stats mdid object from an array of its components
	static CMDIdColStats *GetColStatsMdId(CDXLMemoryManager *dxl_memory_manager,
										  XMLChArray *remaining_tokens,
										  Edxltoken target_attr,
										  Edxltoken target_elem);

	// parse a relation stats mdid object from an array of its components
	static CMDIdRelStats *GetRelStatsMdId(CDXLMemoryManager *dxl_memory_manager,
										  XMLChArray *remaining_tokens,
										  Edxltoken target_attr,
										  Edxltoken target_elem);

	// parse a cast func mdid from the array of its components
	static CMDIdCast *GetCastFuncMdId(CDXLMemoryManager *dxl_memory_manager,
									  XMLChArray *remaining_tokens,
									  Edxltoken target_attr,
									  Edxltoken target_elem);

	// parse a comparison operator mdid from the array of its components
	static CMDIdScCmp *GetScCmpMdId(CDXLMemoryManager *dxl_memory_manager,
									XMLChArray *remaining_tokens,
									Edxltoken target_attr,
									Edxltoken target_elem);

	// parse a dxl datum object
	static CDXLDatum *GetDatumVal(CDXLMemoryManager *dxl_memory_manager,
								  const Attributes &attrs,
								  Edxltoken target_elem);

	// parse a comma-separated list of MDids into a dynamic array
	// will raise an exception if list is not well-formed
	static IMdIdArray *ExtractConvertMdIdsToArray(
		CDXLMemoryManager *dxl_memory_manager, const XMLCh *mdid_list_xml,
		Edxltoken target_attr, Edxltoken target_elem);

	// parse a comma-separated list of unsigned long numbers into a dynamic array
	// will raise an exception if list is not well-formed
	static ULongPtrArray *ExtractConvertValuesToArray(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem);

	static IntPtrArray *ExtractConvertValuesToIntArray(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem);

	static CBitSet *ExtractConvertValuesToIntBitSet(
		CDXLMemoryManager *dxl_memory_manager, const Attributes &attr,
		Edxltoken target_attr, Edxltoken target_elem);

	// parse a comma-separated list of integers numbers into a dynamic array
	// will raise an exception if list is not well-formed
	template <typename T, void (*CleanupFn)(T *),
			  T ValueFromXmlstr(CDXLMemoryManager *, const XMLCh *, Edxltoken,
								Edxltoken)>
	static CDynamicPtrArray<T, CleanupFn> *ExtractIntsToArray(
		CDXLMemoryManager *dxl_memory_manager, const XMLCh *xmlszUl,
		Edxltoken target_attr, Edxltoken target_elem);

	static ULongPtrArray *
	ExtractIntsToUlongArray(CDXLMemoryManager *dxl_memory_manager,
							const XMLCh *xmlszUl, Edxltoken target_attr,
							Edxltoken target_elem)
	{
		return ExtractIntsToArray<ULONG, CleanupDelete,
								  ConvertAttrValueToUlong>(
			dxl_memory_manager, xmlszUl, target_attr, target_elem);
	}

	static IntPtrArray *
	ExtractIntsToIntArray(CDXLMemoryManager *dxl_memory_manager,
						  const XMLCh *xmlszUl, Edxltoken target_attr,
						  Edxltoken target_elem)
	{
		return ExtractIntsToArray<INT, CleanupDelete, ConvertAttrValueToInt>(
			dxl_memory_manager, xmlszUl, target_attr, target_elem);
	}

	static CBitSet *
	ExtractIntsToIntBitSet(CDXLMemoryManager *dxl_memory_manager,
						   const XMLCh *mdid_list_xml, Edxltoken target_attr,
						   Edxltoken target_elem)
	{
		// get the memory pool from the memory manager
		CMemoryPool *mp = dxl_memory_manager->Pmp();

		CBitSet *pbs = GPOS_NEW(mp) CBitSet(mp);

		XMLStringTokenizer mdid_components(
			mdid_list_xml, CDXLTokens::XmlstrToken(EdxltokenComma));
		const ULONG num_tokens = mdid_components.countTokens();

		for (ULONG ul = 0; ul < num_tokens; ul++)
		{
			XMLCh *xmlszNext = mdid_components.nextToken();
			GPOS_ASSERT(nullptr != xmlszNext);

			INT attno = ConvertAttrValueToInt(dxl_memory_manager, xmlszNext,
											  target_attr, target_elem);
			pbs->ExchangeSet(attno);
		}

		return pbs;
	}

	// parse a comma-separated list of CHAR partition types into a dynamic array.
	// will raise an exception if list is not well-formed
	static CharPtrArray *ExtractConvertPartitionTypeToArray(
		CDXLMemoryManager *dxl_memory_manager, const XMLCh *xml_val,
		Edxltoken target_attr, Edxltoken target_elem);

	// parse a semicolon-separated list of comma-separated unsigned
	// long numbers into a dynamc array of unsigned integer arrays
	// will raise an exception if list is not well-formed
	static ULongPtr2dArray *ExtractConvertUlongTo2DArray(
		CDXLMemoryManager *dxl_memory_manager, const XMLCh *xml_val,
		Edxltoken target_attr, Edxltoken target_elem);

	// parse a comma-separated list of segment ids into a dynamic array
	// will raise an exception if list is not well-formed
	static IntPtrArray *ExtractConvertSegmentIdsToArray(
		CDXLMemoryManager *dxl_memory_manager, const XMLCh *seg_id_list_xml,
		Edxltoken target_attr, Edxltoken target_elem);

	// parse a comma-separated list of strings into a dynamic array
	// will raise an exception if list is not well-formed
	static StringPtrArray *ExtractConvertStrsToArray(
		CDXLMemoryManager *dxl_memory_manager, const XMLCh *xml_val);

	// parses the input and output segment ids from Xerces attributes and
	// stores them in the provided DXL Motion operator
	// will raise an exception if lists are not well-formed
	static void SetSegmentInfo(CDXLMemoryManager *mp,
							   CDXLPhysicalMotion *motion,
							   const Attributes &attrs, Edxltoken target_elem);

	static EdxlJoinType ParseJoinType(const XMLCh *xmlszJoinType,
									  const CWStringConst *join_name);

	static EdxlIndexScanDirection ParseIndexScanDirection(
		const XMLCh *direction_xml,
		const CWStringConst *pstrIndexScanDirection);

	// parse system id
	static CSystemId Sysid(CDXLMemoryManager *dxl_memory_manager,
						   const Attributes &attrs, Edxltoken target_attr,
						   Edxltoken target_elem);

	// parse the frame boundary
	static EdxlFrameBoundary ParseDXLFrameBoundary(const Attributes &attrs,
												   Edxltoken token_type);

	// parse the frame specification
	static EdxlFrameSpec ParseDXLFrameSpec(const Attributes &attrs);

	// parse the frame exclusion strategy
	static EdxlFrameExclusionStrategy ParseFrameExclusionStrategy(
		const Attributes &attrs);

	// parse comparison operator type
	static IMDType::ECmpType ParseCmpType(const XMLCh *xml_str_comp_type);

	// parse the distribution policy from the given XML string
	static IMDRelation::Ereldistrpolicy ParseRelationDistPolicy(
		const XMLCh *xml_val);

	// parse the storage type from the given XML string
	static IMDRelation::Erelstoragetype ParseRelationStorageType(
		const XMLCh *xml_val);

	// parse the OnCommit action spec for CTAS
	static CDXLCtasStorageOptions::ECtasOnCommitAction ParseOnCommitActionSpec(
		const Attributes &attr);

	// parse index type
	static IMDIndex::EmdindexType ParseIndexType(const Attributes &attrs);
};

// parse a comma-separated list of integers numbers into a dynamic array
// will raise an exception if list is not well-formed
template <typename T, void (*CleanupFn)(T *),
		  T ValueFromXmlstr(CDXLMemoryManager *, const XMLCh *, Edxltoken,
							Edxltoken)>
CDynamicPtrArray<T, CleanupFn> *
CDXLOperatorFactory::ExtractIntsToArray(CDXLMemoryManager *dxl_memory_manager,
										const XMLCh *mdid_list_xml,
										Edxltoken target_attr,
										Edxltoken target_elem)
{
	// get the memory pool from the memory manager
	CMemoryPool *mp = dxl_memory_manager->Pmp();

	CDynamicPtrArray<T, CleanupFn> *pdrgpt =
		GPOS_NEW(mp) CDynamicPtrArray<T, CleanupFn>(mp);

	XMLStringTokenizer mdid_components(mdid_list_xml,
									   CDXLTokens::XmlstrToken(EdxltokenComma));
	const ULONG num_tokens = mdid_components.countTokens();

	for (ULONG ul = 0; ul < num_tokens; ul++)
	{
		XMLCh *xmlszNext = mdid_components.nextToken();

		GPOS_ASSERT(nullptr != xmlszNext);

		T *pt = GPOS_NEW(mp) T(ValueFromXmlstr(dxl_memory_manager, xmlszNext,
											   target_attr, target_elem));
		pdrgpt->Append(pt);
	}

	return pdrgpt;
}
}  // namespace gpdxl

#endif	// !GPDXL_CDXLOperatorFactory_H

// EOF
