//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    pg_date_func_generator.cc
//
//  @doc:
//    Base class for date functions to generate code
//
//---------------------------------------------------------------------------

#include <assert.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "codegen/pg_arith_func_generator.h"
#include "codegen/pg_date_func_generator.h"
#include "codegen/pg_func_generator_interface.h"
#include "codegen/utils/gp_codegen_utils.h"

#include "llvm/IR/Constant.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#include "c.h"  // NOLINT(build/include)
#include "utils/timestamp.h"
}

using gpcodegen::GpCodegenUtils;
using gpcodegen::PGDateFuncGenerator;
using gpcodegen::PGFuncGeneratorInfo;

bool PGDateFuncGenerator::DateLETimestamp(
    gpcodegen::GpCodegenUtils* codegen_utils,
    const PGFuncGeneratorInfo& pg_func_info,
    llvm::Value** llvm_out_value) {

  llvm::IRBuilder<>* irb = codegen_utils->ir_builder();

  // llvm_args[0] is of date type
  llvm::Value* llvm_arg0_Timestamp = GenerateDate2Timestamp(
      codegen_utils, pg_func_info);

  // timestamp_cmp_internal {{{
#ifdef HAVE_INT64_TIMESTAMP
  *llvm_out_value =
      irb->CreateICmpSLE(llvm_arg0_Timestamp, pg_func_info.llvm_args[1]);
#else
  // TODO(nikos): We do not support NaNs.
  elog(DEBUG1, "Timestamp != int_64: NaNs are not supported.");
  return false;
#endif
  // }}}

  return true;
}

llvm::Value* PGDateFuncGenerator::GenerateDate2Timestamp(
    GpCodegenUtils* codegen_utils,
    const PGFuncGeneratorInfo& pg_func_info) {

  assert(pg_func_info.llvm_args.size() > 1);
  assert(nullptr != pg_func_info.llvm_args[0]);
  assert(nullptr != pg_func_info.llvm_args[0]->getType());

#ifdef HAVE_INT64_TIMESTAMP

  llvm::Value *llvm_USECS_PER_DAY = codegen_utils->
          GetConstant<int64_t>(USECS_PER_DAY);

  llvm::Value* llvm_out_value = nullptr;
  llvm::Value* llvm_err_msg = codegen_utils->GetConstant(
      "date out of range for timestamp");
  PGFuncGeneratorInfo pg_timestamp_func_info(
      pg_func_info.llvm_main_func,
      pg_func_info.llvm_error_block,
      {pg_func_info.llvm_args[0], llvm_USECS_PER_DAY},
      pg_func_info.llvm_args_isNull,
      pg_func_info.llvm_isNull_ptr);

  PGArithFuncGenerator<int64_t, int32_t, int64_t>::ArithOpWithOverflow(
      codegen_utils,
      &gpcodegen::GpCodegenUtils::CreateMulOverflow<int64_t>,
      llvm_err_msg,
      pg_timestamp_func_info,
      &llvm_out_value);

  return llvm_out_value;
#else
  llvm::Value* llvm_arg_64 = codegen_utils->CreateCast<int64_t, int32_t>(
      pg_func_info.llvm_args[0]);
  llvm::Value *llvm_SECS_PER_DAY = codegen_utils->
      GetConstant<int64_t>(SECS_PER_DAY);
  return codegen_utils->ir_builder()->CreateMul(llvm_arg_64, llvm_SECS_PER_DAY);
#endif
}
