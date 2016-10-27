//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    expr_tree_generator.cc
//
//  @doc:
//    Base class for expression tree to generate code
//
//---------------------------------------------------------------------------
#include <cassert>
#include <memory>

#include "codegen/const_expr_tree_generator.h"
#include "codegen/expr_tree_generator.h"
#include "codegen/op_expr_tree_generator.h"
#include "codegen/var_expr_tree_generator.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#include "nodes/execnodes.h"
#include "utils/elog.h"
#include "nodes/nodes.h"
}

using gpcodegen::ExprTreeGenerator;

bool ExprTreeGenerator::VerifyAndCreateExprTree(
    const ExprState* expr_state,
    ExprTreeGeneratorInfo* gen_info,
    std::unique_ptr<ExprTreeGenerator>* expr_tree) {
  assert(nullptr != expr_state &&
         nullptr != expr_state->expr &&
         nullptr != expr_tree);

  if (expr_state->type != T_FuncExprState &&
      expr_state->type != T_ExprState) {
    elog(DEBUG1, "Input expression state type (%d) is not supported",
         expr_state->type);
    return false;
  }
  expr_tree->reset(nullptr);
  bool supported_expr_tree = false;
  switch (nodeTag(expr_state->expr)) {
    case T_OpExpr: {
      supported_expr_tree = OpExprTreeGenerator::VerifyAndCreateExprTree(
          expr_state, gen_info, expr_tree);
      break;
    }
    case T_Var: {
      supported_expr_tree = VarExprTreeGenerator::VerifyAndCreateExprTree(
          expr_state, gen_info, expr_tree);
      break;
    }
    case T_Const: {
      supported_expr_tree = ConstExprTreeGenerator::VerifyAndCreateExprTree(
          expr_state, gen_info, expr_tree);
      break;
    }
    default : {
      supported_expr_tree = false;
      elog(DEBUG1, "Unsupported expression tree %d found",
           nodeTag(expr_state->expr));
    }
  }
  assert((!supported_expr_tree && nullptr == expr_tree->get()) ||
         (supported_expr_tree && nullptr != expr_tree->get()));
  return supported_expr_tree;
}
