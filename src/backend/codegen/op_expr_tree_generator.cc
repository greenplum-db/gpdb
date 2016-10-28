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

#include <assert.h>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "codegen/expr_tree_generator.h"
#include "codegen/op_expr_tree_generator.h"
#include "codegen/pg_func_generator.h"
#include "codegen/pg_func_generator_interface.h"
#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/pg_arith_func_generator.h"
#include "codegen/pg_date_func_generator.h"

#include "llvm/IR/IRBuilder.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#include "c.h"  // NOLINT(build/include)
#include "nodes/execnodes.h"
#include "utils/elog.h"
#include "nodes/nodes.h"
#include "nodes/pg_list.h"
#include "nodes/primnodes.h"
}

namespace llvm {
class Value;
}  // namespace llvm

using gpcodegen::OpExprTreeGenerator;
using gpcodegen::ExprTreeGenerator;
using gpcodegen::GpCodegenUtils;
using gpcodegen::PGFuncGeneratorInterface;
using gpcodegen::PGFuncGeneratorFn;
using gpcodegen::CodeGenFuncMap;
using llvm::IRBuilder;


CodeGenFuncMap
OpExprTreeGenerator::supported_function_;

void OpExprTreeGenerator::InitializeSupportedFunction() {
  if (!supported_function_.empty()) { return; }

  supported_function_[141] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int32_t, int32_t, int32_t>(
          141,
          "int4mul",
          &PGArithFuncGenerator<int32_t, int32_t, int32_t>::MulWithOverflow,
          nullptr,
          true));

  supported_function_[149] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGIRBuilderFuncGenerator<bool, int32_t, int32_t>(
          149,
          "int4le",
          &IRBuilder<>::CreateICmpSLE,
          true));

  supported_function_[177] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int32_t, int32_t, int32_t>(
          177,
          "int4pl",
          &PGArithFuncGenerator<int32_t, int32_t, int32_t>::AddWithOverflow,
          nullptr,
          true));

  // int4_sum is not a strict function. It checks if there are NULL arguments
  // and performs actions accordingly.
  supported_function_[1841] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int64_t, int64_t, int32_t>(
          1841,
          "int4_sum",
          &PGArithFuncGenerator<int64_t, int64_t, int32_t>::AddWithOverflow,
          &PGArithFuncGenerator<int64_t, int64_t, int32_t>::
          CreateArgumentNullChecks,
          false));

  supported_function_[181] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int32_t, int32_t, int32_t>(
          181,
          "int4mi",
          &PGArithFuncGenerator<int32_t, int32_t, int32_t>::SubWithOverflow,
          nullptr,
          true));

  supported_function_[463] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int64_t, int64_t, int64_t>(
          463,
          "int8pl",
          &PGArithFuncGenerator<int64_t, int64_t, int64_t>::AddWithOverflow,
          nullptr,
          true));

  supported_function_[1219] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int64_t, int64_t>(
          1219,
          "int8inc",
          &PGArithUnaryFuncGenerator<int64_t, int64_t>::IncWithOverflow,
          nullptr,
          true));

  // int8inc is not strict, but it does not check if there are NULL attributes.
  supported_function_[2803] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<int64_t, int64_t>(
          2803,
          "int8inc",
          &PGArithUnaryFuncGenerator<int64_t, int64_t>::IncWithOverflow,
          nullptr,
          false));

  supported_function_[216] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<float8, float8, float8>(
          216,
          "float8mul",
          &PGArithFuncGenerator<float8, float8, float8>::MulWithOverflow,
          nullptr,
          true));

  supported_function_[218] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<float8, float8, float8>(
          218,
          "float8pl",
          &PGArithFuncGenerator<float8, float8, float8>::AddWithOverflow,
          nullptr,
          true));

  supported_function_[219] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<float8, float8, float8>(
          219,
          "float8mi",
          &PGArithFuncGenerator<float8, float8, float8>::SubWithOverflow,
          nullptr,
          true));

  supported_function_[1088] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGIRBuilderFuncGenerator<bool, int32_t, int32_t>(
          1088, "date_le", &IRBuilder<>::CreateICmpSLE,
          true));

  supported_function_[2339] = std::unique_ptr<PGFuncGeneratorInterface>(
      new PGGenericFuncGenerator<bool, int32_t, int64_t>(
          2339,
          "date_le_timestamp",
          &PGDateFuncGenerator::DateLETimestamp,
          nullptr,
          true));
}

PGFuncGeneratorInterface* OpExprTreeGenerator::GetPGFuncGenerator(
      unsigned int oid) {
  InitializeSupportedFunction();
  auto itr = supported_function_.find(oid);
  if (itr == supported_function_.end()) {
    return nullptr;
  }
  return itr->second.get();
}

