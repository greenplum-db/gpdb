//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    pg_arith_func_generator.h
//
//  @doc:
//    Class with Static member function to generate code for +, - and * operator
//
//---------------------------------------------------------------------------
#ifndef GPCODEGEN_PG_ARITH_FUNC_GENERATOR_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_PG_ARITH_FUNC_GENERATOR_H_

#include <string>
#include <vector>
#include <memory>

#include "codegen/utils/codegen_utils.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

/**
 * @brief Class with Static member function to generate code for +, - and *
 *        operator
 *
 * @tparam rtype  Return type of function
 * @tparam Arg0   First argument's type
 * @tparam Arg1   Second argument's type
 **/
template <typename rtype, typename Arg0, typename Arg1>
class PGArithFuncGenerator {
 public:
  /**
   * @brief Create LLVM Mul instruction with check for overflow
   *
   * @param codegen_utils     Utility to easy code generation.
   * @param llvm_main_func    Current function for which we are generating code
   * @param llvm_error_block  Basic Block to jump when error happens
   * @param llvm_args         Vector of llvm arguments for the function
   * @param llvm_out_value    Store the results of function
   *
   * @return true if generation was successful otherwise return false
   *
   * @note  If there is overflow, it will do elog::ERROR and then jump to given
   *        error block.
   **/
  static bool MulWithOverflow(gpcodegen::CodegenUtils* codegen_utils,
                              llvm::Function* llvm_main_func,
                              llvm::BasicBlock* llvm_error_block,
                              const std::vector<llvm::Value*>& llvm_args,
                              llvm::Value** llvm_out_value);

  /**
   * @brief Create LLVM Add instruction with check for overflow
   *
   * @param codegen_utils     Utility to easy code generation.
   * @param llvm_main_func    Current function for which we are generating code
   * @param llvm_error_block  Basic Block to jump when error happens
   * @param llvm_args         Vector of llvm arguments for the function
   * @param llvm_out_value    Store the results of function
   *
   * @return true if generation was successful otherwise return false
   *
   * @note  If there is overflow, it will do elog::ERROR and then jump to given
   *        error block.
   **/
  static bool AddWithOverflow(gpcodegen::CodegenUtils* codegen_utils,
                              llvm::Function* llvm_main_func,
                              llvm::BasicBlock* llvm_error_block,
                              const std::vector<llvm::Value*>& llvm_args,
                              llvm::Value** llvm_out_value);
  /**
   * @brief Create LLVM Sub instruction with check for overflow
   *
   * @param codegen_utils     Utility to easy code generation.
   * @param llvm_main_func    Current function for which we are generating code
   * @param llvm_error_block  Basic Block to jump when error happens
   * @param llvm_args         Vector of llvm arguments for the function
   * @param llvm_out_value    Store the results of function
   *
   * @return true if generation was successful otherwise return false
   *
   * @note  If there is overflow, it will do elog::ERROR and then jump to given
   *        error block.
   **/
  static bool SubWithOverflow(gpcodegen::CodegenUtils* codegen_utils,
                              llvm::Function* llvm_main_func,
                              llvm::BasicBlock* llvm_error_block,
                              const std::vector<llvm::Value*>& llvm_args,
                              llvm::Value** llvm_out_value);
};

template <typename rtype, typename Arg0, typename Arg1>
bool PGArithFuncGenerator<rtype, Arg0, Arg1>::MulWithOverflow(
    gpcodegen::CodegenUtils* codegen_utils,
    llvm::Function* llvm_main_func,
    llvm::BasicBlock* llvm_error_block,
    const std::vector<llvm::Value*>& llvm_args,
    llvm::Value** llvm_out_value) {
  assert(nullptr != llvm_out_value);
  // Assumed caller checked vector size and nullptr for codegen_utils

  llvm::Value* llvm_mul_output = codegen_utils->CreateMulOverflow<rtype>(
      llvm_args[0], llvm_args[1]);

  llvm::IRBuilder<>* irb = codegen_utils->ir_builder();

  if (!llvm_mul_output->getType()->isDoubleTy())
  {
    llvm::BasicBlock* llvm_non_overflow_block = codegen_utils->CreateBasicBlock(
        "mul_non_overflow_block", llvm_main_func);
    llvm::BasicBlock* llvm_overflow_block = codegen_utils->CreateBasicBlock(
        "mul_overflow_block", llvm_main_func);

    *llvm_out_value = irb->CreateExtractValue(llvm_mul_output, 0);
    llvm::Value* llvm_overflow_flag = irb->CreateExtractValue(llvm_mul_output, 1);

    irb->CreateCondBr(
        irb->CreateICmpEQ(llvm_overflow_flag,
                          codegen_utils->GetConstant<bool>(true)),
                          llvm_overflow_block,
                          llvm_non_overflow_block);

    irb->SetInsertPoint(llvm_overflow_block);
    // TODO(krajaraman): Elog::ERROR after ElogWrapper integrated.
    irb->CreateBr(llvm_error_block);

    irb->SetInsertPoint(llvm_non_overflow_block);
  }
  else {
    *llvm_out_value = llvm_mul_output;
  }

  return true;
}

