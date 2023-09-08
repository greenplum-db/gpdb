//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (c) 2020-Present VMware, Inc. or its affiliates
//---------------------------------------------------------------------------
#ifndef GPOS_DbgPrintMixin_H
#define GPOS_DbgPrintMixin_H

#include "gpos/error/CAutoTrace.h"
#include "gpos/io/COstreamStdString.h"

namespace gpos
{
// A Mixin class that adds debug printing to a type.
//
// To use it to add DbgStr() and friends to a type U, simply add this as a
// public base:
// namespace ns {
// class U : public gpos::DbgPrintMixin<U> { ... };
// }
//
// Also drop this snippet into the U.cpp file at the top level:
//
// FORCE_GENERATE_DBGSTR(ns::U);
//
// The FORCE_GENERATE_DBGSTR macro ensures code generation for the unused
// functions so that we can call them in a debugger
//
// Use the following printing multi-line messages in your favorite debugger:
//
// (lldb) setting set escape-non-printables false
// (lldb) p x->DbgStr()
//
// (gdb) printf x->DbgStr()
template <class T>
struct DbgPrintMixin
{
#ifdef GPOS_DEBUG
	// debug print for interactive debugging sessions only
	void
	DbgPrint() const
	{
		CMemoryPool *mp = CMemoryPoolManager::GetGlobalMemoryPool();
		CAutoTrace at(mp);
		static_cast<const T *>(this)->OsPrint(at.Os());
	}

	std::wstring
	DbgStr() const GPOS_UNUSED
	{
		COstreamStdString oss;
		static_cast<const T *>(this)->OsPrint(oss);
		return oss.Str();
	}
#endif
};
}  // namespace gpos

// DbgStr() is designed to be unused until evaluated in a debugger. The "used"
// attribute usually does the trick to convince a mainstream compiler (GCC and
// upstream LLVM Clang) to generate code for DbgStr(). There are two issues with
// the "used" attribute, however:
//
// 1. It slows down compilation. Every translation unit that includes the
// declaration of a derived class will take a hit generating DbgPrint() and
// DbgStr(). But we really only need one copy of them. This is a fairly minor
// issue.
//
// 2. Not so minor, However, is that AppleClang insists to omit code generation
// for unused functions in a class template (this is borderline bug, and there
// isn't a flag to disable such a "feature").
//
// The best remedy is for each derived type, explicitly instantiate the base
// template in the translation unit. This forces code generation even in Apple
// Clang, and it speeds up compilation because we only emit code for each
// explicit instantiation once.
#if defined(GPOS_DEBUG)
#define FORCE_GENERATE_DBGSTR(Type) template struct ::gpos::DbgPrintMixin<Type>
#else
#define FORCE_GENERATE_DBGSTR(Type) static_assert(true, "")
#endif


#endif
