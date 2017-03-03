//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    gp_codegen_utils.h
//
//  @doc:
//    Object that extends the functionality of CodegenUtils by adding GPDB
//    specific functionality and utilities to aid in the runtime code generation
//    for a LLVM module
//
//  @test:
//
//
//---------------------------------------------------------------------------

#ifndef GPCODEGEN_GP_CODEGEN_UTILS_H_  // NOLINT(build/header_guard)
#define GPCODEGEN_GP_CODEGEN_UTILS_H_

#include "codegen/utils/codegen_utils.h"
#include "codegen/codegen_wrapper.h"

extern "C" {
#include "utils/elog.h"
#include "utils/palloc.h"
#include "nodes/memnodes.h"
}

#define EXPAND_CREATE_ELOG(codegen_utils, elevel, ...)  \
  codegen_utils->CreateElog(__FILE__, __LINE__, PG_FUNCNAME_MACRO, \
                            elevel, ##__VA_ARGS__)

#define EXPAND_CREATE_EREPORT(codegen_utils, elevel, ecode, errmsg_fmt, ...)  \
  codegen_utils->CreateEreport(__FILE__, __LINE__, PG_FUNCNAME_MACRO, \
                               TEXTDOMAIN, elevel, ecode, errmsg_fmt, \
                               ##__VA_ARGS__)

#define EXPAND_CREATE_PALLOC(codegen_utils, sz) \
  codegen_utils->CreatePalloc(sz, __FILE__, PG_FUNCNAME_MACRO, __LINE__)

namespace gpcodegen {

class GpCodegenUtils : public CodegenUtils {
 public:
  /**
   * @brief Constructor.
   *
   * @param module_name A human-readable name for the module that this
   *        CodegenUtils will manage.
   **/
  explicit GpCodegenUtils(llvm::StringRef module_name)
  : CodegenUtils(module_name) {
  }

  ~GpCodegenUtils() {
  }

  /*
   * @brief Create LLVM instructions to call elog_start() and elog_finish().
   *
   * @warning This method does not create instructions for any sort of exception
   *          handling. In the case that elog throws an error, the code with jump
   *          straight out of the compiled module back to the last location in GPDB
   *          that setjump was called.
   *
   * @param file_name   name of the file from which CreateElog is called
   * @param lineno      number of the line in the file from which CreateElog is called
   * @param func_name   name of the function that calls CreateElog
   * @param llvm_elevel llvm::Value pointer to an integer representing the error level
   * @param llvm_fmt    llvm::Value pointer to the format string
   * @tparam args       llvm::Value pointers to arguments to elog()
   */
  template<typename... V>
  void CreateElog(
      const char* file_name,
      int lineno,
      const char* func_name,
      llvm::Value* llvm_elevel,
      llvm::Value* llvm_fmt,
      const V ... args ) {
    llvm::Function* llvm_elog_start =
        GetOrRegisterExternalFunction(elog_start, "elog_start");
    llvm::Function* llvm_elog_finish =
        GetOrRegisterExternalFunction(elog_finish, "elog_finish");

    ir_builder()->CreateCall(
        llvm_elog_start, {
            GetConstant(file_name),   // Filename
            GetConstant(lineno),    // line number
            GetConstant(func_name)    // function name
    });
    ir_builder()->CreateCall(
        llvm_elog_finish, {
            llvm_elevel,
            llvm_fmt,
            args... });
  }

  /*
   * @brief Create LLVM instructions to call elog_start() and elog_finish().
   *        A convenient alternative that automatically converts an integer elevel and
   *        format string to LLVM constants.
   *
   * @warning This method does not create instructions for any sort of exception
   *          handling. In the case that elog throws an error, the code with jump
   *          straight out of the compiled module back to the last location in GPDB
   *          that setjump was called.
   *
   * @param file_name   name of the file from which CreateElog is called
   * @param lineno      number of the line in the file from which CreateElog is called
   * @param func_name   name of the function that calls CreateElog
   * @param elevel      Integer representing the error level
   * @param fmt         Format string
   * @tparam args       llvm::Value pointers to arguments to elog()
   */
  template<typename... V>
  void CreateElog(
      const char* file_name,
      int lineno,
      const char* func_name,
      int elevel,
      const char* fmt,
      const V ... args ) {
    CreateElog(file_name, lineno, func_name, GetConstant(elevel),
               GetConstant(fmt), args...);
  }


  /*
   * @brief Create LLVM instructions to implement ereport; but only for
   * ereport calls of the form: ereport(elevel, (errcode(code), errmsg(fmt, args...)).
   *
   * The reason for support only this form is because the ereport() mechanism is
   * very flexible because of overloaded C macros - so much so that it is tricky
   * to implement using the LLVM API. For example, it may appear from the above
   * code that errcode and errmsg are called before any errstart or errfinish.
   * However because of the way the macro is implemented, errcode and errmsg are
   * called *after* errstart and only if errstart returns true
   *
   * @note This function calls the following external functions: errstart, errcode, errmsg
   * and errfinish.
   *
   * @param file_name   name of the file from which CreateEreport is called
   * @param lineno      number of the line in the file from which CreateEreport is called
   * @param func_name   name of the function that calls CreateEreport
   * @param domain      llvm::Value pointer to message domain of the report
   * @param elevel      llvm::Value pointer to integer representing the error level
   * @param ecode       llvm::Value pointer to error code passed to a call to
   * @param errmsg_fmt  llvm::Value pointer to format string
   * @tparam args       llvm::Value pointers to arguments to elog()
   */
  template<typename... V>
  void CreateEreport(
      const char* file_name,
      int lineno,
      const char* func_name,
      const char* domain,
      int elevel,
      int ecode,
      const char* errmsg_fmt,
      const V ... args ) {
    CreateEreport(file_name,
                  lineno,
                  func_name,
                  GetConstant(domain),
                  GetConstant(elevel),
                  GetConstant(ecode),
                  GetConstant(errmsg_fmt));
  }

  /*
   * @brief Create LLVM instructions to implements ereport; but only for
   * ereport calls of the form: ereport(elevel, (errcode(code), errmsg(fmt, args...)).
   *
   * @param file_name   name of the file from which CreateEreport is called
   * @param lineno      number of the line in the file from which CreateEreport is called
   * @param func_name   name of the function that calls CreateEreport
   * @param domain      message domain of the report
   * @param elevel      Integer representing the error level
   * @param ecode       error code passed to a call to
   * @param errmsg_fmt  Format string
   * @tparam args       llvm::Value pointers to arguments to elog()
   */
  template<typename... V>
  void CreateEreport(
      const char* file_name,
      int lineno,
      const char* func_name,
      llvm::Value* domain,
      llvm::Value* elevel,
      llvm::Value* ecode,
      llvm::Value* errmsg_fmt,
      const V ... args ) {
    // Make sure the external functions required are available
    llvm::Function* llvm_errstart =
        GetOrRegisterExternalFunction(errstart, "errstart");
    llvm::Function* llvm_errcode =
        GetOrRegisterExternalFunction(errcode, "errcode");
    llvm::Function* llvm_errmsg =
        GetOrRegisterExternalFunction(errmsg, "errmsg");
    llvm::Function* llvm_errfinish =
        GetOrRegisterExternalFunction(errfinish, "errfinish");

    auto irb = ir_builder();

    // Retrive the current function to create new blocks
    llvm::Function* current_function = irb->GetInsertBlock()->getParent();

    // Case when errstart returns true
    llvm::BasicBlock* errfinish_block =
        CreateBasicBlock("errfinish", current_function);
    // Case when errstart returns false
    llvm::BasicBlock* rest_ereport_block =
        CreateBasicBlock("rest_ereport", current_function);
    // Case when elevel >= ERROR
    llvm::BasicBlock* abort_block =
        CreateBasicBlock("abort", current_function);
    // Done with all cases
    llvm::BasicBlock* end_ereport_block =
        CreateBasicBlock("end_ereport", current_function);


    // if (errstart(...)) errfinish(...) {{{
    llvm::Value* llvm_ret = irb->CreateCall(
        llvm_errstart, {
            elevel,
            GetConstant(file_name),
            GetConstant(lineno),
            GetConstant(func_name),
            domain
        });

    irb->CreateCondBr(llvm_ret,
                      errfinish_block /* true */,
                      rest_ereport_block /* false */);

    irb->SetInsertPoint(errfinish_block);
    irb->CreateCall(llvm_errfinish, {
        irb->CreateCall(llvm_errcode, {ecode}),
        irb->CreateCall(llvm_errmsg, {errmsg_fmt, args...}) });
    irb->CreateBr(rest_ereport_block);
    // }}}

    // if (elevel >= ERROR) abort() {{{
    irb->SetInsertPoint(rest_ereport_block);
    irb->CreateCondBr(irb->CreateICmpSGE(elevel, GetConstant(ERROR)),
                      abort_block /* true */,
                      end_ereport_block /* false */);

    irb->SetInsertPoint(abort_block);
    irb->CreateCall(GetOrRegisterExternalFunction(abort, "abort"));

    irb->CreateBr(end_ereport_block);
    // }}}

    // Set up for adding instructions past this point
    irb->SetInsertPoint(end_ereport_block);
  }

  /**
   * @brief Create a Cast instruction to convert given llvm::Value of Datum type
   *        to given Cpp type
   *
   * @note Depend on cpp type's size, it will do trunc or bit cast or
   *       int2ptr cast. This is same as converting gpdb's Datum to cpptype.
   *
   * @tparam CppType  Destination cpp type
   * @param value     LLVM Value on which casting has to be done.
   *
   * @return LLVM Value that casted to given Cpp type.
   **/
  template <typename CppType>
  llvm::Value* CreateDatumToCppTypeCast(llvm::Value* value) {
    assert(nullptr != value);
    llvm::Type* llvm_dest_type = GetType<CppType>();
    // If it is a pointer, do direct pointer cast
    if (llvm_dest_type->isPointerTy()) {
      return ir_builder()->CreateIntToPtr(value, llvm_dest_type);
    }
    unsigned dest_size = llvm_dest_type->getScalarSizeInBits();
    assert(0 != dest_size);

    llvm::Type* llvm_src_type = value->getType();
    unsigned src_size = llvm_src_type->getScalarSizeInBits();

    // Given src value, it should be of type Datum(int64_t)
    assert(llvm_src_type->isIntegerTy());
    assert(src_size == sizeof(Datum) << 3);

    // Datum size should be the largest
    assert(dest_size <= src_size);

    // Get dest type as integer type with same size
    llvm::Type* llvm_dest_as_int_type = llvm::IntegerType::get(*context(),
                                                             dest_size);
    llvm::Value* llvm_casted_value = value;

    if (dest_size < src_size) {
      llvm_casted_value = ir_builder()->CreateTrunc(value,
                                                    llvm_dest_as_int_type);
    }

    if (llvm_src_type->getTypeID() != llvm_dest_type->getTypeID()) {
      return ir_builder()->CreateBitCast(llvm_casted_value, llvm_dest_type);
    }

    return llvm_casted_value;
  }

  /**
   * @brief Create instructions to call MemoryContextAllocImpl in the
   *        CurrentMemoryContext. Use the macro EXPAND_CREATE_PALLOC to get the
   *        line number, function name and file name.
   *
   * @param size  Size to allocate in the CurrentMemoryContext
   * @param file  File name
   * @param func  Function name
   * @param line  Line number
   * @return LLVM::Value pointer to the allocated memory
   */
  llvm::Value* CreatePalloc(Size size,
                            const char* file,
                            const char *func,
                            int line);

  /**
   * @brief Create a Cast instruction to convert given llvm::Value of any type
   *        to Datum
   *
   * @note This is same as converting any cpptype to Datum in gpdb.
   *
   * @param value     LLVM Value on which casting has to be done.
   * @param is_src_unsigned true if given value is of unsigned type.
   *
   *
   * @return LLVM Value that casted to Datum type.
   **/
  llvm::Value* CreateCppTypeToDatumCast(llvm::Value* value,
                                        bool is_src_unsigned = false);
};
}  // namespace gpcodegen

#endif  // GPCODEGEN_GP_CODEGEN_UTILS_H
// EOF
