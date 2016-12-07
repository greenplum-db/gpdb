//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright (C) 2016 Pivotal Software, Inc.
//
//  @filename:
//    slot_getattr_codegen.cc
//
//  @doc:
//    Contains slot_getattr generator
//
//---------------------------------------------------------------------------
#include <assert.h>
#include <string.h>
#include <string>

#include "codegen/base_codegen.h"
#include "codegen/codegen_wrapper.h"
#include "codegen/codegen_config.h"
#include "codegen/codegen_manager.h"
#include "codegen/slot_getattr_codegen.h"
#include "codegen/utils/gp_codegen_utils.h"
#include "codegen/utils/utility.h"

#include "llvm/IR/Argument.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"

extern "C" {
#include "postgres.h"  // NOLINT(build/include)
#include "c.h"  // NOLINT(build/include)
#include "executor/tuptable.h"
#include "utils/elog.h"
#include "access/htup.h"
#include "access/memtup.h"
#include "access/tupdesc.h"
#include "access/tupmacs.h"
#include "catalog/pg_attribute.h"

extern void slot_deform_tuple(TupleTableSlot* slot, int nattr);
}

namespace llvm {
class BasicBlock;
class Value;
}  // namespace llvm


using gpcodegen::SlotGetAttrCodegen;

constexpr char SlotGetAttrCodegen::kSlotGetAttrPrefix[];

std::unordered_map<
  gpcodegen::CodegenManager*, SlotGetAttrCodegen::SlotGetAttrCodegenCache>
    SlotGetAttrCodegen::codegen_cache_by_manager;

SlotGetAttrCodegen* SlotGetAttrCodegen::GetCodegenInstance(
    gpcodegen::CodegenManager* manager,
    TupleTableSlot *slot,
    int max_attr) {

  // TODO(krajaraman, frahman) : Refactor so creation happens through
  // CodegenManager::CreateAndEnrollGenerator. In that case, we don't
  // need to do this `if` condition here.
  if (!CodegenConfig::IsGeneratorEnabled<SlotGetAttrCodegen>()) {
    return nullptr;
  }

  // Create an cache entry for this manager if it doesn't already exist
  auto it = codegen_cache_by_manager[manager].find(slot);

  SlotGetAttrCodegen* generator = nullptr;
  if (it != codegen_cache_by_manager[manager].end()) {
    // For a slot already seen before, update max_attr value only
    generator = it->second;
    generator->max_attr_ = std::max(generator->max_attr_, max_attr);
  } else {
    // TODO(krajaraman, frahman) : Refactor so creation happens through
    // CodegenManager::CreateAndEnrollGenerator.
    // For a slot we haven't see before, create and add a new object
    generator = new SlotGetAttrCodegen(manager, slot, max_attr);
    codegen_cache_by_manager[manager].insert(std::make_pair(slot, generator));
    // Enroll this in the manager so that it can take ownership
    manager->EnrollCodeGenerator(CodegenFuncLifespan_Parameter_Invariant,
                                 generator);
  }

  assert(nullptr != generator);
  return generator;
}

void SlotGetAttrCodegen::RemoveSelfFromCache() {
  CodegenManager* manager = this->manager();

  assert(manager != nullptr &&
         codegen_cache_by_manager.find(manager) !=
             codegen_cache_by_manager.end());
  std::unordered_map<TupleTableSlot*, SlotGetAttrCodegen*>& cache_map =
      codegen_cache_by_manager[manager];
  auto it = codegen_cache_by_manager[manager].find(slot_);

  assert(it != cache_map.end() && it->second == this);
  // Delete this SlotGetAttr from it's manager
  cache_map.erase(it);

  // Clean up manager data out of the map if this was the last instance
  if (cache_map.empty()) {
    codegen_cache_by_manager.erase(manager);
  }
}

SlotGetAttrCodegen::~SlotGetAttrCodegen() {
  RemoveSelfFromCache();
}

bool SlotGetAttrCodegen::GenerateCodeInternal(
    gpcodegen::GpCodegenUtils* codegen_utils) {

  // This function may be called multiple times, but it should generate code
  // only once
  if (IsGenerated()) {
    return true;
  }

  // Give the function a human readable name
  std::string function_name = GetUniqueFuncName() + "_" +
      std::to_string(reinterpret_cast<uint64_t>(slot_)) + "_" +
      std::to_string(max_attr_);
  llvm::Function* function = CreateFunction<SlotGetAttrFn>(codegen_utils,
                                                           function_name);

  bool isGenerated = GenerateSlotGetAttr(
      codegen_utils, slot_, max_attr_, function);

  if (isGenerated) {
    elog(DEBUG1, "slot_getattr was generated successfully!");
    assert(nullptr != function);
    llvm_function_ = function;
    return true;
  } else {
    elog(DEBUG1, "slot_getattr generation failed!");
    llvm_function_ = nullptr;
    return false;
  }
}

