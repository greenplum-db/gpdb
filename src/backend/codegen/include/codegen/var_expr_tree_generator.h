//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    var_expr_tree_generator.h
//
//  @doc:
//    Object that generator code for variable expression.
//
//---------------------------------------------------------------------------
#ifndef GPCODEGEN_VAR_EXPR_TREE_GENERATOR_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_VAR_EXPR_TREE_GENERATOR_H_

#include "codegen/expr_tree_generator.h"
#include "codegen/codegen_wrapper.h"

#include "llvm/IR/Value.h"

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

/**
 * @brief Object that generator code for variable expression.
 **/
class VarExprTreeGenerator : public ExprTreeGenerator {
 public:
  static bool VerifyAndCreateExprTree(
        const ExprState* expr_state,
        ExprTreeGeneratorInfo* gen_info,
        std::unique_ptr<ExprTreeGenerator>* expr_tree);

  bool GenerateCode(gpcodegen::GpCodegenUtils* codegen_utils,
                    const ExprTreeGeneratorInfo& gen_info,
                    llvm::Value* const llvm_isnull_ptr,
                    llvm::Value** llvm_out_value) final;
 protected:
  /**
   * @brief Constructor.
   *
   * @param expr_state Expression state
   **/
  explicit VarExprTreeGenerator(const ExprState* expr_state);
};

/** @} */
}  // namespace gpcodegen

#endif  // GPCODEGEN_VAR_EXPR_TREE_GENERATOR_H_
