//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    pg_numeric_func_generator.cc
//
//  @doc:
//    Base class for numeric functions to generate code
//
//---------------------------------------------------------------------------

#include "codegen/pg_numeric_func_generator.h"

using gpcodegen::GpCodegenUtils;
using gpcodegen::PGNumericFuncGenerator;
using gpcodegen::PGFuncGeneratorInfo;

bool PGNumericFuncGenerator::GenerateIntFloatAvgAmalg(
    gpcodegen::GpCodegenUtils* codegen_utils,
    const gpcodegen::PGFuncGeneratorInfo& pg_func_info,
    llvm::Value** llvm_out_value) {
  // TODO(nikos): Can we figure if we need to detoast during generation?
  llvm::Function* llvm_pg_detoast_datum = codegen_utils->
      GetOrRegisterExternalFunction(pg_detoast_datum, "pg_detoast_datum");

  auto irb = codegen_utils->ir_builder();
  llvm::Function* current_function = irb->GetInsertBlock()->getParent();

  llvm::Value* llvm_in_tr0 =
      irb->CreateCall(llvm_pg_detoast_datum, {pg_func_info.llvm_args[0]});
  llvm::Value* llvm_in_tr1 =
      irb->CreateCall(llvm_pg_detoast_datum, {pg_func_info.llvm_args[1]});

  // if(transdata == NULL ||
  //    VARSIZE(transdata) != sizeof(IntFloatAvgTransdata)) { ... }
  llvm::Value* llvm_tr0 = nullptr;
  GeneratePallocTransdata(codegen_utils, llvm_in_tr0, &llvm_tr0);

  // if(tr1 == NULL || VARSIZE(tr1) != sizeof(IntFloatAvgTransdata))
  llvm::Value* llvm_varlena_null_size_cond = nullptr;
  GenerateVarlenSizeCheck(codegen_utils,
                          llvm_in_tr1,
                          codegen_utils->
                          GetConstant<uint32>(sizeof(IntFloatAvgTransdata)),
                          &llvm_varlena_null_size_cond);

  llvm::BasicBlock* update_block = codegen_utils->CreateBasicBlock(
      "update_block", current_function);
  llvm::BasicBlock* end_update_block = codegen_utils->CreateBasicBlock(
      "end_update_block", current_function);
  irb->CreateCondBr(llvm_varlena_null_size_cond,
                    end_update_block,
                    update_block);

  irb->SetInsertPoint(update_block);
  // tr0->count += tr1->count;
  // tr0->sum += tr1->sum; {{
  llvm::Value* llvm_tr0_sum_ptr =
      codegen_utils->GetPointerToMember(llvm_tr0, &IntFloatAvgTransdata::sum);
  llvm::Value* llvm_tr0_count_ptr =
      codegen_utils->GetPointerToMember(llvm_tr0, &IntFloatAvgTransdata::count);
  llvm::Value* llvm_tr1_sum_ptr = codegen_utils->
      GetPointerToMember(llvm_in_tr1, &IntFloatAvgTransdata::sum);
  llvm::Value* llvm_tr1_count_ptr = codegen_utils->
      GetPointerToMember(llvm_in_tr1, &IntFloatAvgTransdata::count);
  irb->CreateStore(irb->CreateFAdd(
      irb->CreateLoad(llvm_tr0_sum_ptr),
      irb->CreateLoad(llvm_tr1_sum_ptr)),
                   llvm_tr0_sum_ptr);
  irb->CreateStore(irb->CreateAdd(
      irb->CreateLoad(llvm_tr0_count_ptr),
      irb->CreateLoad(llvm_tr1_count_ptr)),
                   llvm_tr0_count_ptr);
  // }}
  irb->CreateBr(end_update_block);
  irb->SetInsertPoint(end_update_block);

  *llvm_out_value = llvm_tr0;
  return true;
}