bool SlotGetAttrCodegen::GenerateSlotGetAttr(
    gpcodegen::GpCodegenUtils* codegen_utils,
    TupleTableSlot *slot,
    int max_attr,
    llvm::Function* slot_getattr_func) {

  // So looks like we're going to generate code
  auto irb = codegen_utils->ir_builder();

  // BasicBlock of function entry.
  llvm::BasicBlock* entry_block = codegen_utils->CreateBasicBlock(
      "entry", slot_getattr_func);
  // BasicBlock for checking correct slot
  llvm::BasicBlock* slot_check_block = codegen_utils->CreateBasicBlock(
      "slot_check", slot_getattr_func);
  // BasicBlock for checking virtual tuple type.
  llvm::BasicBlock* virtual_tuple_check_block = codegen_utils->CreateBasicBlock(
      "virtual_tuple_check", slot_getattr_func);
  // BasicBlock for checking memtuple type.
  llvm::BasicBlock* memtuple_check_block = codegen_utils->CreateBasicBlock(
      "memtuple_check", slot_getattr_func);
  // BasicBlock for handling memtuple.
  llvm::BasicBlock* memtuple_block = codegen_utils->CreateBasicBlock(
      "memtuple", slot_getattr_func);
  // BasicBlock for heap tuple check.
  llvm::BasicBlock* heap_tuple_check_block = codegen_utils->CreateBasicBlock(
      "heap_tuple_check", slot_getattr_func);
  // BasicBlock for main.
  llvm::BasicBlock* main_block = codegen_utils->CreateBasicBlock(
      "main", slot_getattr_func);
  // BasicBlock for final computations.
  llvm::BasicBlock* final_block = codegen_utils->CreateBasicBlock(
      "final", slot_getattr_func);
  // BasicBlock for return
  llvm::BasicBlock* return_block = codegen_utils->CreateBasicBlock(
      "return", slot_getattr_func);
  // BasicBlock for fall back.
  llvm::BasicBlock* fallback_block = codegen_utils->CreateBasicBlock(
      "fallback", slot_getattr_func);

  // External functions
  llvm::Function* llvm_memset =
      codegen_utils->GetOrRegisterExternalFunction(memset, "memset");
  llvm::Function* llvm_memtuple_getattr =
      codegen_utils->GetOrRegisterExternalFunction(memtuple_getattr,
                                                   "memtuple_getattr");
  llvm::Function* llvm_slot_deform_tuple =
      codegen_utils->GetOrRegisterExternalFunction(slot_deform_tuple,
                                                   "slot_deform_tuple");
  llvm::Function* llvm_att_align_nominal =
      codegen_utils->GetOrRegisterExternalFunction(att_align_nominal_regular,
                                                   "att_align_nominal");

  // Generation-time constants
  llvm::Value* llvm_slot = codegen_utils->GetConstant(slot);
  llvm::Value* llvm_max_attr = codegen_utils->GetConstant(max_attr);

  // Function arguments to slot_getattr
  llvm::Value* llvm_slot_arg = ArgumentByPosition(slot_getattr_func, 0);
  llvm::Value* llvm_attnum_arg = ArgumentByPosition(slot_getattr_func, 1);
  llvm::Value* llvm_isnull_ptr_arg = ArgumentByPosition(slot_getattr_func, 2);

  // Entry block
  // -----------

  irb->SetInsertPoint(entry_block);
#ifdef CODEGEN_DEBUG
  EXPAND_CREATE_ELOG(codegen_utils,
                     DEBUG1,
                     "Codegen'ed slot_getattr called!");
#endif
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
  llvm::Value* llvm_slot_tts_mt_bind /* MemTupleBinding* */ =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::tts_mt_bind));

  // We start a sequence of checks to ensure that everything is fine and
  // we do not need to fall back.
  irb->CreateBr(slot_check_block);


  // Slot check block
  // ----------------
  irb->SetInsertPoint(slot_check_block);
  // Compare slot given during code generation and the one passed
  // in as an argument to slot_getattr
  irb->CreateCondBr(
      irb->CreateICmpEQ(llvm_slot, llvm_slot_arg),
      virtual_tuple_check_block /* true */,
      fallback_block /* false */);

  // Virtual tuple check block
  // -------------------------

  irb->SetInsertPoint(virtual_tuple_check_block);
  llvm::Value* llvm_slot_PRIVATE_tts_flags_ptr =
      codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_flags);
  // (slot->PRIVATE_tts_flags & TTS_VIRTUAL) != 0 (= TupHasVirtualTuple(slot))
  llvm::Value* llvm_tuple_is_virtual_1 = irb->CreateICmpNE(
      irb->CreateAnd(
          irb->CreateLoad(llvm_slot_PRIVATE_tts_flags_ptr),
          codegen_utils->GetConstant(TTS_VIRTUAL)),
          codegen_utils->GetConstant(0));
  // slot->PRIVATE_tts_nvalid >= attnum
  llvm::Value* llvm_tuple_is_virtual_2 = irb->
      CreateICmpSGE(irb->CreateLoad(llvm_slot_PRIVATE_tts_nvalid_ptr),
                    llvm_attnum_arg);
  // TupHasVirtualTuple(slot) && slot->PRIVATE_tts_nvalid >= attnum
  llvm::Value* llvm_tuple_is_virtual = irb->CreateAnd(llvm_tuple_is_virtual_1,
                                                      llvm_tuple_is_virtual_2);
  // If it is indeed a virtual tuple, we must have already deformed this tuple,
  // so go straight to returning the contained values
  irb->CreateCondBr(
      llvm_tuple_is_virtual,
      return_block /* true */,
      memtuple_check_block /* false */);


  // Memtuple type check block
  // ----------------------

  irb->SetInsertPoint(memtuple_check_block);

  // slot->PRIVATE_tts_memtuple != NULL
  llvm::Value* llvm_slot_PRIVATE_tts_memtuple =
      irb->CreateLoad(codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_memtuple));
  llvm::Value* llvm_tuple_has_memtuple = irb->CreateICmpNE(
      llvm_slot_PRIVATE_tts_memtuple,
      codegen_utils->GetConstant((MemTuple) NULL));

  irb->CreateCondBr(
      llvm_tuple_has_memtuple,
      memtuple_block /*true*/, heap_tuple_check_block /*false*/);

  // Memtuple Block
  // --------------

  irb->SetInsertPoint(memtuple_block);
  // return memtuple_getattr(slot->PRIVATE_tts_memtuple,
  //    slot->tts_mt_bind, attnum, isnull);
  llvm::Value* llvm_memtuple_ret = irb->CreateCall(llvm_memtuple_getattr, {
      llvm_slot_PRIVATE_tts_memtuple,
      llvm_slot_tts_mt_bind,
      llvm_attnum_arg,
      llvm_isnull_ptr_arg});
  irb->CreateRet(llvm_memtuple_ret);


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

  // int off = 0;
  llvm::Value* llvm_off_ptr = irb->CreateAlloca(
      codegen_utils->GetType<int>(), nullptr, "off");
  irb->CreateStore(codegen_utils->GetConstant<int>(0), llvm_off_ptr);

  TupleDesc tupleDesc = slot->tts_tupleDescriptor;
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
      "attribute_block_"+0, slot_getattr_func);

  irb->CreateBr(attribute_block);

  bool varlen_attrs_found = false;
  int attnum = 0;
  for (; attnum < max_attr; ++attnum) {
    Form_pg_attribute thisatt = att[attnum];

    // If any thisatt is varlen
    if (thisatt->attlen < 0) {
      // When we have variable length attributes, we can no longer benefit
      // from codegen, since the next offset needs to be computed after the
      // tuple is read into memory.
      if (attnum < codegen_varlen_tolerance) {
        // Also, if one of the first few attributes is varlen, might as well
        // call slot_deform_tuple directly, instead of going through a codegen'd
        // wrapper function.
        return false;
      }
      varlen_attrs_found = true;
      break;
    }

    // ith attribute's block
    // ----------
    // Check if attribute can be null. If yes, then create blocks
    // to handle null and not null cases.
    irb->SetInsertPoint(attribute_block);

    // Create the block of (attnum+1)th attribute and jump to it after
    // you finish with current attribute.
    next_attribute_block = codegen_utils->CreateBasicBlock(
        "attribute_block_" + std::to_string(attnum+1), slot_getattr_func);

    llvm::Value* llvm_next_values_ptr =
        irb->CreateInBoundsGEP(llvm_slot_PRIVATE_tts_values,
                               {codegen_utils->GetConstant(attnum)});

    // If attribute can be null, then create blocks to handle
    // null and not null cases.
    if (!thisatt->attnotnull) {
      // Create blocks
      is_null_block = codegen_utils->CreateBasicBlock(
          "is_null_block_" + std::to_string(attnum), slot_getattr_func);
      is_not_null_block = codegen_utils->CreateBasicBlock(
          "is_not_null_block_" + std::to_string(attnum),
          slot_getattr_func);

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
      // (Exp_1 & Expr_2)
      llvm::Value* llvm_expr1_and_expr2 = irb->CreateICmpNE(
          irb->CreateAnd(llvm_expr_1, llvm_expr_2),
          codegen_utils->GetConstant<int8>(0));

      // !(Expr_1 & Expr_2)
      llvm::Value* llvm_att_isnull = irb->CreateNot(llvm_expr1_and_expr2,
                                                    "llvm_att_isnull");
      // }}

      llvm::Value* llvm_hasnulls = irb->CreateICmpNE(
          irb->CreateAnd(llvm_heaptuple_t_data_t_infomask,
                         codegen_utils->GetConstant<uint16>(HEAP_HASNULL)),
                         codegen_utils->GetConstant<uint16>(0),
                         "llvm_hasnulls");

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

    // off = att_align_nominal(off, thisatt->attalign);
    irb->CreateStore(irb->CreateCall(
        llvm_att_align_nominal, {irb->CreateLoad(llvm_off_ptr),
            codegen_utils->GetConstant<char>(thisatt->attalign)}),
                     llvm_off_ptr);

    // values[attnum] = fetchatt(thisatt, tp + off) {{{
    llvm::Value* llvm_next_t_data_ptr =
        irb->CreateInBoundsGEP(llvm_tuple_data_ptr,
                               {irb->CreateLoad(llvm_off_ptr)});

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
          elog(DEBUG1,
               "We do not support other data type length, passed by value");
          return false;
      }
    } else {
      // We do not support attributes by reference
      elog(DEBUG1, "We do not support attributes by reference");
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

    // off += thisatt->attlen;
    irb->CreateStore(irb->CreateAdd(
        irb->CreateLoad(llvm_off_ptr),
        codegen_utils->GetConstant<int>(thisatt->attlen)),
                     llvm_off_ptr);

    // Jump to next attribute
    irb->CreateBr(next_attribute_block);

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
  // Save the state for the next execution

  irb->SetInsertPoint(final_block);

  // slot->PRIVATE_tts_off = off;
  llvm::Value* llvm_slot_PRIVATE_tts_off_ptr /* long* */ =
      codegen_utils->GetPointerToMember(
          llvm_slot, &TupleTableSlot::PRIVATE_tts_off);
  irb->CreateStore(
      codegen_utils->CreateCast<long, int>(  // NOLINT(runtime/int)
          irb->CreateLoad(llvm_off_ptr)), llvm_slot_PRIVATE_tts_off_ptr);

  // slot->PRIVATE_tts_nvalid = attnum;
  irb->CreateStore(codegen_utils->GetConstant(attnum),
                   llvm_slot_PRIVATE_tts_nvalid_ptr);

  if (varlen_attrs_found) {
    // If we encountered any varlen attribute, we stop codegen from that
    // attribute onwards and call slot_deform_tuple() directly to deform the
    // rest of the tuple.
    irb->CreateCall(llvm_slot_deform_tuple, {
        llvm_slot_arg,
        llvm_max_attr});
  }

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
  irb->CreateBr(return_block);

  // Return block
  // ------------
  // slot_getattr() after calling _slot_getsomeattrs() {{{

  irb->SetInsertPoint(return_block);

  // *isnull = slot->PRIVATE_tts_isnull[attnum-1];
  llvm::Value* llvm_isnull_from_slot_val =
      irb->CreateLoad(irb->CreateInBoundsGEP(
            llvm_slot_PRIVATE_tts_isnull,
            {irb->CreateSub(llvm_attnum_arg, codegen_utils->GetConstant(1))}));
  irb->CreateStore(llvm_isnull_from_slot_val, llvm_isnull_ptr_arg);

  // return slot->PRIVATE_tts_values[attnum-1];
  llvm::Value* llvm_value_from_slot_val =
      irb->CreateLoad(irb->CreateInBoundsGEP(
            llvm_slot_PRIVATE_tts_values,
            {irb->CreateSub(llvm_attnum_arg, codegen_utils->GetConstant(1))}));
  irb->CreateRet(llvm_value_from_slot_val);

  // Fall back block
  // ---------------
  // Note: We collect error code information, based on the block from which we
  // fall back, and log it for debugging purposes.
  irb->SetInsertPoint(fallback_block);
  llvm::PHINode* llvm_error = irb->
      CreatePHI(codegen_utils->GetType<char *>(), 2);
  llvm_error->addIncoming(codegen_utils->GetConstant("slot check failed"),
      slot_check_block);
  llvm_error->addIncoming(codegen_utils->GetConstant("tuple check failed"),
      heap_tuple_check_block);

  EXPAND_CREATE_ELOG(codegen_utils,
                     DEBUG1,
                     "Falling back to regular slot_getattr, reason = %s",
                     llvm_error);

  codegen_utils->CreateFallback<SlotGetAttrFn>(
      codegen_utils->GetOrRegisterExternalFunction(slot_getattr_regular,
                                                   "slot_getattr_regular"),
      slot_getattr_func);
  return true;
}
