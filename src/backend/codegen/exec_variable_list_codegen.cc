//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    codegen_utils.cpp
//
//  @doc:
//    Contains different code generators
//
//---------------------------------------------------------------------------
#include "codegen/exec_variable_list_codegen.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "codegen/utils/clang_compiler.h"
#include "codegen/utils/utility.h"
#include "codegen/utils/instance_method_wrappers.h"
#include "codegen/utils/codegen_utils.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Casting.h"

extern "C" {
#include "postgres.h"
#include "utils/elog.h"
#include "access/htup.h"
#include "nodes/execnodes.h"
#include "executor/tuptable.h"
}

using gpcodegen::ExecVariableListCodegen;

constexpr char ExecVariableListCodegen::kExecVariableListPrefix[];

class ElogWrapper {
 public:
  explicit ElogWrapper(gpcodegen::CodegenUtils* codegen_utils) :
    codegen_utils_(codegen_utils) {
    SetupElog();
  }
  ~ElogWrapper() {
    TearDownElog();
  }

  template<typename... V>
  void CreateElog(
      llvm::Value* llvm_elevel,
      llvm::Value* llvm_fmt,
      V ... args ) {
    assert(NULL != llvm_elevel);
    assert(NULL != llvm_fmt);

    codegen_utils_->ir_builder()->CreateCall(
        llvm_elog_start_, {
            codegen_utils_->GetConstant(""),  // Filename
            codegen_utils_->GetConstant(0),   // line number
            codegen_utils_->GetConstant("")   // function name
    });
    codegen_utils_->ir_builder()->CreateCall(
        llvm_elog_finish_, {
            llvm_elevel,
            llvm_fmt,
            args...
    });
  }
  template<typename... V>
  void CreateElog(
      int elevel,
      const char* fmt,
      V ... args ) {
    CreateElog(codegen_utils_->GetConstant(elevel),
               codegen_utils_->GetConstant(fmt),
               args...);
  }

 private:
  llvm::Function* llvm_elog_start_;
  llvm::Function* llvm_elog_finish_;

  gpcodegen::CodegenUtils* codegen_utils_;

  void SetupElog() {
    assert(codegen_utils_ != nullptr);
    llvm_elog_start_ = codegen_utils_->RegisterExternalFunction(elog_start);
    assert(llvm_elog_start_ != nullptr);
    llvm_elog_finish_ = codegen_utils_->RegisterExternalFunction(elog_finish);
    assert(llvm_elog_finish_ != nullptr);
  }

  void TearDownElog(){
    llvm_elog_start_ = nullptr;
    llvm_elog_finish_ = nullptr;
  }
};

ExecVariableListCodegen::ExecVariableListCodegen
(
    ExecVariableListFn regular_func_ptr,
    ExecVariableListFn* ptr_to_regular_func_ptr,
    ProjectionInfo* proj_info,
    TupleTableSlot* slot) :
    BaseCodegen(kExecVariableListPrefix,
        regular_func_ptr,
        ptr_to_regular_func_ptr),
    proj_info_(proj_info),
    slot_(slot) {
}


