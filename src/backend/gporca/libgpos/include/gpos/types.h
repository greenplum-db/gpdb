//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2008 Greenplum, Inc.
//
//	@filename:
//		types.h
//
//	@doc:
//		Type definitions for gpos to avoid using native types directly;
//
//		TODO: 03/15/2008; the seletion of basic types which then
//		get mapped to internal types should be done by autoconf or other
//		external tools; for the time being these are hard-coded; cpl asserts
//		are used to make sure things are failing if compiled with a different
//		compiler.
//---------------------------------------------------------------------------
#ifndef GPOS_types_H
#define GPOS_types_H

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <iostream>

#include "gpos/assert.h"

#define GPOS_SIZEOF(x) ((gpos::ULONG) sizeof(x))
#define GPOS_ARRAY_SIZE(x) (GPOS_SIZEOF(x) / GPOS_SIZEOF(x[0]))
#define GPOS_OFFSET(T, M) ((gpos::ULONG)(SIZE_T) & (((T *) 0x1)->M) - 1)

/* wide character string literate */
#define GPOS_WSZ_LIT(x) L##x

namespace gpos
{
// Basic types to be used instead of built-ins
// Add types as needed;

using BYTE = unsigned char;
using CHAR = char;
// ignore signed char for the moment

// wide character type
using WCHAR = wchar_t;

using BOOL = bool;

// numeric types

using SIZE_T = size_t;
using SSIZE_T = ssize_t;
using MODE_T = mode_t;

// define ULONG,ULLONG as types which implement standard's
// requirements for ULONG_MAX and ULLONG_MAX; eliminate standard's slack
// by fixed sizes rather than min requirements

using ULONG = uint32_t;
GPOS_CPL_ASSERT(4 == sizeof(ULONG));
enum
{
	ulong_max = ((::gpos::ULONG) -1)
};

using ULLONG = uint64_t;
GPOS_CPL_ASSERT(8 == sizeof(ULLONG));
enum
{
	ullong_max = ((::gpos::ULLONG) -1)
};

using ULONG_PTR = uintptr_t;
#define ULONG_PTR_MAX (gpos::ullong_max)

using USINT = uint16_t;
using SINT = int16_t;
using INT = int32_t;
using LINT = int64_t;
using INT_PTR = intptr_t;

GPOS_CPL_ASSERT(2 == sizeof(USINT));
GPOS_CPL_ASSERT(2 == sizeof(SINT));
GPOS_CPL_ASSERT(4 == sizeof(INT));
GPOS_CPL_ASSERT(8 == sizeof(LINT));

enum
{
	int_max = ((::gpos::INT)(gpos::ulong_max >> 1)),
	int_min = (-gpos::int_max - 1)
};

enum
{
	lint_max = ((::gpos::LINT)(gpos::ullong_max >> 1)),
	lint_min = (-gpos::lint_max - 1)
};

enum
{
	usint_max = ((::gpos::USINT) -1)
};

enum
{
	sint_max = ((::gpos::SINT)(gpos::usint_max >> 1)),
	sint_min = (-gpos::sint_max - 1)
};

using DOUBLE = double;

// holds for all platforms
GPOS_CPL_ASSERT(sizeof(ULONG_PTR) == sizeof(void *));

// variadic parameter list type
using VA_LIST = va_list;

// enum for results on OS level (instead of using a global error variable)
enum GPOS_RESULT
{
	GPOS_OK = 0,

	GPOS_FAILED = 1,
	GPOS_OOM = 2,
	GPOS_NOT_FOUND = 3,
	GPOS_TIMEOUT = 4
};


// enum for locale encoding
enum ELocale
{
	ELocInvalid,  // invalid key for hashtable iteration
	ElocEnUS_Utf8,
	ElocGeDE_Utf8,

	ElocSentinel
};
}  // namespace gpos

#endif	// !GPOS_types_H

// EOF
