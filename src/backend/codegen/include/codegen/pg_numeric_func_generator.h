//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    pg_numeric_func_generator.h
//
//  @doc:
//    Base class for numeric functions to generate code
//
//---------------------------------------------------------------------------

#ifndef GPDB_PG_NUMERIC_FUNC_GENERATOR_H_  // NOLINT(build/header_guard)
#define GPDB_PG_NUMERIC_FUNC_GENERATOR_H_

#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/pg_func_generator.h"
#include "codegen/pg_func_generator_interface.h"

#include "llvm/IR/Constant.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#include "c.h"  // NOLINT(build/include)
#include "utils/numeric.h"
}

namespace llvm {
class Value;
}

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

class GpCodegenUtils;
struct PGFuncGeneratorInfo;

/**
 * @brief Class with Static member function to generate code for numeric
 *        operators.
 **/

class PGNumericFuncGenerator {
 public:
  /**
   * @brief  Create LLVM instructions for intfloat_avg_accum_decum function,
   *         which is called by int8_avg_accum and float8_avg_accum
   *         built-in functions. Only accum case has been implemented.
   *
   * @tparam CType              Data type of input argument.
   * @param  codegen_utils      Utility for easy code generation.
   * @param  pg_func_info       Details for pgfunc generation
   * @param  llvm_out_value     Variable to keep the result
   *
   * @return true if generation was successful otherwise return false.
   **/
  template<typename CType>
  static bool GenerateIntFloatAvgAccum(
      gpcodegen::GpCodegenUtils* codegen_utils,
      const gpcodegen::PGFuncGeneratorInfo& pg_func_info,
      llvm::Value** llvm_out_value);

  /**
   * @brief Create LLVM instructions for intfloat_avg_amalg_demalg function,
   *        which is called by int8_avg_amalg and float8_avg_amalg
   *        built-in functions. Only amalg case has been implemented.
   *
   * @param codegen_utils      Utility for easy code generation.
   * @param pg_func_info       Details for pgfunc generation
   * @param llvm_out_value     Variable to keep the result
   *
   * @return true if generation was successful otherwise return false.
   **/
  static bool GenerateIntFloatAvgAmalg(
      gpcodegen::GpCodegenUtils* codegen_utils,
      const gpcodegen::PGFuncGeneratorInfo& pg_func_info,
      llvm::Value** llvm_out_value);

 private:
  /**
   * @brief A helper function that creates LLVM instructions that check if a
   *        pointer points to a memory chunk that has a given size.
   *
   * @param codegen_utils      Utility for easy code generation.
   * @param llvm_ptr           Pointer to the memory chunk
   * @param llvm_size          Expected size
   * @param llvm_out_cond      Will contain the result of the condition.
   *
   * @return true if generation was successful otherwise return false.
   **/
  static bool GenerateVarlenSizeCheck(gpcodegen::GpCodegenUtils* codegen_utils,
                                      llvm::Value* llvm_ptr,
                                      llvm::Value* llvm_size,
                                      llvm::Value** llvm_out_cond);

  /**
   * @brief A helper function that creates LLVM instructions which implement
   *        the logic of the first `if` statement in intfloat_avg_accum_decum
   *        and intfloat_avg_amalg_demalg.
   *
   * @param codegen_utils          Utility for easy code generation.
   * @param llvm_in_transdata_ptr  Pointer to transdata llvm value
   * @param llvm_out_trandata_ptr  If llvm_in_transdata_ptr is not valid,
   *                               then llvm_out_trandata_ptr will point to
   *                               a valid transdata llvm value.
   *
   * @return true if generation was successful otherwise return false.
   **/
  static bool GeneratePallocTransdata(gpcodegen::GpCodegenUtils* codegen_utils,
                                      llvm::Value* llvm_in_transdata_ptr,
                                      llvm::Value** llvm_out_trandata_ptr);
};

template<typename CType>
bool PGNumericFuncGenerator::GenerateIntFloatAvgAccum(
    gpcodegen::GpCodegenUtils* codegen_utils,
    const gpcodegen::PGFuncGeneratorInfo& pg_func_info,
    llvm::Value** llvm_out_value) {
  // TODO(nikos): Can we figure if we need to detoast during generation?
  llvm::Function* llvm_pg_detoast_datum = codegen_utils->
      GetOrRegisterExternalFunction(pg_detoast_datum, "pg_detoast_datum");

  auto irb = codegen_utils->ir_builder();

  llvm::Value* llvm_in_transdata_ptr =
      irb->CreateCall(llvm_pg_detoast_datum, {pg_func_info.llvm_args[0]});
  llvm::Value* llvm_newval = codegen_utils->CreateCast<float8, CType>(
      pg_func_info.llvm_args[1]);

  // if(transdata == NULL ||
  //    VARSIZE(transdata) != sizeof(IntFloatAvgTransdata)) { ... } {{
  llvm::Value* llvm_transdata_ptr = nullptr;
  GeneratePallocTransdata(codegen_utils,
                          llvm_in_transdata_ptr,
                          &llvm_transdata_ptr);
  // }}
  // ++transdata->count;
  // transdata->sum += newval; {{
  llvm::Value* llvm_transdata_sum_ptr = codegen_utils->GetPointerToMember(
      llvm_transdata_ptr, &IntFloatAvgTransdata::sum);
  llvm::Value* llvm_transdata_count_ptr = codegen_utils->GetPointerToMember(
      llvm_transdata_ptr, &IntFloatAvgTransdata::count);
  irb->CreateStore(
      irb->CreateFAdd(irb->CreateLoad(llvm_transdata_sum_ptr), llvm_newval),
      llvm_transdata_sum_ptr);
  irb->CreateStore(irb->CreateAdd(irb->CreateLoad(llvm_transdata_count_ptr),
                                  codegen_utils->GetConstant<int64>(1)),
                   llvm_transdata_count_ptr);
  // }}
  *llvm_out_value = llvm_transdata_ptr;
  return true;
}

/** @} */
}  // namespace gpcodegen

#endif  // GPDB_PG_NUMERIC_FUNC_GENERATOR_H_