OpExprTreeGenerator::OpExprTreeGenerator(
    const ExprState* expr_state,
    std::vector<
        std::unique_ptr<ExprTreeGenerator>>&& arguments)  // NOLINT(build/c++11)
    :  ExprTreeGenerator(expr_state, ExprTreeNodeType::kOperator),
       arguments_(std::move(arguments)) {
}

bool OpExprTreeGenerator::VerifyAndCreateExprTree(
    const ExprState* expr_state,
    ExprTreeGeneratorInfo* gen_info,
    std::unique_ptr<ExprTreeGenerator>* expr_tree) {
  assert(nullptr != expr_state &&
         nullptr != expr_state->expr &&
         T_OpExpr == nodeTag(expr_state->expr) &&
         nullptr != expr_tree);

  OpExpr* op_expr = reinterpret_cast<OpExpr*>(expr_state->expr);
  expr_tree->reset(nullptr);
  PGFuncGeneratorInterface* pg_func_gen = GetPGFuncGenerator(op_expr->opfuncid);
  if (nullptr == pg_func_gen) {
    elog(DEBUG1, "Unsupported operator %d.", op_expr->opfuncid);
    return false;
  }

  List *arguments = reinterpret_cast<const FuncExprState*>(expr_state)->args;
  assert(nullptr != arguments);
  // In ExecEvalFuncArgs
  assert(list_length(arguments) ==
        static_cast<int>(pg_func_gen->GetTotalArgCount()));

  ListCell   *arg = nullptr;
  bool supported_tree = true;
  std::vector<std::unique_ptr<ExprTreeGenerator>> expr_tree_arguments;
  foreach(arg, arguments) {
    // retrieve argument's ExprState
    ExprState  *argstate = reinterpret_cast<ExprState*>(lfirst(arg));
    assert(nullptr != argstate);
    std::unique_ptr<ExprTreeGenerator> arg(nullptr);
    supported_tree &= ExprTreeGenerator::VerifyAndCreateExprTree(argstate,
                                                                 gen_info,
                                                                 &arg);
    if (!supported_tree) {
      break;
    }
    assert(nullptr != arg);
    expr_tree_arguments.push_back(std::move(arg));
  }
  if (!supported_tree) {
    return supported_tree;
  }
  expr_tree->reset(new OpExprTreeGenerator(expr_state,
                                           std::move(expr_tree_arguments)));
  return true;
}

bool OpExprTreeGenerator::GenerateCode(GpCodegenUtils* codegen_utils,
                                       const ExprTreeGeneratorInfo& gen_info,
                                       llvm::Value** llvm_out_value,
                                       llvm::Value* const llvm_isnull_ptr) {
  assert(nullptr != llvm_out_value);
  *llvm_out_value = nullptr;
  OpExpr* op_expr = reinterpret_cast<OpExpr*>(expr_state()->expr);
  CodeGenFuncMap::iterator itr =  supported_function_.find(op_expr->opfuncid);
  auto irb = codegen_utils->ir_builder();

  if (itr == supported_function_.end()) {
    // Operators are stored in pg_proc table.
    // See postgres.bki for more details.
    elog(WARNING, "Unsupported operator %d.", op_expr->opfuncid);
    return false;
  }

  // Get the interface to generate code for operator function
  PGFuncGeneratorInterface* pg_func_interface = itr->second.get();
  assert(nullptr != pg_func_interface);

  if (arguments_.size() != pg_func_interface->GetTotalArgCount()) {
    elog(WARNING, "Expected argument size to be %lu\n",
         pg_func_interface->GetTotalArgCount());
    return false;
  }
  bool arg_generated = true;
  std::vector<llvm::Value*> llvm_arguments;
  std::vector<llvm::Value*> llvm_arguments_isNull;
  for (auto& arg : arguments_) {
    llvm::Value* llvm_arg_isnull_ptr = irb->CreateAlloca(
        codegen_utils->GetType<bool>(), nullptr, "isNull");
    irb->CreateStore(codegen_utils->GetConstant<bool>(false),
                     llvm_arg_isnull_ptr);

    llvm::Value* llvm_arg = nullptr;
    arg_generated &= arg->GenerateCode(codegen_utils,
                                       gen_info,
                                       &llvm_arg,
                                       llvm_arg_isnull_ptr);
    if (!arg_generated) {
      return false;
    }
    llvm_arguments.push_back(llvm_arg);
    llvm_arguments_isNull.push_back(irb->CreateLoad(llvm_arg_isnull_ptr));
  }
  llvm::Value* llvm_op_value = nullptr;
  PGFuncGeneratorInfo pg_func_info(gen_info.llvm_main_func,
                                   gen_info.llvm_error_block,
                                   llvm_arguments,
                                   llvm_arguments_isNull);

  bool retval = pg_func_interface->GenerateCode(codegen_utils,
                                                pg_func_info,
                                                &llvm_op_value,
                                                llvm_isnull_ptr);

  // convert return type to Datum
  *llvm_out_value = codegen_utils->CreateCppTypeToDatumCast(llvm_op_value);
  return retval;
}
