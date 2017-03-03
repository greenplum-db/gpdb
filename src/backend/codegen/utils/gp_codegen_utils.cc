//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    gp_codegen_utils.cc
//
//  @doc:
//    Object that extends the functionality of CodegenUtils by adding GPDB
//    specific functionality and utilities to aid in the runtime code generation
//    for a LLVM module
//
//  @test:
//    Unittests in tests/code_generator_unittest.cc
//
//
//---------------------------------------------------------------------------

#include "codegen/utils/gp_codegen_utils.h"

namespace gpcodegen {

llvm::Value* GpCodegenUtils::CreateCppTypeToDatumCast(
    llvm::Value* value, bool is_src_unsigned) {
  assert(nullptr != value);

  llvm::Type* llvm_dest_type = GetType<Datum>();
  unsigned dest_size = llvm_dest_type->getScalarSizeInBits();
  assert(sizeof(Datum) << 3 == dest_size);

  llvm::Type* llvm_src_type = value->getType();
  unsigned src_size = llvm_src_type->getScalarSizeInBits();
  // If the source value is pointer type, just convert to int.
  if (llvm_src_type->isPointerTy()) {
    return ir_builder()->CreatePtrToInt(value, llvm_dest_type);
  }

  assert(0 != src_size);
  assert(dest_size >= src_size);

  llvm::Value* llvm_int_value = value;
  // Convert given value to int type to do ext / trunc
  if (!llvm_src_type->isIntegerTy()) {
    // Get src type as integer type with same as size
    llvm::Type* llvm_src_as_int_type = llvm::IntegerType::get(*context(),
                                                              src_size);
    llvm_int_value = ir_builder()->CreateBitCast(
        value, llvm_src_as_int_type);
  }

  llvm::Value* llvm_casted_value = llvm_int_value;
  if (src_size < dest_size) {
    // If a given src type is integer but not a bool and unsigned flag is set to
    // false, then do signed extension.
    if (llvm_src_type->isIntegerTy() &&
        src_size > 1 &&
        !is_src_unsigned) {
      llvm_casted_value = ir_builder()->CreateSExt(llvm_int_value,
                                                   llvm_dest_type);
    } else {
      // Datum doesn't do signed extension if src type is not integer, not bool
      // and given type is unsigned.
      llvm_casted_value = ir_builder()->CreateZExt(llvm_int_value,
                                                   llvm_dest_type);
    }
  }
  return llvm_casted_value;
}

llvm::Value* GpCodegenUtils::CreatePalloc(Size size,
                                          const char* file,
                                          const char *func,
                                          int line) {
  llvm::Function* llvm_memory_context_alloc_impl =
      GetOrRegisterExternalFunction(MemoryContextAllocImpl,
      "MemoryContextAllocImpl");
  // Define llvm_memory_context_alloc_impl as a system memory allocation
  // function that returns a pointer to allocated storage.
  llvm_memory_context_alloc_impl->setDoesNotAlias(0 /* return value */);
  llvm::Value* llvm_current_memory_context =
      ir_builder()->CreateLoad(GetConstant(&CurrentMemoryContext));
  return ir_builder()->CreateCall(llvm_memory_context_alloc_impl, {
      llvm_current_memory_context,
      GetConstant<Size>(size),
          GetConstant(file),
          GetConstant(func),
          GetConstant(line)});
}

}  // namespace gpcodegen

// EOF