template <typename rtype, typename Arg0, typename Arg1>
bool PGArithFuncGenerator<rtype, Arg0, Arg1>::AddWithOverflow(
    gpcodegen::CodegenUtils* codegen_utils,
    llvm::Function* llvm_main_func,
    llvm::BasicBlock* llvm_error_block,
    const std::vector<llvm::Value*>& llvm_args,
    llvm::Value** llvm_out_value) {
  assert(nullptr != llvm_out_value);
  // Assumed caller checked vector size and nullptr for codegen_utils

  llvm::Value* llvm_add_output = codegen_utils->CreateAddOverflow<rtype>(
      llvm_args[0], llvm_args[1]);

  llvm::IRBuilder<>* irb = codegen_utils->ir_builder();

  if (!llvm_add_output->getType()->isDoubleTy())
  {
    llvm::BasicBlock* llvm_non_overflow_block = codegen_utils->CreateBasicBlock(
        "add_non_overflow_block", llvm_main_func);
    llvm::BasicBlock* llvm_overflow_block = codegen_utils->CreateBasicBlock(
        "add_overflow_block", llvm_main_func);

    *llvm_out_value = irb->CreateExtractValue(llvm_add_output, 0);
    llvm::Value* llvm_overflow_flag = irb->CreateExtractValue(llvm_add_output, 1);

    irb->CreateCondBr(
        irb->CreateICmpEQ(llvm_overflow_flag,
                          codegen_utils->GetConstant<bool>(true)),
                          llvm_overflow_block,
                          llvm_non_overflow_block );

    irb->SetInsertPoint(llvm_overflow_block);
    // TODO : krajaraman Elog::ERROR after ElogWrapper integrad.
    irb->CreateBr(llvm_error_block);
    irb->SetInsertPoint(llvm_non_overflow_block);
  }
  else {
    *llvm_out_value = llvm_add_output;
  }

  return true;
}

template <typename rtype, typename Arg0, typename Arg1>
bool PGArithFuncGenerator<rtype, Arg0, Arg1>::SubWithOverflow(
    gpcodegen::CodegenUtils* codegen_utils,
    llvm::Function* llvm_main_func,
    llvm::BasicBlock* llvm_error_block,
    const std::vector<llvm::Value*>& llvm_args,
    llvm::Value** llvm_out_value) {
  assert(nullptr != llvm_out_value);
  // Assumed caller checked vector size and nullptr for codegen_utils

  llvm::Value* llvm_sub_output = codegen_utils->CreateSubOverflow<rtype>(
      llvm_args[0], llvm_args[1]);

  llvm::IRBuilder<>* irb = codegen_utils->ir_builder();

  if (!llvm_sub_output->getType()->isDoubleTy())
  {
    llvm::BasicBlock* llvm_non_overflow_block = codegen_utils->CreateBasicBlock(
        "sub_non_overflow_block", llvm_main_func);
    llvm::BasicBlock* llvm_overflow_block = codegen_utils->CreateBasicBlock(
        "sub_overflow_block", llvm_main_func);

    *llvm_out_value = irb->CreateExtractValue(llvm_sub_output, 0);
    llvm::Value* llvm_overflow_flag = irb->CreateExtractValue(llvm_sub_output, 1);

    irb->CreateCondBr(
        irb->CreateICmpEQ(llvm_overflow_flag,
                          codegen_utils->GetConstant<bool>(true)),
                          llvm_overflow_block,
                          llvm_non_overflow_block);

    irb->SetInsertPoint(llvm_overflow_block);
    // TODO(krajaraman): Elog::ERROR after ElogWrapper integrated.
    irb->CreateBr(llvm_error_block);

    irb->SetInsertPoint(llvm_non_overflow_block);
  }
  else {
    *llvm_out_value = llvm_sub_output;
  }
  return true;
}

/** @} */
}  // namespace gpcodegen

#endif  // GPCODEGEN_PG_ARITH_FUNC_GENERATOR_H_