bool PGNumericFuncGenerator::GenerateVarlenSizeCheck(
    gpcodegen::GpCodegenUtils* codegen_utils,
    llvm::Value* llvm_ptr,
    llvm::Value* llvm_size,
    llvm::Value** llvm_out_cond) {

  llvm::Function* llvm_varsize = codegen_utils->
      GetOrRegisterExternalFunction(VARSIZE_regular, "VARSIZE_regular");

  auto irb = codegen_utils->ir_builder();

  *llvm_out_cond = irb->CreateOr(
      irb->CreateICmpEQ(llvm_ptr, codegen_utils->GetConstant<void*>(nullptr)),
      irb->CreateICmpNE(
          irb->CreateCall(llvm_varsize, {llvm_ptr}),
          llvm_size));
  return true;
}

bool PGNumericFuncGenerator::GeneratePallocTransdata(
    gpcodegen::GpCodegenUtils* codegen_utils,
    llvm::Value* llvm_in_transdata_ptr,
    llvm::Value** llvm_out_trandata_ptr) {

  llvm::Function* llvm_set_varsize = codegen_utils->
      GetOrRegisterExternalFunction(SET_VARSIZE_regular, "SET_VARSIZE_regular");

  auto irb = codegen_utils->ir_builder();

  llvm::BasicBlock* entry_block = irb->GetInsertBlock();
  llvm::Function* current_function = entry_block->getParent();
  llvm::BasicBlock* transdata_palloc_block = codegen_utils->
      CreateBasicBlock("transdata_palloc_block", current_function);
  llvm::BasicBlock* end_transdata_palloc_block = codegen_utils->
      CreateBasicBlock("end_transdata_palloc_block", current_function);

  // if(tr0 == NULL || VARSIZE(tr0) != sizeof(IntFloatAvgTransdata)) {{
  llvm::Value* palloc_cond = nullptr;
  GenerateVarlenSizeCheck(codegen_utils,
                          llvm_in_transdata_ptr,
                          codegen_utils->GetConstant<uint32>(
                              sizeof(IntFloatAvgTransdata)),
                              &palloc_cond);
  irb->CreateCondBr(palloc_cond,
                    transdata_palloc_block,
                    end_transdata_palloc_block);
  // }}
  irb->SetInsertPoint(transdata_palloc_block);
  // tr0 = (IntFloatAvgTransdata *) palloc(sizeof(IntFloatAvgTransdata));
  llvm::Value* llvm_palloc_transdata_ptr =
      EXPAND_CREATE_PALLOC(codegen_utils, sizeof(IntFloatAvgTransdata));
  // SET_VARSIZE(tr0, sizeof(IntFloatAvgTransdata));
  irb->CreateCall(llvm_set_varsize, {
      llvm_palloc_transdata_ptr,
      codegen_utils->GetConstant(sizeof(IntFloatAvgTransdata))});
  // tr0->sum = 0;
  irb->CreateStore(codegen_utils->GetConstant<float8>(0),
                   codegen_utils->GetPointerToMember(
                       llvm_palloc_transdata_ptr, &IntFloatAvgTransdata::sum));
  // tr0->count = 0;
  irb->CreateStore(codegen_utils->GetConstant<int64>(0),
                   codegen_utils->GetPointerToMember(
                       llvm_palloc_transdata_ptr,
                       &IntFloatAvgTransdata::count));

  irb->CreateBr(end_transdata_palloc_block);
  irb->SetInsertPoint(end_transdata_palloc_block);
  assert(llvm_in_transdata_ptr->getType() ==
      llvm_palloc_transdata_ptr->getType());
  llvm::PHINode* llvm_transdata_ptr = irb->
      CreatePHI(llvm_in_transdata_ptr->getType(), 2);
  llvm_transdata_ptr->addIncoming(llvm_in_transdata_ptr, entry_block);
  llvm_transdata_ptr->addIncoming(llvm_palloc_transdata_ptr,
                                  transdata_palloc_block);

  *llvm_out_trandata_ptr = llvm_transdata_ptr;
  return true;
}
