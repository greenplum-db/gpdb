//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    op_expr_tree_generator.h
//
//  @doc:
//    Object that generate code for operator expression.
//
//---------------------------------------------------------------------------
#ifndef GPCODEGEN_OP_EXPR_TREE_GENERATOR_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_OP_EXPR_TREE_GENERATOR_H_

#include <vector>

#include "codegen/expr_tree_generator.h"
#include "codegen/pg_func_generator_interface.h"
#include "codegen/pg_func_generator.h"

#include "llvm/IR/Value.h"


namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

using CodeGenFuncMap = std::unordered_map<unsigned int,
    std::unique_ptr<gpcodegen::PGFuncGeneratorInterface>>;

/**
 * @brief Object that generate code for operator expression.
 **/
class OpExprTreeGenerator : public ExprTreeGenerator {
 public:
  /**
   * @brief Initialize PG operator function that we support for code generation.
   **/
  static void InitializeSupportedFunction();

  static bool VerifyAndCreateExprTree(
        const ExprState* expr_state,
        ExprTreeGeneratorInfo* gen_info,
        std::unique_ptr<ExprTreeGenerator>* expr_tree);

  static gpcodegen::PGFuncGeneratorInterface* GetPGFuncGenerator(
      unsigned int oid);

  bool GenerateCode(gpcodegen::GpCodegenUtils* codegen_utils,
                    const ExprTreeGeneratorInfo& gen_info,
                    llvm::Value* llvm_isnull_ptr,
                    llvm::Value** llvm_out_value) final;

 protected:
  /**
   * @brief Constructor.
   *
   * @param expr_state Expression state
   * @param arguments Arguments to operator as list of ExprTreeGenerator
   **/
  OpExprTreeGenerator(
      const ExprState* expr_state,
      std::vector<
          std::unique_ptr<
              ExprTreeGenerator>>&& arguments);  // NOLINT(build/c++11)

 private:
  std::vector<std::unique_ptr<ExprTreeGenerator>> arguments_;
  // Map of supported function with respective generator to generate code
  static CodeGenFuncMap supported_function_;
};


/** @} */
}  // namespace gpcodegen

#endif  // GPCODEGEN_OP_EXPR_TREE_GENERATOR_H_
