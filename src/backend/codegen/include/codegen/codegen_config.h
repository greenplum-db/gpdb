//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    codegen_config.h
//
//  @doc:
//    Configure Class to handle gucs in gpdb
//
//---------------------------------------------------------------------------
#ifndef GPCODEGEN_CODEGEN_CONFIG_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_CODEGEN_CONFIG_H_

extern "C" {
// Variables that are defined in guc
extern bool init_codegen;
extern bool codegen;
extern bool codegen_validate_functions;
extern bool codegen_exec_variable_list;
extern bool codegen_slot_getattr;
extern bool codegen_exec_eval_expr;
extern bool codegen_advance_aggregate;
// TODO(shardikar): Retire this GUC after performing experiments to find the
// tradeoff of codegen-ing slot_getattr() (potentially by measuring the
// difference in the number of instructions) when one of the first few
// attributes is varlen.
extern int codegen_varlen_tolerance;
}

namespace gpcodegen {

/** \addtogroup gpcodegen
 *  @{
 */

// Forward declaration
class ExecVariableListCodegen;
class SlotGetAttrCodegen;
class ExecEvalExprCodegen;
class AdvanceAggregatesCodegen;

class CodegenConfig {
 public:
  /**
   * @brief Template function to check if generator enabled.
   *
   * @tparam ClassType Type of Code Generator class
   *
   * @return true if respective generator is enabled with gpdb guc.
   **/
  template <class ClassType>
  inline static bool IsGeneratorEnabled();
};

template<>
inline bool CodegenConfig::IsGeneratorEnabled<ExecVariableListCodegen>() {
  return codegen_exec_variable_list;
}

template<>
inline bool CodegenConfig::IsGeneratorEnabled<SlotGetAttrCodegen>() {
  return codegen_slot_getattr;
}

template<>
inline bool CodegenConfig::IsGeneratorEnabled<ExecEvalExprCodegen>() {
  return codegen_exec_eval_expr;
}

template<>
inline bool CodegenConfig::IsGeneratorEnabled<AdvanceAggregatesCodegen>() {
  return codegen_advance_aggregate;
}


/** @} */

}  // namespace gpcodegen

#endif  // GPCODEGEN_CODEGEN_CONFIG_H_
