//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    codegen_interface.cc
//
//  @doc:
//    Implementation of codegen interface's static function
//
//---------------------------------------------------------------------------
#include "codegen/codegen_interface.h"

using gpcodegen::CodegenInterface;

// Initalization of unique counter
unsigned CodegenInterface::unique_counter_ = 0;

std::string CodegenInterface::GenerateUniqueName(
    const std::string& orig_func_name) {
  return orig_func_name + std::to_string(unique_counter_++);
}
