//---------------------------------------------------------------------------
// Greenplum Database
// Copyright (C) 2016 Pivotal Software, Inc.
//
// @filename:
//   init_codegen.cpp
//
// @doc:
//   C wrappers for initialization of codegen library.
//
//---------------------------------------------------------------------------

#include "codegen/init_codegen.h"

#include "codegen/utils/code_generator.h"

extern "C" int InitCodeGen() {
  return gpcodegen::CodeGenerator::InitializeGlobal() ? 0 : 1;
}
