//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    const_expr_tree_generator.h
//
//  @doc:
//    Object that generate code for const expression.
//
//---------------------------------------------------------------------------
#ifndef GPCODEGEN_CONST_EXPR_TREE_GENERATOR_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_CONST_EXPR_TREE_GENERATOR_H_

#include "codegen/expr_tree_generator.h"

#include "llvm/IR/Value.h"

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

/**
 * @brief Object that generate code for const expression.
 **/
class ConstExprTreeGenerator : public ExprTreeGenerator {
 public:
  static bool VerifyAndCreateExprTree(
      const ExprState* expr_state,
      ExprTreeGeneratorInfo* gen_info,
      std::unique_ptr<ExprTreeGenerator>* expr_tree);

  bool GenerateCode(gpcodegen::GpCodegenUtils* codegen_utils,
                    const ExprTreeGeneratorInfo& gen_info,
                    llvm::Value** llvm_out_value,
                    llvm::Value* const llvm_isnull_ptr) final;

 protected:
  /**
   * @brief Constructor.
   *
   * @param expr_state Expression state from epxression tree
   **/
  explicit ConstExprTreeGenerator(const ExprState* expr_state);
};

/** @} */
}  // namespace gpcodegen

#endif  // GPCODEGEN_CONST_EXPR_TREE_GENERATOR_H_