bool ExecVariableListCodegen::GenerateExecVariableList(
    gpcodegen::CodegenUtils* codegen_utils) {

  assert(NULL != codegen_utils);

  ElogWrapper elogwrapper(codegen_utils);
  static_assert(sizeof(Datum) == sizeof(int64),
      "sizeof(Datum) doesn't match sizeof(int64)");

  if ( NULL == proj_info_->pi_varSlotOffsets ) {
    elog(DEBUG1,
        "Cannot codegen ExecVariableList because varSlotOffsets are null");
    return false;
  }

  // Only do codegen if all the elements in the target list are on the same
  // tuple slot This is an assumption for scan nodes, but we fall back when
  // joins are involved.
  for (int i = list_length(proj_info_->pi_targetlist) - 1; i > 0; i--) {
    if (proj_info_->pi_varSlotOffsets[i] !=
        proj_info_->pi_varSlotOffsets[i-1]) {
      elog(DEBUG1,
          "Cannot codegen ExecVariableList because multiple slots to deform.");
      return false;
    }
  }

  // Find the largest attribute index in projInfo->pi_targetlist
  int max_attr = *std::max_element(
      proj_info_->pi_varNumbers,
      proj_info_->pi_varNumbers + list_length(proj_info_->pi_targetlist));

  // System attribute
  if (max_attr <= 0) {
    elog(DEBUG1, "Cannot generate code for ExecVariableList"
                 "because max_attr is negative (i.e., system attribute).");
    return false;
  } else if (max_attr > slot_->tts_tupleDescriptor->natts) {
    elog(DEBUG1, "Cannot generate code for ExecVariableList"
                 "because max_attr is greater than natts.");
    return false;
  }

  // So looks like we're going to generate code
  llvm::Function* exec_variable_list_func = CreateFunction<ExecVariableListFn>(
      codegen_utils, GetUniqueFuncName());

  auto irb = codegen_utils->ir_builder();

  // BasicBlock of function entry.
  llvm::BasicBlock* entry_block = codegen_utils->CreateBasicBlock(
      "entry", exec_variable_list_func);
  // BasicBlock for checking correct slot
  llvm::BasicBlock* slot_check_block = codegen_utils->CreateBasicBlock(
      "slot_check", exec_variable_list_func);
  // BasicBlock for checking tuple type.
  llvm::BasicBlock* tuple_type_check_block = codegen_utils->CreateBasicBlock(
      "tuple_type_check", exec_variable_list_func);
  // BasicBlock for heap tuple check.
  llvm::BasicBlock* heap_tuple_check_block = codegen_utils->CreateBasicBlock(
      "heap_tuple_check", exec_variable_list_func);
  // BasicBlock for main.
  llvm::BasicBlock* main_block = codegen_utils->CreateBasicBlock(
      "main", exec_variable_list_func);
  // BasicBlock for final computations.
  llvm::BasicBlock* final_block = codegen_utils->CreateBasicBlock(
      "final", exec_variable_list_func);
  // BasicBlock for fall back.
  llvm::BasicBlock* fallback_block = codegen_utils->CreateBasicBlock(
      "fallback", exec_variable_list_func);

  // External functions
  llvm::Function* llvm_memset = codegen_utils->RegisterExternalFunction(memset);

  // Generation-time constants
  llvm::Value* llvm_max_attr = codegen_utils->GetConstant(max_attr);
  llvm::Value* llvm_slot = codegen_utils->GetConstant(slot_);

  // Function arguments to ExecVariableList
  llvm::Value* llvm_projInfo_arg = ArgumentByPosition(exec_variable_list_func, 0);
  llvm::Value* llvm_values_arg = ArgumentByPosition(exec_variable_list_func, 1);
  llvm::Value* llvm_isnull_arg = ArgumentByPosition(exec_variable_list_func, 2);

  // Entry block
  // -----------

  irb->SetInsertPoint(entry_block);
  // We start a sequence of checks to ensure that everything is fine and
  // we do not need to fall back.
  irb->CreateBr(slot_check_block);

  // Slot check block
  // ----------------

  irb->SetInsertPoint(slot_check_block);
  llvm::Value* llvm_econtext =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_projInfo_arg, &ProjectionInfo::pi_exprContext));
  llvm::Value* llvm_varSlotOffsets =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_projInfo_arg, &ProjectionInfo::pi_varSlotOffsets));

  // We want to fall back when ExecVariableList is called with a slot that's
  // different from the one we generated the function (eg HashJoin). We also
  // assume only 1 slot and that the slot is in a scan node ie from
  // exprContext->ecxt_scantuple or varOffset = 0
  llvm::Value* llvm_slot_arg =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_econtext, &ExprContext::ecxt_scantuple));

  irb->CreateCondBr(
      irb->CreateICmpEQ(llvm_slot, llvm_slot_arg),
      tuple_type_check_block /* true */,
      fallback_block /* false */);

  // Tuple type check block
  // ----------------------
  // We fall back if we see a virtual tuple or mem tuple,
  // but it's possible to partially handle those cases also

  irb->SetInsertPoint(tuple_type_check_block);
  llvm::Value* llvm_slot_PRIVATE_tts_flags_ptr =
      codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_flags);
  llvm::Value* llvm_slot_PRIVATE_tts_memtuple =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_memtuple));

  // (slot->PRIVATE_tts_flags & TTS_VIRTUAL) != 0
  llvm::Value* llvm_tuple_is_virtual = irb->CreateICmpNE(
      irb->CreateAnd(
          irb->CreateLoad(llvm_slot_PRIVATE_tts_flags_ptr),
          codegen_utils->GetConstant(TTS_VIRTUAL)),
          codegen_utils->GetConstant(0));

  // slot->PRIVATE_tts_memtuple != NULL
  llvm::Value* llvm_tuple_has_memtuple = irb->CreateICmpNE(
      llvm_slot_PRIVATE_tts_memtuple,
      codegen_utils->GetConstant((MemTuple) NULL));

  // Fall back if tuple is virtual or memtuple is null
  irb->CreateCondBr(
      irb->CreateOr(llvm_tuple_is_virtual, llvm_tuple_has_memtuple),
      fallback_block /*true*/, heap_tuple_check_block /*false*/);

  // HeapTuple check block
  // ---------------------
  // We fall back if the given tuple is not a heaptuple.

  irb->SetInsertPoint(heap_tuple_check_block);
  // In _slot_getsomeattrs, check if: TupGetHeapTuple(slot) != NULL
  llvm::Value* llvm_slot_PRIVATE_tts_heaptuple =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_heaptuple));
  llvm::Value* llvm_tuple_has_heaptuple = irb->CreateICmpNE(
      llvm_slot_PRIVATE_tts_heaptuple,
      codegen_utils->GetConstant((HeapTuple) NULL));
  irb->CreateCondBr(llvm_tuple_has_heaptuple,
                    main_block /*true*/,
                    fallback_block /*false*/);

  // Main block
  // ----------
  // We generate code for slot_deform_tuple.  Note that we do not need to check
  // the type of the tuple, since previous check guarantees that the we use the
  // enrolled slot, which includes a heap tuple.
  irb->SetInsertPoint(main_block);

  llvm::Value* llvm_heaptuple_t_data =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_slot_PRIVATE_tts_heaptuple,
          &HeapTupleData::t_data));

  llvm::Value* llvm_heaptuple_t_data_t_infomask =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_heaptuple_t_data, &HeapTupleHeaderData::t_infomask));

  // Implementation for : {{{
  // attno = HeapTupleHeaderGetNatts(tuple->t_data);
  // attno = Min(attno, attnum);
  llvm::Value* llvm_heaptuple_t_data_t_infomask2 =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_heaptuple_t_data,
          &HeapTupleHeaderData::t_infomask2));
  llvm::Value* llvm_attno_t0 =
      irb->CreateZExt(
          irb->CreateAnd(llvm_heaptuple_t_data_t_infomask2,
                         codegen_utils->GetConstant<int16>(HEAP_NATTS_MASK)),
                         codegen_utils->GetType<int>());

  llvm::Value* llvm_attno =
      irb->CreateSelect(
          irb->CreateICmpSLT(llvm_attno_t0, llvm_max_attr),
          llvm_attno_t0,
          llvm_max_attr);
  // }}}

  // Retrieve slot's PRIVATE variables
  llvm::Value* llvm_slot_PRIVATE_tts_isnull /* bool* */ =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_isnull));
  llvm::Value* llvm_slot_PRIVATE_tts_values /* Datum* */ =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_values));
  llvm::Value* llvm_slot_PRIVATE_tts_nvalid_ptr /* int* */ =
      codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_nvalid);

  llvm::Value* llvm_heaptuple_t_data_t_hoff = irb->CreateLoad(
      codegen_utils->GetPointerToMember(llvm_heaptuple_t_data,
                                        &HeapTupleHeaderData::t_hoff));

  // tup->t_bits
  llvm::Value* llvm_heaptuple_t_data_t_bits =
      codegen_utils->GetPointerToMember(
          llvm_heaptuple_t_data,
          &HeapTupleHeaderData::t_bits);

  // Find the start of input data byte array
  // same as tp in slot_deform_tuple (pointer to tuple data)
  llvm::Value* llvm_tuple_data_ptr = irb->CreateInBoundsGEP(
      llvm_heaptuple_t_data, {llvm_heaptuple_t_data_t_hoff});

  int off = 0;
  TupleDesc tupleDesc = slot_->tts_tupleDescriptor;
  Form_pg_attribute* att = tupleDesc->attrs;

  // For each attribute we use three blocks to i) check for null,
  // ii) handle null case, and iii) handle not null case.
  // Note that if an attribute cannot be null then we do not create
  // the last two blocks.
  llvm::BasicBlock* attribute_block = nullptr;
  llvm::BasicBlock* is_null_block = nullptr;
  llvm::BasicBlock* is_not_null_block = nullptr;

  // points to the attribute_block of the next attribute
  llvm::BasicBlock* next_attribute_block = nullptr;

  // block of first attribute
  attribute_block = codegen_utils->CreateBasicBlock(
      "attribute_block_"+0, exec_variable_list_func);

  irb->CreateBr(attribute_block);

  for (int attnum = 0; attnum < max_attr; ++attnum) {
    Form_pg_attribute thisatt = att[attnum];

    // If any thisatt is varlen
    if (thisatt->attlen < 0) {
      // We don't support variable length attributes.
      return false;
    }

    // ith attribute's block
    // ----------
    // Check if attribute can be null. If yes, then create blocks
    // to handle null and not null cases.
    irb->SetInsertPoint(attribute_block);

    // Create the block of (attnum+1)th attribute and jump to it after
    // you finish with current attribute.
    next_attribute_block = codegen_utils->CreateBasicBlock(
        "attribute_block_"+(attnum+1), exec_variable_list_func);

    llvm::Value* llvm_next_values_ptr =
        irb->CreateInBoundsGEP(llvm_slot_PRIVATE_tts_values,
                               {codegen_utils->GetConstant(attnum)});

    // If attribute can be null, then create blocks to handle
    // null and not null cases.
    if (!thisatt->attnotnull) {
      // Create blocks
      is_null_block = codegen_utils->CreateBasicBlock(
          "is_null_block_"+attnum, exec_variable_list_func);
      is_not_null_block = codegen_utils->CreateBasicBlock(
          "is_not_null_block_"+attnum, exec_variable_list_func);

      llvm::Value* llvm_attnum = codegen_utils->GetConstant(attnum);

      // Check if attribute is null
      // if (hasnulls && att_isnull(attnum, bp)) {{{
      // att_isnull(attnum, bp): !((BITS)[(ATT) >> 3] & (1<<((ATT) & 0x07))) {{
      // Expr_1: ((BITS)[(ATT) >> 3]
      llvm::Value* llvm_expr_1 = irb->CreateLoad(irb->CreateGEP(
          llvm_heaptuple_t_data_t_bits,
          {codegen_utils->GetConstant(0),
              irb->CreateAShr(llvm_attnum, codegen_utils->GetConstant(3))}));
      // Expr_2: (1 << ((ATT) & 0x07))
      llvm::Value* llvm_expr_2 = irb->CreateTrunc(
          irb->CreateShl(codegen_utils->GetConstant(1),
                         irb->CreateAnd(llvm_attnum,
                                        codegen_utils->GetConstant(0x07))),
                                        codegen_utils->GetType<int8>());
      // !(Exp_1 & Expr_2)
      llvm::Value* llvm_att_isnull = irb->CreateNot(
          irb->CreateTrunc(irb->CreateAnd(llvm_expr_1, llvm_expr_2),
                           codegen_utils->GetType<bool>()));
      // }}

      llvm::Value* llvm_hasnulls = irb->CreateTrunc(
          irb->CreateAnd(llvm_heaptuple_t_data_t_infomask,
                         codegen_utils->GetConstant<uint16>(HEAP_HASNULL)),
                         codegen_utils->GetType<bool>());

      // hasnulls && att_isnull(attnum, bp)
      llvm::Value* llvm_is_null = irb->CreateAnd(
          llvm_hasnulls, llvm_att_isnull);

      irb->CreateCondBr(
          llvm_is_null,
          is_null_block, /* true */
          is_not_null_block /* false */);
      //}}} End of if (hasnulls && att_isnull(attnum, bp))

      // Is null block
      // ----------------

      irb->SetInsertPoint(is_null_block);

      // values[attnum] = (Datum) 0; {{{
      irb->CreateStore(
          codegen_utils->GetConstant<Datum>(0),
          llvm_next_values_ptr);
      // }}}

      // isnull[attnum] = true; {{{
      llvm::Value* llvm_isnull_ptr =
          irb->CreateInBoundsGEP(llvm_slot_PRIVATE_tts_isnull,
                                 {llvm_attnum});
      irb->CreateStore(
          codegen_utils->GetConstant<bool>(true),
          llvm_isnull_ptr);
      // }}}

      // Jump to next attribute
      irb->CreateBr(next_attribute_block);

      // Is not null block
      // ----------------

      irb->SetInsertPoint(is_not_null_block);
    }  // End of if ( !thisatt->attnotnull )

    off = att_align(off, thisatt->attalign);

    // values[attnum] = fetchatt(thisatt, tp + off) {{{
    llvm::Value* llvm_next_t_data_ptr =
        irb->CreateInBoundsGEP(llvm_tuple_data_ptr,
                               {codegen_utils->GetConstant(off)});

    llvm::Value* llvm_colVal = nullptr;
    if (thisatt->attbyval) {
      // Load the value from the calculated input address.
      switch (thisatt->attlen) {
        case sizeof(char):
                  llvm_colVal = irb->CreateLoad(llvm_next_t_data_ptr);
        break;
        case sizeof(int16):
                  llvm_colVal = irb->CreateLoad(
                      codegen_utils->GetType<int16>(),
                      irb->CreateBitCast(llvm_next_t_data_ptr,
                                         codegen_utils->GetType<int16*>()));
        break;
        case sizeof(int32):
                  llvm_colVal = irb->CreateLoad(
                      codegen_utils->GetType<int32>(),
                      irb->CreateBitCast(llvm_next_t_data_ptr,
                                         codegen_utils->GetType<int32*>()));
        break;
        case sizeof(Datum):
                  llvm_colVal = irb->CreateLoad(
                      codegen_utils->GetType<int64>(),
                      irb->CreateBitCast(llvm_next_t_data_ptr,
                                         codegen_utils->GetType<int64*>()));
        break;
        default:
          // We do not support other data type length, passed by value
          return false;
      }
    } else {
      // We do not support attributes by reference
      return false;
    }

    // store colVal into out_values[attnum]
    irb->CreateStore(
        irb->CreateZExt(llvm_colVal, codegen_utils->GetType<Datum>()),
        llvm_next_values_ptr);

    // }}} End of values[attnum] = fetchatt(thisatt, tp + off)

    // isnull[attnum] = false; {{{
    llvm::Value* llvm_next_isnull_ptr =
        irb->CreateInBoundsGEP(llvm_slot_PRIVATE_tts_isnull,
                               {codegen_utils->GetConstant(attnum)});
    irb->CreateStore(
        codegen_utils->GetConstant<bool>(false),
        llvm_next_isnull_ptr);
    // }}} End of isnull[attnum] = false;

    // Jump to next attribute
    irb->CreateBr(next_attribute_block);

    off += thisatt->attlen;
    // Process next attribute
    attribute_block = next_attribute_block;
  }  // end for

  // Next attribute block
  // ----------------
  // Note that we have iterated over all attributes already,
  // so simply jump to final block.

  irb->SetInsertPoint(next_attribute_block);
  irb->CreateBr(final_block);

  // Final block
  // ----------------

  irb->SetInsertPoint(final_block);

  // slot->PRIVATE_tts_off = off;
  llvm::Value* llvm_slot_PRIVATE_tts_off_ptr /* long* */ =
      codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_off);
  irb->CreateStore(codegen_utils->GetConstant<long>(off),
                   llvm_slot_PRIVATE_tts_off_ptr);

  // slot->PRIVATE_tts_nvalid = attnum;
  irb->CreateStore(llvm_max_attr, llvm_slot_PRIVATE_tts_nvalid_ptr);

  // End of slot_deform_tuple

  // _slot_getsomeattrs() after calling slot_deform_tuple {{{
  // for (; attno < attnum; attno++)
  // {
  //   slot->PRIVATE_tts_values[attno] = (Datum) 0;
  //   slot->PRIVATE_tts_isnull[attno] = true;
  // }

  // TODO(shardikar): Convert these to loops
  llvm::Value* llvm_num_bytes = irb->CreateMul(
      codegen_utils->GetConstant(sizeof(Datum)),
      irb->CreateZExtOrTrunc(
          irb->CreateSub(llvm_max_attr, llvm_attno),
          codegen_utils->GetType<size_t>()));
  codegen_utils->ir_builder()->CreateCall(
      llvm_memset, {
          irb->CreateBitCast(
              irb->CreateInBoundsGEP(llvm_slot_PRIVATE_tts_values,
                                     {llvm_attno}),
              codegen_utils->GetType<void*>()),
              codegen_utils->GetConstant(0),
              llvm_num_bytes});

  codegen_utils->ir_builder()->CreateCall(
      llvm_memset, {
          irb->CreateBitCast(
              irb->CreateInBoundsGEP(llvm_slot_PRIVATE_tts_isnull,
                                     {llvm_attno}),
              codegen_utils->GetType<void*>()),
              codegen_utils->GetConstant(static_cast<int>(true)),
              llvm_num_bytes});

  // TupSetVirtualTuple(slot);
  irb->CreateStore(
      irb->CreateOr(
          irb->CreateLoad(llvm_slot_PRIVATE_tts_flags_ptr),
          codegen_utils->GetConstant<int>(TTS_VIRTUAL)),
          llvm_slot_PRIVATE_tts_flags_ptr);

  // }}}

  // slot_getattr() after calling _slot_getsomeattrs() and remainder of
  // ExecVariableList {{{
  //
  // This code from ExecVariableList copies the contents of the isnull & values
  // arrays in the slot to output variable from the function parameters to
  // ExecVariableList
  int  *varNumbers = proj_info_->pi_varNumbers;
  for (int i = list_length(proj_info_->pi_targetlist) - 1; i >= 0; i--) {
    // *isnull = slot->PRIVATE_tts_isnull[attnum-1];
    llvm::Value* llvm_isnull_from_slot_val =
        irb->CreateLoad(irb->CreateInBoundsGEP(
              llvm_slot_PRIVATE_tts_isnull,
              {codegen_utils->GetConstant(varNumbers[i] - 1)}));
    llvm::Value* llvm_isnull_ptr =
        irb->CreateInBoundsGEP(llvm_isnull_arg,
                               {codegen_utils->GetConstant(i)});
    irb->CreateStore(llvm_isnull_from_slot_val, llvm_isnull_ptr);

    // values[i] = slot_getattr(varSlot, varNumber+1, &(isnull[i]));
    llvm::Value* llvm_value_from_slot_val =
        irb->CreateLoad(irb->CreateInBoundsGEP(
              llvm_slot_PRIVATE_tts_values,
              {codegen_utils->GetConstant(varNumbers[i] - 1)}));
    llvm::Value* llvm_values_ptr =
        irb->CreateInBoundsGEP(llvm_values_arg,
                               {codegen_utils->GetConstant(i)});
    irb->CreateStore(llvm_value_from_slot_val, llvm_values_ptr);
  }


  // We're all done!
  codegen_utils->ir_builder()->CreateRetVoid();

  // Fall back block
  // ---------------
  // Note: We collect error code information, based on the block from which we
  // fall back, and log it for debugging purposes.
  irb->SetInsertPoint(fallback_block);
  llvm::PHINode* llvm_error = irb->CreatePHI(codegen_utils->GetType<int>(), 4);
  llvm_error->addIncoming(codegen_utils->GetConstant(0),
      slot_check_block);
  llvm_error->addIncoming(codegen_utils->GetConstant(1),
      tuple_type_check_block);
  llvm_error->addIncoming(codegen_utils->GetConstant(2),
      heap_tuple_check_block);

  elogwrapper.CreateElog(
      DEBUG1,
      "Falling back to regular ExecVariableList, reason = %d",
      llvm_error);

  codegen_utils->CreateFallback<ExecVariableListFn>(
      codegen_utils->RegisterExternalFunction(GetRegularFuncPointer()),
      exec_variable_list_func);
  return true;
}


bool ExecVariableListCodegen::GenerateCodeInternal(
    CodegenUtils* codegen_utils) {
  bool isGenerated = GenerateExecVariableList(codegen_utils);

  if (isGenerated) {
    elog(DEBUG1, "ExecVariableList was generated successfully!");
    return true;
  } else {
    elog(DEBUG1, "ExecVariableList generation failed!");
    return false;
  }
}
