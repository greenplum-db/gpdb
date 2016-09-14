//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    codegen_utils.h
//
//  @doc:
//    Object that manages runtime code generation for a single LLVM module.
//
//  @test:
//
//
//---------------------------------------------------------------------------

#ifndef GPCODEGEN_CODEGEN_UTILS_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_CODEGEN_UTILS_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <vector>

#include "codegen/utils/annotated_type.h"
#include "codegen/utils/macros.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/Utils/Cloning.h"

namespace gpcodegen {

// Forward declaration of helper class for friending purposes.
namespace codegen_utils_detail {
template <typename, typename> class ConstantMaker;
template <typename> class FunctionTypeUnpacker;
template <typename, typename> class ArithOpMaker;
}  // namespace codegen_utils_detail


// Easy static check to match integer macros with enum equivalents
#define STATIC_ASSERT_OPTIMIZATION_LEVEL(ename, mname) \
    static_assert(gpcodegen::CodegenUtils::OptimizationLevel::ename == \
    gpcodegen::CodegenUtils::OptimizationLevel(mname), \
    "gpcodegen::CodegenUtils::OptimizationLevel::" #ename  \
    " == " #mname)


/** \addtogroup gpcodegen
 *  @{
 */

/**
 * @brief Object that manages runtime code generation for a single LLVM module.
 **/
class CodegenUtils {
 public:
  enum class OptimizationLevel : unsigned {
    kNone = 0,   // -O0
    kLess,       // -O1
    kDefault,    // -O2
    kAggressive  // -O3
  };

  enum class SizeLevel : unsigned {
    kNormal = 0,  // Default
    kSmall,       // -Os
    kTiny         // -Oz
  };

  /**
   * @brief Constructor.
   *
   * @param module_name A human-readable name for the module that this
   *        CodegenUtils will manage.
   **/
  explicit CodegenUtils(llvm::StringRef module_name);

  virtual ~CodegenUtils() {
  }

  /**
   * @brief Initialize global LLVM state (in particular, information about the
   *        host machine which will be the target for code generation). Must be
   *        called at least once before creating CodegenUtils objects.
   *
   * @return true if initilization was successful, false if some part of
   *         initialization failed.
   **/
  static bool InitializeGlobal();

  /**
   * @return An IRBuilder object that can be used to generate IR instructions.
   **/
  llvm::IRBuilder<>* ir_builder() {
    return &ir_builder_;
  }

  /**
   * @return The LLVM Module that is managed by this CodegenUtils, or NULL if
   *         PrepareForExecution() has already been called.
   *
   * @note When a CodegenUtils is initially created, a Module is created with
   *       it and it is accessible via this method. The Module is mutable and
   *       new code can be inserted into it UNTIL PrepareForExecution() is
   *       called, at which point the Module's code is "frozen" and can no
   *       longer be modified, and this method returns NULL.
   **/
  llvm::Module* module() {
    return module_.get();
  }

  /**
   * @brief Get a C++ type's equivalent in the LLVM type system.
   *
   * @note Because of some differences between the C++ and LLVM type systems,
   *       some caveats about using this method apply:
   *       1. LLVM's type system has no notion of const-ness, so const and/or
   *          volatile-qualified versions of a C++ type are treated the same as
   *          the base type.
   *       2. In LLVM, the convention for generic "untyped" pointers is to
   *          represent them as i8* (a pointer to an 8-bit integer, equivalent
   *          to char* in C++). The method respects that convention, and
   *          returns the LLVM i8* type when called with CppType = void* or
   *          some cv-qualified version thereof.
   *       3. C++ enums are converted to their underlying integer type.
   *       4. C++ references are converted to pointers (this is how references
   *          are implemented under-the-covers in C++ anyway).
   *       5. Pointers or references to classes or structs are converted to
   *          generic untyped pointers (i8* in LLVM, equivalent to void* in
   *          C++).
   *
   * @tparam CppType a C++ type to map to an LLVM type.
   * @return A pointer to CppType's equivalent LLVM type in this
   *         CodegenUtils's context.
   **/
  template <typename CppType>
  llvm::Type* GetType();

  /**
   * @brief Get a C++ type's equivalent in the LLVM type system with additional
   *        annotations to capture C++ type properties that are not reflected by
   *        the LLVM type.
   *
   * @tparam CppType A C++ type to map to an LLVM type with annotations.
   * @return An AnnotatedType object that contains a pointer to CppType's
   *         equivalent LLVM type along with annotations capturing additional
   *         properties of CppType.
   **/
  template <typename CppType>
  AnnotatedType GetAnnotatedType();

  /**
   * @brief Get a function's type-signature as an LLVM FunctionType.
   *
   * @note The ReturnType and all ArgumentTypes are converted according to the
   *       same rules as GetType().
   *
   * @tparam ReturnType The function's return type.
   * @tparam ArgumentTypes The types of any number of arguments to the function.
   * @param  is_var_arg Whether the function has trailing variable arguments list
   * @return A pointer to the complete function type-signature's equivalent as
   *         an LLVM FunctionType in this CodegenUtils's context.
   **/
  template <typename ReturnType, typename... ArgumentTypes>
  llvm::FunctionType* GetFunctionType(const bool is_var_arg = false);

  /**
   * @brief Get an LLVM constant based on a C++ value.
   *
   * @note Unique LLVM constants are created on-demand and have the same
   *       lifetime as the LLVMContext that they were created in.
   * @note If constant_value is an lvalue-reference, then the llvm::Constant
   *       produced will be based on the referent value, NOT the reference
   *       itself. If reference semantics are desired, then convert to a
   *       pointer first by taking the address of the reference with '&'.
   *
   * @param constant_value A C++ value to map to an LLVM constant.
   * @return A pointer to an llvm::Constant object with constant_value's value
   *         in this CodegenUtils's context.
   **/
  template <typename CppType>
  llvm::Constant* GetConstant(const CppType constant_value);

  /**
   * @brief Get a pointer to a member variable in a struct (or a public member
   *        variable in a class), generating instructions at the IRBuilder's
   *        current insertion point.
   *
   * @note In most cases, the caller does not need to explicitly specify
   *       template parameters, as they can be inferred automatically based on
   *       pointers_to_members.
   * @note This method only does address computation and returns an LLVM pointer
   *       to the desired member variable in a struct. It does not actually
   *       dereference any pointers or generate any load instructions. To obtain
   *       the actual value held in the member variable, simply create a load
   *       instruction with the returned pointer as the argument.
   *
   * @tparam PointerToMemberTypes Any number of pointer-to-member types of the
   *         form MemberType StructType::*
   * @param base_ptr A pointer to an instance of a top-level struct or class to
   *        access into.
   * @param pointers_to_members Any number of pointer-to-members of the form
   *        MemberType StructType::*. This allows accessing members of nested
   *        structs arbitrarily deep in a single call.
   * @return A pointer to the member denoted by pointers_to_members in the
   *         struct indicated by base_ptr.
   **/
  template <typename... PointerToMemberTypes>
  llvm::Value* GetPointerToMember(
      llvm::Value* base_ptr,
      PointerToMemberTypes&&... pointers_to_members) {
    return GetPointerToMemberImpl(
        base_ptr,
        nullptr,  // cast_type
        0,        // cumulative_offset
        std::forward<PointerToMemberTypes>(pointers_to_members)...);
  }

  /**
   * @brief Create an LLVM function in the module managed by this
   *        CodegenUtils.
   *
   * @note This method creates an empty Function object with no body. Caller
   *       can then create BasicBlocks inside the Function to implement its
   *       actual body.
   *
   * @tparam FuncType FunctionType. e.g ReturnType (*)(ArgumenTypes)
   *
   * @param name The function's name.
   * @param is_var_arg Whether the function has trailing variable arguments list
   * @param linkage The linkage visibility of the function. Defaults to
   *        ExternalLinkage, which makes the function visible and callable from
   *        anywhere.
   * @return A pointer to a newly-created empty function in this
   *         CodegenUtils's module (object is owned by this CodegenUtils).
   **/
  template <typename FuncType>
  llvm::Function* CreateFunction(
      const llvm::Twine& name,
      const bool is_var_arg = false,
      const llvm::GlobalValue::LinkageTypes linkage
          = llvm::GlobalValue::ExternalLinkage);

  /**
   * @brief Create a new BasicBlock inside a Function in this CodegenUtils's
   *        module.
   *
   * @param name The name of the BasicBlock. This is not required to be unique,
   *        and may be an empty string for an anonymous BasicBlock.
   * @param parent The Function that the BasicBlock should be created in. The
   *        first BasicBlock created in a Function is that Function's entry
   *        point, and any subsequently created BasicBlocks are entered via
   *        instructions like 'br' or 'switch'.
   * @return A new BasicBlock inside parent.
   **/
  llvm::BasicBlock* CreateBasicBlock(const llvm::Twine& name,
                                     llvm::Function* parent) {
#ifdef CODEGEN_DEBUG
    // DEBUG-only assertion to make sure that caller does not try to generate IR
    // for an external function.
    assert(!parent->getName().startswith(kExternalFunctionNamePrefix));
#endif  // CODEGEN_DEBUG
    return llvm::BasicBlock::Create(context_, name, parent, nullptr);
  }

  /**
   * @brief Use LLVM intrinsic to create Multiplication with overflow
   *        instruction
   *
   * @tparam CppType  CppType for multiplication
   * @param arg0  First argument
   * @param arg1  Second argument
   *
   * @return LLVM Value as a pair of results and overflow flag.
   **/
  template <typename CppType>
  llvm::Value* CreateMulOverflow(llvm::Value* arg0, llvm::Value* arg1);

  /**
   * @brief Use LLVM intrinsic to create Add with overflow
   *        instruction
   *
   * @tparam CppType  CppType for add
   * @param arg0  First argument
   * @param arg1  Second argument
   *
   * @return LLVM Value as a pair of results and overflow flag.
   **/
  template <typename CppType>
  llvm::Value* CreateAddOverflow(llvm::Value* arg0, llvm::Value* arg1);

  /**
     * @brief Use LLVM intrinsic to increase the value of a variable by one
     *        with overflow instruction
     *
     * @tparam CppType  CppType for add
     * @param arg   Input variable
     *
     * @return LLVM Value as a pair of results and overflow flag.
     **/
  template <typename CppType>
  llvm::Value* CreateIncOverflow(llvm::Value* arg);

  /**
   * @brief Use LLVM intrinsic to create Subtract with overflow
   *        instruction
   *
   * @tparam CppType  CppType for subtract
   * @param arg0  First argument
   * @param arg1  Second argument
   *
   * @return LLVM Value as a pair of results and overflow flag.
   **/
  template <typename CppType>
  llvm::Value* CreateSubOverflow(llvm::Value* arg0, llvm::Value* arg1);

  /**
   * @brief Cast given llvm::value to DestType.
   *
   * @tparam DestType  Destination type
   * @tparam SrcType   Source type
   * @param value  Value that needs to be casted. Expected to be of same type as
   *               SrcType
   *
   * @return LLVM Value of DestType
   **/
  template <typename DestType, typename SrcType>
  llvm::Value* CreateCast(llvm::Value* value);

  /**
   * @brief Register an external function if previously unregistered. Otherwise
   *        return a pointer to the previously registered llvm::Function
   *
   * @warning This method returns a pointer to an llvm::Function object. The
   *          caller should NOT attempt to add BasicBlocks to the function, as
   *          that would cause conflicts when mapping the function to its
   *          external implementation during PrepareForExecution().
   *
   * @tparam ReturnType The return type of the external_function. This does not
   *         need to be specified if external_function is not overloaded (it
   *         will be inferred automatically).
   * @tparam argument_types The types of the arguments to external_function.
   *         These do not need to be specified if external_function is not
   *         overloaded (they will be inferred automatically).
   * @param external_function A function pointer to install for use in this
   *        CodegenUtils.
   * @param name An optional name to refer to the external function by. If
   *        non-empty, this CodegenUtils will record additional information
   *        so that the registered function will also be callable by its name
   *        in C++ source code compiled by ClangCompiler (see
   *        ClangCompiler::GenerateExternalFunctionDeclarations()).
   * @param is_var_arg Whether the function has trailing variable arguments list
   * @return A callable LLVM function.
   */
  template <typename ReturnType, typename... ArgumentTypes>
  llvm::Function* GetOrRegisterExternalFunction(
      ReturnType (*external_function)(ArgumentTypes...),
      const std::string& name = "",
      const bool is_var_arg = false) {
    std::unordered_map<std::uint64_t, std::string>::iterator it;
    bool key_absent;

    std::tie(it, key_absent) = external_functions_.emplace(
        reinterpret_cast<std::uint64_t>(external_function),
        name.empty() ? GenerateExternalFunctionName()
                     : name);

    if (!key_absent) {
      return module()->getFunction(it->second);
    } else {
      if (!name.empty()) {
        RecordNamedExternalFunction<ReturnType, ArgumentTypes...>(name);
      }
      return CreateFunctionImpl<ReturnType, ArgumentTypes...>(
          it->second, is_var_arg);
    }
  }

  template <typename ReturnType, typename... ArgumentTypes>
  llvm::Function* GetOrRegisterExternalFunction(
      ReturnType (*external_function)(ArgumentTypes..., ...),
      const std::string& name = "") {
    return GetOrRegisterExternalFunction(
        reinterpret_cast<ReturnType (*)(ArgumentTypes...)>(external_function),
        name,
        true);
  }

  /**
   * @brief Optimize the code in the module managed by this CodegenUtils before
   *        execution.
   *
   * This method applies "generic" IR-to-IR optimization passes and is intended
   * to be run before the final code-generation in PrepareForExecution(). It
   * runs both function-level and whole module-level optimization passes.
   *
   * @note It is recommended to run this method just once before calling
   *       PrepareForExecution().
   *
   * @param generic_opt_level How aggressively to optimize code. Higher levels
   *        will enable more computationally expensive optimization passes that
   *        may produce better-optimized code at the cost of increased up-front
   *        optimization time.
   * @param size_level How aggressively to attempt to reduce the total size of
   *        code. Higher levels (especially kTiny) may actually lead to
   *        worse-performing code because they prioritize size-reduction over
   *        performance.
   * @param optimize_for_host_cpu If true, attempt to exploit information about
   *        the specific CPU model we are running on during optimization (for
   *        example, LLVM may vectorize code based on what SIMD instructions are
   *        available).
   * @return true if optimization was successful, false if some error occured.
   **/
  bool Optimize(const OptimizationLevel generic_opt_level,
                const SizeLevel size_level,
                const bool optimize_for_host_cpu);

  /**
   * @brief Prepare code generated by this CodegenUtils for execution.
   *
   * Internally, this creates an LLVM MCJIT ExecutionEngine and gives ownership
   * of the Module to it. Actual compilation of functions may be deferred until
   * the first call to GetUntypedFunctionPointer().
   *
   * @note The cpu_opt_level and optimize_for_host_cpu options for this method
   *       only affect the actual generation of machine code. See also the
   *       Optimize() method, which controls more general code transformations.
   *
   * @param cpu_opt_level The level of optimization to apply when translating
   *        LLVM IR to machine code. Higher levels may produce better optimized
   *        code at the expense of increased compilation time.
   * @param optimize_for_host_cpu If true, LLVM will optimize generated machine
   *        code for the specific CPU model we are running on.
   * @return true if an ExecutionEngine was set up successfully, false if some
   *         error occured.
   **/
  bool PrepareForExecution(const OptimizationLevel cpu_opt_level,
                           const bool optimize_for_host_cpu);

  /**
   * @brief Get a pointer to the compiled machine-code version of a function
   *        generated by this CodegenUtils.
   *
   * @note PrepareForExecution() should be called before calling this method.
   *
   * @tparam ReturnType The function's return type.
   * @tparam ArgumentTypes The types of any number of arguments to the function.
   * @param function_name The name of the function to retrieve.
   * @return A pointer to the compiled version of the function specified by
   *         function_name, or NULL if the function is not found or couldn't
   *         be compiled.
   **/
  template <typename FunctionType>
  FunctionType GetFunctionPointer(const std::string& function_name);


  /**
    * @brief Generate the commonly used "fallback case" that generates a call to
    * the regular_function passing all parameters from generated_function.
    *
    * @tparam FuncType FunctionType. e.g ReturnType (*)(ArgumenTypes)
    * @param regular_function     LLVM Function pointer to the regular fallback function
    * @param generated_function   LLVM Function pointer to the function being generated
    *
    **/
  template<typename FunctionType>
  void CreateFallback(llvm::Function* regular_function,
                      llvm::Function* generated_function);

  /*
   * @brief Force inline an LLVM function at the call site. Note that this only
   *        does one level of inlining.
   *
   * @param LLVM call Instruction where the called function should be inlined
   * @return false if it is not possible to inline. true otherwise.
   */
  bool InlineFunction(llvm::CallInst* call_inst) {
    // Check that the call instruction belongs to a BasicBlock which is part of
    // a valid function
    if (!(call_inst && call_inst->getParent()
          && call_inst->getParent()->getParent())) {
      return false;
    }
    llvm::InlineFunctionInfo info;
    return llvm::InlineFunction(llvm::CallSite(call_inst), info);
  }

  /*
   * @brief Dump the IR of all underlying LLVM modules.
   *
   * @param out  Stream to send the output to - as required by llvm::Value::print
   */
  void PrintUnderlyingModules(llvm::raw_ostream& out); // NOLINT

 protected:
  /**
   * @return LLVMContext
   **/
  llvm::LLVMContext* context() {
    return &context_;
  }

 private:
  // Give ClangCompiler access to 'context_' add allow it to add compiled C++
  // sources to 'auxiliary_modules_'.
  friend class ClangCompiler;

  template <typename, typename>
  friend class codegen_utils_detail::ConstantMaker;

  template <typename, typename>
  friend class codegen_utils_detail::ArithOpMaker;

  template <typename>
  friend class codegen_utils_detail::FunctionTypeUnpacker;

  // Allow ClangCompilerTest to inspect 'auxiliary_modules_' to check if they
  // have debugging information attached.
  friend class ClangCompilerTest;

  // Records an external function's name and annotated type-signature so that a
  // declaration for the function can be recreated by ClangCompiler.
  struct NamedExternalFunction {
    std::string name;
    AnnotatedType return_type;
    std::vector<AnnotatedType> argument_types;
  };

  static constexpr char kExternalVariableNamePrefix[] = "_gpcodegenv";
  static constexpr char kExternalFunctionNamePrefix[] = "_gpcodegenx";

  // Used internally when CreateFunction is called. Given the ReturnType
  // and ArgumentTypes this will create an LLVM functions in the module
  // managed by this CodegenUtils.
  template <typename ReturnType, typename... ArgumentTypes>
  llvm::Function* CreateFunctionImpl(
      const llvm::Twine& name,
      const bool is_var_arg = false,
      const llvm::GlobalValue::LinkageTypes linkage
          = llvm::GlobalValue::ExternalLinkage);

  // Used internall when GetFunctionPointer is called. Given the ReturnType
  // and ArgumentTypes this will get a pointer to the compiled machine-code
  // version of a function generated by this CodegenUtils.
  template <typename ReturnType, typename... ArgumentTypes>
  auto GetFunctionPointerImpl(const std::string& function_name)
      -> ReturnType (*)(ArgumentTypes...);

  // Used internally when GetConstant() is called with a pointer type. Creates a
  // new GlobalVariable with external linkage in '*module_' and remembers its
  // name and address in 'external_global_variables_'. GlobalVariables created
  // by this method are eventually mapped to their actual addresses in memory
  // by PrepareForExecution().
  llvm::GlobalVariable* AddExternalGlobalVariable(llvm::Type* type,
                                                  const void* address);

  // Check that a function with the specified 'function_name' has the
  // 'expected_function_type', crashing with a failed assertion if it does not.
  // Calling this method can be slow (particularly after calling
  // PrepareForExecution()), so it is intended mainly to be used in DEBUG
  // builds.
  void CheckFunctionType(const std::string& function_name,
                         const llvm::FunctionType* expected_function_type);

  // Generate a unique name for an external global variable.
  std::string GenerateExternalVariableName();

  // Generate a unique name for an external function.
  std::string GenerateExternalFunctionName();

  // Helper method for GetPointerToMember(). This base version does the actual
  // address computation and casting.
  llvm::Value* GetPointerToMemberImpl(llvm::Value* base_ptr,
                                      llvm::Type* cast_type,
                                      const std::size_t cumulative_offset);

  // Helper method to call any llvm intrinsic feature. E.g. CreateMulOverflow.
  // TODO(krajaramn) : Support any number of arguments
  llvm::Value* CreateIntrinsicInstrCall(llvm::Intrinsic::ID Id,
                                        llvm::ArrayRef<llvm::Type*> Tys,
                                        llvm::Value* arg0,
                                        llvm::Value* arg1);

  // Helper method for GetPointerToMember(). This variadic template recursively
  // consumes pointers-to-members, adding up 'cumulative_offset' and resolving
  // 'MemberType' to '*cast_type' at the penultimate level of recursion before
  // calling the base version above.
  template <typename StructType,
            typename MemberType,
            typename... TailPointerToMemberTypes>
  llvm::Value* GetPointerToMemberImpl(
      llvm::Value* base_ptr,
      llvm::Type* cast_type,
      const std::size_t cumulative_offset,
      MemberType StructType::* pointer_to_member,
      TailPointerToMemberTypes&&... tail_pointers_to_members);

  // Helper method for RegisterExternalFunction() that appends an entry for an
  // external function to 'named_external_functions_'.
  template <typename ReturnType, typename... ArgumentTypes>
  void RecordNamedExternalFunction(const std::string& name);

  llvm::LLVMContext context_;
  llvm::IRBuilder<> ir_builder_;

  // Primary module directly managed by this CodegenUtils.
  std::unique_ptr<llvm::Module> module_;

  // Additional modules to codegen from, generated by tools like ClangCompiler.
  std::vector<std::unique_ptr<llvm::Module>> auxiliary_modules_;

  std::unique_ptr<llvm::ExecutionEngine> engine_;

  // Map of (address, function_name) for each external function registered by
  // GetOrRegisterExternalFunction(). PrepareForExecution() adds a mapping for
  // each such function to '*engine_' after creating it.
  std::unordered_map<std::uint64_t, std::string> external_functions_;

  // Keep track of additional information about named external functions so that
  // ClangCompiler can reconstruct accurate declarations for them.
  std::vector<NamedExternalFunction> named_external_functions_;

  // Pairs of (variable_name, address) for external GlobalVariables created by
  // AddExternalGlobalVariable(). PrepareForExecution() adds a mapping for each
  // such GlobalVariable to '*engine_' after creating it.
  std::vector<std::pair<const std::string, const std::uint64_t>>
      external_global_variables_;

  // Counters for external variables/functions registered in this CodegenUtils.
  // Used by GenerateExternalVariableName() and GenerateExternalFunctionName(),
  // respectively, to generate unique names for functions/globals.
  unsigned external_variable_counter_;
  unsigned external_function_counter_;

  DISALLOW_COPY_AND_ASSIGN(CodegenUtils);
};


/** @} */

// ----------------------------------------------------------------------------
// Implementation of CodegenUtils::GetType() and
// CodegenUtils::GetAnnotatedType().

// Because function template partial specialization is not allowed, we use
// helper classes to implement GetType() and GetAnnotatedType(). They are
// encapsulated in this nested namespace, which is not considered part of the
// public API.
namespace codegen_utils_detail {

// type_traits-style template that detects whether 'T' is bool, or a const
// and/or volatile qualified version of bool.
template <typename T>
using is_bool = std::is_same<bool, typename std::remove_cv<T>::type>;

// TypeMaker has various template specializations to handle different
// categories of C++ types. The specializations provide a static method Get()
// that takes an 'llvm::LLVMContext*' pointer as an argument and returns an
// 'llvm::Type*' pointer that points to the equivalent of 'CppType' in LLVM's
// type system. Specializations also provide a static method GetAnnotated()
// that returns an AnnotatedType with additional information about properties of
// the C++ type. The base version of this template is empty.
template <typename CppType, typename Enable = void>
class TypeMaker {
};

// Explicit specialization for void.
template <>
class TypeMaker<void> {
 public:
  static llvm::Type* Get(llvm::LLVMContext* context) {
    return llvm::Type::getVoidTy(*context);
  }

  static AnnotatedType GetAnnotated(llvm::LLVMContext* context) {
    return AnnotatedType::CreateScalar<void>(Get(context));
  }
};

// NOTE: LLVM's type system does not have a notion of const-ness, so the const
// and mutable versions of a C++ type map to the same LLVM type. Instead of
// explicit specialization for bool, float, and double below, we use partial
// specialization with some type_traits magic to capture all the const and
// volatile-qualified versions of such a type in a single template.

// Specialization for bool (treated as a 1-bit integer in LLVM IR).
template <typename BoolType>
class TypeMaker<
    BoolType,
    typename std::enable_if<is_bool<BoolType>::value>::type> {
 public:
  static llvm::Type* Get(llvm::LLVMContext* context) {
    return llvm::Type::getInt1Ty(*context);
  }

  static AnnotatedType GetAnnotated(llvm::LLVMContext* context) {
    return AnnotatedType::CreateScalar<BoolType>(Get(context));
  }
};

// Specialization for 32-bit float.
template <typename FloatType>
class TypeMaker<
    FloatType,
    typename std::enable_if<std::is_same<
        float,
        typename std::remove_cv<FloatType>::type>::value>::type> {
 public:
  static llvm::Type* Get(llvm::LLVMContext* context) {
    return llvm::Type::getFloatTy(*context);
  }

  static AnnotatedType GetAnnotated(llvm::LLVMContext* context) {
    return AnnotatedType::CreateScalar<FloatType>(Get(context));
  }
};

// Specialization for 64-bit double.
template <typename DoubleType>
class TypeMaker<
    DoubleType,
    typename std::enable_if<std::is_same<
        double,
        typename std::remove_cv<DoubleType>::type>::value>::type> {
 public:
  static llvm::Type* Get(llvm::LLVMContext* context) {
    return llvm::Type::getDoubleTy(*context);
  }

  static AnnotatedType GetAnnotated(llvm::LLVMContext* context) {
    return AnnotatedType::CreateScalar<DoubleType>(Get(context));
  }
};

// Partial specialization for integer types other than bool.
template <typename IntType>
class TypeMaker<
    IntType,
    typename std::enable_if<std::is_integral<IntType>::value
                            && !is_bool<IntType>::value>::type> {
 public:
  static llvm::Type* Get(llvm::LLVMContext* context) {
    return llvm::Type::getIntNTy(*context, sizeof(IntType) << 3);
  }

  static AnnotatedType GetAnnotated(llvm::LLVMContext* context) {
    return AnnotatedType::CreateScalar<IntType>(Get(context));
  }
};

// Partial specialization for enums, which are converted to their underlying
// integer representation.
template <typename EnumType>
class TypeMaker<
    EnumType,
    typename std::enable_if<std::is_enum<EnumType>::value>::type> {
 public:
  static llvm::Type* Get(llvm::LLVMContext* context) {
    return TypeMaker<typename std::underlying_type<EnumType>::type>::Get(
        context);
  }

  static AnnotatedType GetAnnotated(llvm::LLVMContext* context) {
    return AnnotatedType::CreateScalar<EnumType>(Get(context));
  }
};

// Partial specialization for pointers to non-class, non-void types.
template <typename PtrType>
class TypeMaker<
    PtrType,
    typename std::enable_if<
        std::is_pointer<PtrType>::value
        && !std::is_class<typename std::remove_pointer<PtrType>::type>::value
        && !std::is_void<typename std::remove_pointer<PtrType>::type>::value>
            ::type> {
 public:
  static llvm::Type* Get(llvm::LLVMContext* context) {
    return TypeMaker<typename std::remove_pointer<PtrType>::type>::Get(context)
        ->getPointerTo();
  }

  static AnnotatedType GetAnnotated(llvm::LLVMContext* context) {
    return TypeMaker<typename std::remove_pointer<PtrType>::type>
        ::GetAnnotated(context).template AddPointer<PtrType>();
  }
};

// Partial specialization for pointers to class/struct types. These are
// converted to untyped pointers (i8* in LLVM, equivalent to void* in C++).
template <typename ClassPtrType>
class TypeMaker<
    ClassPtrType,
    typename std::enable_if<
        std::is_pointer<ClassPtrType>::value
        && std::is_class<
            typename std::remove_pointer<ClassPtrType>::type>::value>::type> {
 public:
  static llvm::Type* Get(llvm::LLVMContext* context) {
    return llvm::Type::getInt8PtrTy(*context);
  }

  static AnnotatedType GetAnnotated(llvm::LLVMContext* context) {
    return AnnotatedType::CreateVoidPtr<ClassPtrType>(Get(context));
  }
};

// Special case for void*. Although it is possible to make a pointer to LLVM's
// void type, convention in LLVM is that all general-purpose "untyped" pointers
// are i8*. This partial specialization captures all the different const and/or
// volatile qualified versions of void*.
template <typename VoidPtrType>
class TypeMaker<
    VoidPtrType,
    typename std::enable_if<
        std::is_pointer<VoidPtrType>::value
        && std::is_void<
            typename std::remove_pointer<VoidPtrType>::type>::value>::type> {
 public:
  static llvm::Type* Get(llvm::LLVMContext* context) {
    return llvm::Type::getInt8PtrTy(*context);
  }

  static AnnotatedType GetAnnotated(llvm::LLVMContext* context) {
    return AnnotatedType::CreateVoidPtr<VoidPtrType>(Get(context));
  }
};

// Partial specialization for (lvalue-)references. A reference is really just
// a pointer.
template <typename ReferentType>
class TypeMaker<ReferentType&> {
 public:
  static llvm::Type* Get(llvm::LLVMContext* context) {
    return TypeMaker<ReferentType*>::Get(context);
  }

  static AnnotatedType GetAnnotated(llvm::LLVMContext* context) {
    return TypeMaker<ReferentType*>::GetAnnotated(context).ConvertToReference();
  }
};

// Partial specialization for C/C++ array types, which are converted to their
// corresponding llvm array types
template <typename ArrayElementType, size_t N>
class TypeMaker<ArrayElementType[N]> {
 public:
  static llvm::Type* Get(llvm::LLVMContext* context) {
    llvm::Type* unit_type =
      codegen_utils_detail::TypeMaker<ArrayElementType>::Get(context);
    return llvm::ArrayType::get(unit_type, N);
  }
};

}  // namespace codegen_utils_detail

template <typename CppType>
llvm::Type* CodegenUtils::GetType() {
  return codegen_utils_detail::TypeMaker<CppType>::Get(&context_);
}

template <typename CppType>
AnnotatedType CodegenUtils::GetAnnotatedType() {
  return codegen_utils_detail::TypeMaker<CppType>::GetAnnotated(&context_);
}

// ----------------------------------------------------------------------------
// Implementation of CodegenUtils::GetFunctionType().

// Helper template classes are nested in this namespace and are not considered
// part of the public API.
namespace codegen_utils_detail {

// TypeVectorBuilder is a variadic template. Specializations of
// TypeVectorBuilder have a two static methods AppendTypes() and
// AppendAnnotatedTypes(). AppendTypes() takes a 'CodegenUtils*' pointer and a
// pointer to a vector of 'llvm::Type*' pointers.
// Calling TypeVectorBuilder<ArgumentTypes...>::AppendTypes() appends the
// equivalent llvm::Type for each of 'ArgumentTypes' to the vector. Similarly,
// AppendAnnotatedTypes() appends an AnnotatedType for each of 'ArgumentTypes'
// to a vector.
template <typename... ArgumentTypes>
class TypeVectorBuilder;

// Base version for zero argument types does nothing.
template <>
class TypeVectorBuilder<> {
 public:
  static void AppendTypes(CodegenUtils* generator,
                          std::vector<llvm::Type*>* types) {
  }

  static void AppendAnnotatedTypes(
      CodegenUtils* generator,
      std::vector<AnnotatedType>* annotated_types) {
  }
};

// Version for 1+ argument types appends converts and appends the first type,
// then recurses.
template <typename HeadType, typename... TailTypes>
class TypeVectorBuilder<HeadType, TailTypes...> {
 public:
  static_assert(!std::is_same<HeadType, void>::value,
                "void is not allowed as an argument type for "
                "gpcodegen::CodegenUtils::GetFunctionType()");

  static void AppendTypes(CodegenUtils* generator,
                          std::vector<llvm::Type*>* types) {
    types->push_back(generator->GetType<HeadType>());
    TypeVectorBuilder<TailTypes...>::AppendTypes(generator, types);
  }

  static void AppendAnnotatedTypes(
      CodegenUtils* generator,
      std::vector<AnnotatedType>* annotated_types) {
    annotated_types->emplace_back(generator->GetAnnotatedType<HeadType>());
    TypeVectorBuilder<TailTypes...>::AppendAnnotatedTypes(generator,
                                                          annotated_types);
  }
};

}  // namespace codegen_utils_detail

template <typename ReturnType, typename... ArgumentTypes>
llvm::FunctionType* CodegenUtils::GetFunctionType(const bool is_var_arg) {
  std::vector<llvm::Type*> argument_types;
  codegen_utils_detail::TypeVectorBuilder<ArgumentTypes...>::AppendTypes(
      this,
      &argument_types);
  return llvm::FunctionType::get(GetType<ReturnType>(),
                                 argument_types,
                                 is_var_arg);
}

// ----------------------------------------------------------------------------
// Implementation of CodegenUtils::GetConstant().

// Helper template classes are nested in this namespace and are not considered
// part of the public API.
namespace codegen_utils_detail {

// ConstantMaker has various template specializations to handle constants of
// different C++ types. The specialized versions have a static method Get()
// that takes a 'constant_value' of type 'const CppType' and a pointer to
// a CodegenUtils object, and returns a pointer to an llvm::Constant
// equivalent to 'constant_value' in the CodegenUtils's context.
template <typename CppType, typename Enable = void>
class ConstantMaker {
};

// Partial specialization for unsigned integer types (including bool).
template <typename UnsignedIntType>
class ConstantMaker<
    UnsignedIntType,
    typename std::enable_if<
        std::is_integral<UnsignedIntType>::value
        && std::is_unsigned<UnsignedIntType>::value>::type> {
 public:
  static_assert(sizeof(UnsignedIntType) <= sizeof(std::uint64_t),
                "Unable to make an integer constant wider than 64 bits.");

  static llvm::Constant* Get(const UnsignedIntType constant_value,
                             CodegenUtils* generator) {
    return llvm::ConstantInt::get(generator->GetType<UnsignedIntType>(),
                                  constant_value);
  }
};

// Partial specialization for signed integer types.
template <typename SignedIntType>
class ConstantMaker<
    SignedIntType,
    typename std::enable_if<
        std::is_integral<SignedIntType>::value
        && std::is_signed<SignedIntType>::value>::type> {
 public:
  static_assert(sizeof(SignedIntType) <= sizeof(std::int64_t),
                "Unable to make an integer constant wider than 64 bits.");

  static llvm::Constant* Get(const SignedIntType constant_value,
                             CodegenUtils* generator) {
    return llvm::ConstantInt::getSigned(generator->GetType<SignedIntType>(),
                                        constant_value);
  }
};

// Partial specialization for enums (mapped to the underlying integer type).
template <typename EnumType>
class ConstantMaker<
    EnumType,
    typename std::enable_if<std::is_enum<EnumType>::value>::type> {
 public:
  static llvm::Constant* Get(const EnumType constant_value,
                             CodegenUtils* generator) {
    typedef typename std::underlying_type<EnumType>::type EnumAsIntType;
    return ConstantMaker<EnumAsIntType>::Get(
        static_cast<EnumAsIntType>(constant_value),
        generator);
  }
};

// Explicit specialization for 32-bit float.
template <>
class ConstantMaker<float> {
 public:
  static llvm::Constant* Get(const float constant_value,
                             CodegenUtils* generator) {
    return llvm::ConstantFP::get(generator->GetType<float>(),
                                 constant_value);
  }
};

// Explicit specialization for 64-bit double.
template <>
class ConstantMaker<double> {
 public:
  static llvm::Constant* Get(const double constant_value,
                             CodegenUtils* generator) {
    return llvm::ConstantFP::get(generator->GetType<double>(),
                                 constant_value);
  }
};

// Partial specialization for pointers.
template <typename PointedType>
class ConstantMaker<PointedType*> {
 public:
  static llvm::Constant* Get(const PointedType* constant_value,
                             CodegenUtils* generator) {
    if (constant_value == nullptr) {
      return llvm::ConstantPointerNull::get(
          static_cast<llvm::PointerType*>(generator->GetType<PointedType*>()));
    } else {
      // If pointer is non-NULL, assume it points to valid memory and make the
      // pointed-to data a global variable.
      return generator->AddExternalGlobalVariable(
          generator->GetType<PointedType*>()->getPointerElementType(),
          constant_value);
    }
  }
};

}  // namespace codegen_utils_detail

template <typename CppType>
llvm::Constant* CodegenUtils::GetConstant(const CppType constant_value) {
  return codegen_utils_detail::ConstantMaker<CppType>::Get(constant_value,
                                                            this);
}

// ----------------------------------------------------------------------------
// Implementation of recursive variadic version of
// CodegenUtils::GetPointerToMemberImpl().

template <typename StructType,
          typename MemberType,
          typename... TailPointerToMemberTypes>
llvm::Value* CodegenUtils::GetPointerToMemberImpl(
    llvm::Value* base_ptr,
    llvm::Type* cast_type,
    const std::size_t cumulative_offset,
    MemberType StructType::* pointer_to_member,
    TailPointerToMemberTypes&&... tail_pointers_to_members) {
  // Sanity check. '*cast_type' should not be set until the final level of
  // recursion.
  assert(cast_type == nullptr);

  // Calculate the offset (in bytes) of the member denoted by
  // '*pointer_to_member' inside StructType.
  const std::size_t member_offset
      = reinterpret_cast<std::size_t>(
          &(static_cast<const StructType*>(0)->*pointer_to_member));

  // If we have reached the end of pointer-to-member arguments, it is time to
  // determine the member type to cast the returned pointer to.
  if (sizeof...(tail_pointers_to_members) == 0) {
    cast_type = GetType<MemberType*>();
  }

  return GetPointerToMemberImpl(
      base_ptr,
      cast_type,
      cumulative_offset + member_offset,
      std::forward<TailPointerToMemberTypes>(tail_pointers_to_members)...);
}

// ----------------------------------------------------------------------------
// Implementation of CodegenUtils::RecordNamedExternalFunction()

template <typename ReturnType, typename... ArgumentTypes>
void CodegenUtils::RecordNamedExternalFunction(const std::string& name) {
  NamedExternalFunction named_external_fn{name,
                                          GetAnnotatedType<ReturnType>(),
                                          {}};
  codegen_utils_detail::TypeVectorBuilder<ArgumentTypes...>
      ::AppendAnnotatedTypes(this,
                             &named_external_fn.argument_types);

  named_external_functions_.emplace_back(std::move(named_external_fn));
}


// ----------------------------------------------------------------------------
// Implementation of CodegenUtils::GetTypeDefFunctionPointer &
// CodegenUtils::CreateFunctionPointer().

namespace codegen_utils_detail {
/**
 * @brief Templated helper class that provides a static method which
 *        unpack the return type, argument types and call the respective
 *        method
 *
 * @tparam FunctionType Function type.
 * @tparam MethodPtr A pointer-to-method.
 **/
template <typename FunctionType>
class FunctionTypeUnpacker;

// Partial specialization of FunctionTypeUnpacker.
// This class will unpack the function type to ReturnType and ArgumentTypes.
// GetFunctionPointerImpl - Call CodegenUtils's GetFunctionPointerImpl
// CreateFunctionImpl - Call CodegenUtils's CreateFunctionImpl.
template<typename ReturnType, typename... ArgumentTypes>
class FunctionTypeUnpacker<ReturnType(*)(ArgumentTypes...)> {
 public:
  using R = ReturnType;

  static llvm::Function* CreateFunctionImpl(
      CodegenUtils* codegen_utils,
      const llvm::Twine& name,
      const bool is_var_arg,
      const llvm::GlobalValue::LinkageTypes linkage) {
    return codegen_utils->CreateFunctionImpl<ReturnType, ArgumentTypes...>(
        name, is_var_arg, linkage);
  }

  static auto GetFunctionPointerImpl(gpcodegen::CodegenUtils* codegen_utils,
                                       const std::string& func_name)
    -> ReturnType (*)(ArgumentTypes...) {
    return codegen_utils->GetFunctionPointerImpl<ReturnType, ArgumentTypes...>(
        func_name);
  }
};
}  // namespace codegen_utils_detail

template <typename FunctionType>
llvm::Function* CodegenUtils::CreateFunction(
    const llvm::Twine& name,
    const bool is_var_arg,
    const llvm::GlobalValue::LinkageTypes linkage) {
  return codegen_utils_detail::FunctionTypeUnpacker<FunctionType>::
      CreateFunctionImpl(this, name, is_var_arg, linkage);
}

template <typename ReturnType, typename... ArgumentTypes>
llvm::Function* CodegenUtils::CreateFunctionImpl(
    const llvm::Twine& name,
    const bool is_var_arg,
    const llvm::GlobalValue::LinkageTypes linkage) {
  return llvm::Function::Create(
      GetFunctionType<ReturnType, ArgumentTypes...>(is_var_arg),
      linkage,
      name,
      module_.get());
}

template <typename FunctionType>
FunctionType CodegenUtils::GetFunctionPointer(
    const std::string& function_name) {
  return codegen_utils_detail::FunctionTypeUnpacker<FunctionType>::
      GetFunctionPointerImpl(this, function_name);
}

template <typename ReturnType, typename... ArgumentTypes>
auto CodegenUtils::GetFunctionPointerImpl(const std::string& function_name)
    -> ReturnType (*)(ArgumentTypes...) {
  if (engine_) {
#ifdef CODEGEN_DEBUG
    CheckFunctionType(function_name,
                      GetFunctionType<ReturnType, ArgumentTypes...>());
#endif
    return reinterpret_cast<ReturnType (*)(ArgumentTypes...)>(
        engine_->getFunctionAddress(function_name));
  } else {
    return nullptr;
  }
}

template <typename FunctionType>
void CodegenUtils::CreateFallback(llvm::Function* regular_function,
                                  llvm::Function* generated_function) {
  assert(regular_function != nullptr);
  assert(generated_function != nullptr);

  std::vector<llvm::Value*> forwarded_args;
  for (llvm::Argument& arg : generated_function->args()) {
    forwarded_args.push_back(&arg);
  }

  llvm::CallInst* call_fallback_func = ir_builder()->CreateCall(
      regular_function, forwarded_args);
  /* Return the result of the call, or void if the function returns void. */
  if (std::is_same<typename
      codegen_utils_detail::FunctionTypeUnpacker<FunctionType>::R,
                                                 void>::value) {
    ir_builder()->CreateRetVoid();
  } else {
    ir_builder()->CreateRet(call_fallback_func);
  }
}

// ----------------------------------------------------------------------------
// Implementation of CodegenUtils::CreateMulOverflow, CreateSubOverflow...

// Helper template classes are nested in this namespace and are not considered
// part of the public API.
namespace codegen_utils_detail {

// ArithOpMaker has various template specializations to handle different
// categories of C++ types. The specializations provide a static method
// CreateMulOverflow(), CreateAddOverflow(), CreateSubOverflow() etc..
// that takes an CodegenUtils and arguments and call respective llvm instruction
// for given CppType.
// The base version of this template is empty.
template <typename CppType, typename Enable = void>
class ArithOpMaker {
};

// Partial specialization for unsigned integer types (including bool).
template <typename UnsignedIntType>
class ArithOpMaker<
UnsignedIntType,
typename std::enable_if<
std::is_integral<UnsignedIntType>::value
&& std::is_unsigned<UnsignedIntType>::value>::type> {
 public:
  static llvm::Value* CreateAddOverflow(CodegenUtils* generator,
                                        llvm::Value* arg0,
                                        llvm::Value* arg1) {
    Checker(generator->GetType<UnsignedIntType>(), arg0, arg1);
    return generator->CreateIntrinsicInstrCall(
        llvm::Intrinsic::uadd_with_overflow,
        generator->GetType<UnsignedIntType>(),
        arg0,
        arg1);
  }
  static llvm::Value* CreateSubOverflow(CodegenUtils* generator,
                                        llvm::Value* arg0,
                                        llvm::Value* arg1) {
    Checker(generator->GetType<UnsignedIntType>(), arg0, arg1);
    return generator->CreateIntrinsicInstrCall(
        llvm::Intrinsic::usub_with_overflow,
        generator->GetType<UnsignedIntType>(),
        arg0,
        arg1);
  }
  static llvm::Value* CreateMulOverflow(CodegenUtils* generator,
                                        llvm::Value* arg0,
                                        llvm::Value* arg1) {
    Checker(generator->GetType<UnsignedIntType>(), arg0, arg1);
    return generator->CreateIntrinsicInstrCall(
        llvm::Intrinsic::umul_with_overflow,
        generator->GetType<UnsignedIntType>(),
        arg0,
        arg1);
  }

 private:
  static void Checker(llvm::Type* llvm_unsigned_type,
                      llvm::Value* arg0,
                      llvm::Value* arg1) {
    assert(nullptr != arg0 && nullptr != arg0->getType());
    assert(nullptr != arg1 && nullptr != arg1->getType());
    assert(arg0->getType()->isIntegerTy());
    assert(arg0->getType()->getScalarSizeInBits() ==
        llvm_unsigned_type->getScalarSizeInBits());
    assert(arg1->getType()->isIntegerTy());
    assert(arg1->getType()->getScalarSizeInBits() ==
            llvm_unsigned_type->getScalarSizeInBits());
  }
};

// Partial specialization for signed integer types.
template <typename SignedIntType>
class ArithOpMaker<
SignedIntType,
typename std::enable_if<
std::is_integral<SignedIntType>::value
&& std::is_signed<SignedIntType>::value>::type> {
 public:
  static llvm::Value* CreateAddOverflow(CodegenUtils* generator,
                                        llvm::Value* arg0,
                                        llvm::Value* arg1) {
    Checker(generator->GetType<SignedIntType>(), arg0, arg1);
    return generator->CreateIntrinsicInstrCall(
        llvm::Intrinsic::sadd_with_overflow,
        generator->GetType<SignedIntType>(),
        arg0,
        arg1);
  }

  static llvm::Value* CreateSubOverflow(CodegenUtils* generator,
                                        llvm::Value* arg0,
                                        llvm::Value* arg1) {
    Checker(generator->GetType<SignedIntType>(), arg0, arg1);
    return generator->CreateIntrinsicInstrCall(
        llvm::Intrinsic::ssub_with_overflow,
        generator->GetType<SignedIntType>(),
        arg0,
        arg1);
  }

  static llvm::Value* CreateMulOverflow(CodegenUtils* generator,
                                        llvm::Value* arg0,
                                        llvm::Value* arg1) {
    Checker(generator->GetType<SignedIntType>(), arg0, arg1);
    return generator->CreateIntrinsicInstrCall(
        llvm::Intrinsic::smul_with_overflow,
        generator->GetType<SignedIntType>(),
        arg0,
        arg1);
  }
 private:
  static void Checker(llvm::Type* llvm_signed_type,
                      llvm::Value* arg0,
                      llvm::Value* arg1) {
    assert(nullptr != arg0 && nullptr != arg0->getType());
    assert(nullptr != arg1 && nullptr != arg1->getType());
    assert(arg0->getType()->isIntegerTy());
    assert(arg0->getType()->getScalarSizeInBits() ==
        llvm_signed_type->getScalarSizeInBits());
    assert(arg1->getType()->isIntegerTy());
    assert(arg1->getType()->getScalarSizeInBits() ==
            llvm_signed_type->getScalarSizeInBits());
  }
};

// Partial specialization for enums (mapped to the underlying integer type).
template <typename EnumType>
class ArithOpMaker<
EnumType,
typename std::enable_if<std::is_enum<EnumType>::value>::type> {
 public:
};

// Explicit specialization for 32-bit float.
template <>
class ArithOpMaker<float> {
 public:
};

// Explicit specialization for 64-bit double.
template <>
class ArithOpMaker<double> {
 public:
  static llvm::Value* CreateAddOverflow(CodegenUtils* generator,
                                        llvm::Value* arg0,
                                        llvm::Value* arg1) {
    Checker(arg0, arg1);

    // TODO(armenatzoglou) Support overflow
    return generator->ir_builder()->CreateFAdd(arg0, arg1);
  }

  static llvm::Value* CreateSubOverflow(CodegenUtils* generator,
                                        llvm::Value* arg0,
                                        llvm::Value* arg1) {
    Checker(arg0, arg1);

    // TODO(armenatzoglou) Support overflow
    return generator->ir_builder()->CreateFSub(arg0, arg1);
  }

  static llvm::Value* CreateMulOverflow(CodegenUtils* generator,
                                        llvm::Value* arg0,
                                        llvm::Value* arg1) {
    Checker(arg0, arg1);

    // TODO(armenatzoglou) Support overflow
    return generator->ir_builder()->CreateFMul(arg0, arg1);
  }


 private:
  static void Checker(llvm::Value* arg0,
                      llvm::Value* arg1) {
    assert(nullptr != arg0 && nullptr != arg0->getType());
    assert(nullptr != arg1 && nullptr != arg1->getType());
    assert(arg0->getType()->isDoubleTy());
    assert(arg1->getType()->isDoubleTy());
  }
};

}  // namespace codegen_utils_detail


template <typename CppType>
llvm::Value* CodegenUtils::CreateAddOverflow(llvm::Value* arg0,
                                             llvm::Value* arg1) {
  return codegen_utils_detail::ArithOpMaker<CppType>::CreateAddOverflow(this,
                                                                        arg0,
                                                                        arg1);
}

template <typename CppType>
llvm::Value* CodegenUtils::CreateIncOverflow(llvm::Value* arg) {
  return codegen_utils_detail::ArithOpMaker<CppType>::
      CreateAddOverflow(this, arg, GetConstant<CppType>(1));
}

template <typename CppType>
llvm::Value* CodegenUtils::CreateSubOverflow(llvm::Value* arg0,
                                             llvm::Value* arg1) {
  return codegen_utils_detail::ArithOpMaker<CppType>::CreateSubOverflow(this,
                                                                        arg0,
                                                                        arg1);
}

template <typename CppType>
llvm::Value* CodegenUtils::CreateMulOverflow(llvm::Value* arg0,
                                             llvm::Value* arg1) {
  return codegen_utils_detail::ArithOpMaker<CppType>::CreateMulOverflow(this,
                                                                        arg0,
                                                                        arg1);
}

// ----------------------------------------------------------------------------
// Implementation of CodegenUtils::CreateCast().

// Helper template classes are nested in this namespace and are not considered
// part of the public API.
namespace codegen_utils_detail {

// CastMaker has various template specializations to handle casting of
// different C++ types. The specialized versions have a static method
// CreateCast() that takes an 'llvm::Value' of SrcType and a pointer to
// CodegenUtils object,  and returns a pointer to an
// 'llvm::Value' equivalent to DestType
template <typename DestType,
          typename SrcType,
          typename Enable = void>
class CastMaker {
};

// Partial specialization for unsigned int to signed / unsigned integer.
template <typename IntegerType, typename UnsignedIntType>
class CastMaker<
    IntegerType,
    UnsignedIntType,
    typename std::enable_if<
        std::is_integral<IntegerType>::value
        && std::is_integral<UnsignedIntType>::value
        && std::is_unsigned<UnsignedIntType>::value>::type> {
 public:
  static llvm::Value* CreateCast(llvm::Value* value,
                                 CodegenUtils* codegen_utils) {
    assert(nullptr != codegen_utils);
    llvm::Type* llvm_dest_type = codegen_utils->GetType<IntegerType>();
    Checker(value, llvm_dest_type);
    return codegen_utils->ir_builder()->CreateZExtOrTrunc(value,
                                                         llvm_dest_type);
  }
 private:
  static void Checker(llvm::Value* value,
               llvm::Type* llvm_dest_type) {
    assert(nullptr != value);
    assert(nullptr != value->getType());
    assert(value->getType()->isIntegerTy());
    assert(llvm_dest_type->isIntegerTy());
  }
};

// Partial specialization for signed int to signed / unsigned integer.
template <typename IntegerType, typename SignedIntType>
class CastMaker<
    IntegerType,
    SignedIntType,
    typename std::enable_if<
        std::is_integral<IntegerType>::value
        && std::is_integral<SignedIntType>::value
        && std::is_signed<SignedIntType>::value>::type> {
 public:
  static llvm::Value* CreateCast(llvm::Value* value,
                                 CodegenUtils* codegen_utils) {
    assert(nullptr != codegen_utils);
    llvm::Type* llvm_dest_type = codegen_utils->GetType<IntegerType>();
    Checker(value, llvm_dest_type);

    return codegen_utils->ir_builder()->CreateSExtOrTrunc(value,
                                                          llvm_dest_type);
  }
 private:
  static void Checker(llvm::Value* value,
               llvm::Type* llvm_dest_type) {
    assert(nullptr != value);
    assert(nullptr != value->getType());
    assert(value->getType()->isIntegerTy());
    assert(llvm_dest_type->isIntegerTy());
  }
};

// Partial specialization for any floating point to 32-bit float
template <typename FloatingPoint>
class CastMaker<
    float,
    FloatingPoint,
    typename std::enable_if<
            std::is_floating_point<FloatingPoint>::value>::type> {
 public:
  static llvm::Value* CreateCast(llvm::Value* value,
                                 CodegenUtils* codegen_utils) {
    assert(nullptr != codegen_utils);
    llvm::Type* llvm_dest_type = codegen_utils->GetType<float>();
    Checker(value, llvm_dest_type);
    if (value->getType()->isFloatTy()) { return value; }
    return codegen_utils->ir_builder()->CreateFPTrunc(
        value, llvm_dest_type);
  }
 private:
  static void Checker(llvm::Value* value,
                 llvm::Type* llvm_dest_type) {
      assert(nullptr != value);
      assert(nullptr != value->getType());
      assert(value->getType()->isFloatTy() ||
             value->getType()->isDoubleTy());
      assert(llvm_dest_type->isFloatTy());
    }
};

// Partial specialization for any floating point to 64-bit float
template <typename FloatingPoint>
class CastMaker<
    double,
    FloatingPoint,
    typename std::enable_if<
            std::is_floating_point<FloatingPoint>::value>::type> {
 public:
  static llvm::Value* CreateCast(llvm::Value* value,
                                 CodegenUtils* codegen_utils) {
    assert(nullptr != codegen_utils);
    llvm::Type* llvm_dest_type = codegen_utils->GetType<double>();
    Checker(value, llvm_dest_type);
    if (value->getType()->isDoubleTy()) { return value; }
    return codegen_utils->ir_builder()->CreateFPExt(
        value, llvm_dest_type);
  }
 private:
  static void Checker(llvm::Value* value,
                      llvm::Type* llvm_dest_type) {
    assert(nullptr != value);
    assert(nullptr != value->getType());
    assert(value->getType()->isFloatTy() ||
           value->getType()->isDoubleTy());
    assert(llvm_dest_type->isDoubleTy());
  }
};

}  // namespace codegen_utils_detail

template <typename DestType, typename SrcType>
llvm::Value* CodegenUtils::CreateCast(llvm::Value* value) {
  return codegen_utils_detail::CastMaker<DestType, SrcType>::CreateCast(
      value, this);
}

}  // namespace gpcodegen

#endif  // GPCODEGEN_CODEGEN_UTILS_H_
// EOF
