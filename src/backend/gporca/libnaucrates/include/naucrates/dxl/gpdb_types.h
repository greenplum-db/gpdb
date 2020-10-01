//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		gpdb_types.h
//
//	@doc:
//		Types from GPDB.
//---------------------------------------------------------------------------
#ifndef GPDXL_gpdb_types_H
#define GPDXL_gpdb_types_H

#include "gpos/types.h"

using namespace gpos;

typedef ULONG OID;

#define GPDB_INT2 OID(21)
#define GPDB_INT4 OID(23)
#define GPDB_INT8 OID(20)
#define GPDB_BOOL OID(16)
#define GPDB_TID OID(27)
#define GPDB_OID OID(26)
#define GPDB_XID OID(28)
#define GPDB_CID OID(29)

#define GPDB_NUMERIC OID(1700)
#define GPDB_FLOAT4 OID(700)
#define GPDB_FLOAT8 OID(701)
#define GPDB_CASH OID(790)

// time related types
#define GPDB_DATE OID(1082)
#define GPDB_TIME OID(1083)
#define GPDB_TIMETZ OID(1266)
#define GPDB_TIMESTAMP OID(1114)
#define GPDB_TIMESTAMPTZ OID(1184)
#define GPDB_ABSTIME OID(702)
#define GPDB_RELTIME OID(703)
#define GPDB_INTERVAL OID(1186)
#define GPDB_TIMEINTERVAL OID(704)

// text related types
#define GPDB_CHAR OID(1042)
#define GPDB_VARCHAR OID(1043)
#define GPDB_TEXT OID(25)
#define GPDB_NAME OID(19)
#define GPDB_SINGLE_CHAR OID(18)

// network related types
#define GPDB_INET OID(869)
#define GPDB_CIDR OID(650)
#define GPDB_MACADDR OID(829)

#define GPDB_UNKNOWN OID(705)

#define GPDB_DATE_TIMESTAMP_EQUALITY OID(2347)
#define GPDB_TIMESTAMP_DATE_EQUALITY OID(2373)
#define GPDB_DATE_TO_TIMESTAMP_CAST OID(2024)
#define GPDB_TIMESTAMP_TO_DATE_CAST OID(1082)

#define GPDB_COUNT_STAR OID(2803)  // count(*)
#define GPDB_COUNT_ANY OID(2147)   // count(Any)
#define GPDB_UUID OID(2950)
#define GPDB_ANY OID(2283)

#endif	// !GPDXL_gpdb_types_H


// EOF
