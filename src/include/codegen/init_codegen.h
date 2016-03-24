//---------------------------------------------------------------------------
// Greenplum Database
// Copyright (C) 2016 Pivotal Software, Inc.
//
// @filename:
//   init_codegen.h
//
// @doc:
//   C wrappers for initialization of codegen library.
//
//---------------------------------------------------------------------------
#ifndef CODEGEN_INIT_CODEGEN_H_
#define CODEGEN_INIT_CODEGEN_H_

#ifdef __cplusplus
extern "C" {
#endif

// Do one-time global initialization of Clang and LLVM libraries. Returns 0
// on success, nonzero on error.
int InitCodeGen();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // CODEGEN_INIT_CODEGEN_H_

