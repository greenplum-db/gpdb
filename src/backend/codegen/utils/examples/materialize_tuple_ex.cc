//---------------------------------------------------------------------------
//  Greenplum Database
//  Copyright 2016 Pivotal Software, Inc.
//
//  @filename:
//    materialize_tuple_ex.cc
//
//  @doc:
//    Contains an example that implements the MaterializeTuple() function that 
//    uses type information available at runtime to generate an output tuple 
//    in a buffer.
//
//  @test:
//
//
//---------------------------------------------------------------------------

#include <cstdio>

#include "codegen/utils/code_generator.h"
#include "codegen/utils/utility.h"
#include "llvm/IR/Verifier.h"

namespace {

typedef void (*MaterializeTupleFunction)(char *);

enum Types {
  kBool = 0, // 1 byte
  kInt       // 4 bytes
};

const int kTupleSize = 9;
const int kNumSlots = 3;
const int kOffsets[] = {0, 4, 5};
const Types kTypes[] = {Types::kInt, Types::kBool, Types::kInt};

bool ParseBool() { return true; }
int ParseInt() { return 6513; }

void PrintTuple(char *tuple, int len) {
  for (int i = 0; i < len; ++i) {
    std::printf("%02x ", static_cast<int>(tuple[i]));
  }
  std::printf("\n");
}

void testMaterializeTuple(MaterializeTupleFunction func) {
  char *tuple = new char[kTupleSize]();
  PrintTuple(tuple, kTupleSize);
  func(tuple);
  PrintTuple(tuple, kTupleSize);
  delete[] tuple;
}

void materializeTuple(char *tuple) {
  for (int i = 0; i < kNumSlots; ++i) {
    char *slot = tuple + kOffsets[i];
    switch (kTypes[i]) {
    case Types::kBool:
      *slot = ParseBool();
      break;
    case Types::kInt:
      *reinterpret_cast<int *>(slot) = ParseInt();
      break;
    }
  }
}

// The following method tries to faithfully reproduce the semantics of the
// MaterializeTuple function (implemented natively above).
//
// The reader should note that this possible misses the point of codegen in
// that since we know all the types and offsets at the time of code generation,
// we can actually unroll the loop and eliminate any branching by type here.
//
// This implementation does illustrate the basic blocks and branches necessary
// when implementing a for loop, a switch construct, type conversion and
// external
//
MaterializeTupleFunction
GenerateCodeForMaterializeTuple(gpcodegen::CodeGenerator *code_generator) {

  auto irb = code_generator->ir_builder();
  llvm::Function *mt_function =
      code_generator->CreateFunction<void, char *>("materializeTuple");

  // "Constants"
  llvm::Value *kNumSlots_value = code_generator->GetConstant(kNumSlots);
  llvm::Value *kOffsets_array = code_generator->GetConstant(kOffsets);
  llvm::Value *kTypes_array = code_generator->GetConstant(kTypes);
  llvm::Value *bool_type = code_generator->GetConstant(Types::kBool);
  llvm::Value *int_type = code_generator->GetConstant(Types::kInt);
  // "External functions"
  llvm::Function *parseInt_f =
      code_generator->RegisterExternalFunction(ParseInt);
  llvm::Function *parseBool_f =
      code_generator->RegisterExternalFunction(ParseBool);

  /*
   *
   *        +-------+
   *        | entry |
   *        +-------+
   *            |
   *            V
   *     +------------+
   *  +->| loop_start |
   *  |  +------------+
   *  |         |                +-------------+
   *  |         V          +---->| switch_bool |--+
   *  |   +-----------+    |     +-------------+  |
   *  |   | loop_main |----|                      |
   *  |   +-----------+    |     +------------+   |
   *  |                    +---->| switch_int |---|
   *  |  +-------------+         +------------+   |
   *  +--| switch_term |<-------------------------+
   *     +-------------+
   *            |
   *            V
   *      +-----------+
   *      | loop_term |
   *      +-----------+
   */

  // "Blocks"
  llvm::BasicBlock *entry =
      code_generator->CreateBasicBlock("entry", mt_function);
  llvm::BasicBlock *loop_start =
      code_generator->CreateBasicBlock("loop_start", mt_function);
  llvm::BasicBlock *loop_main =
      code_generator->CreateBasicBlock("loop_main", mt_function);
  llvm::BasicBlock *switch_term =
      code_generator->CreateBasicBlock("switch_term", mt_function);
  llvm::BasicBlock *switch_bool =
      code_generator->CreateBasicBlock("switch_bool", mt_function);
  llvm::BasicBlock *switch_int =
      code_generator->CreateBasicBlock("switch_int", mt_function);
  llvm::BasicBlock *loop_term =
      code_generator->CreateBasicBlock("loop_term", mt_function);

  // "Input Arguments"
  llvm::Value *tuple_arg =
      (llvm::Value *)gpcodegen::ArgumentByPosition(mt_function, 0);

  // entry block
  irb->SetInsertPoint(entry);
  irb->CreateBr(loop_start);

  // loop-start block
  irb->SetInsertPoint(loop_start);
  llvm::PHINode *kOffsets_index = irb->CreatePHI(code_generator->GetType<int>(),
                                                 2 /* num of incoming edges */);
  kOffsets_index->addIncoming(code_generator->GetConstant(0), entry);

  // kOffsets_index == kNumSlots_value
  irb->CreateCondBr(irb->CreateICmpEQ(kOffsets_index, kNumSlots_value),
                    loop_term, loop_main);

  // loop-main block
  irb->SetInsertPoint(loop_main);
  // Type offset_value = kOffsets_array[kOffsets_index]
  llvm::Value *offset_value =
      irb->CreateLoad(code_generator->ir_builder()->CreateInBoundsGEP(
          code_generator->GetType<int>(), kOffsets_array, kOffsets_index));
  // Type type_value = kTypes_array[kOffsets_index]
  llvm::Value *type_value =
      irb->CreateLoad(code_generator->ir_builder()->CreateInBoundsGEP(
          code_generator->GetType<Types>(), kTypes_array, kOffsets_index));
  // char* slot_ptr = tuple_arg[offset_value]
  llvm::Value *slot_ptr = irb->CreateInBoundsGEP(
      code_generator->GetType<char>(), tuple_arg, offset_value);

  // switch(type)
  llvm::SwitchInst *switch_ins = irb->CreateSwitch(type_value, switch_term, 3);
  switch_ins->addCase(static_cast<llvm::ConstantInt *>(bool_type), switch_bool);
  switch_ins->addCase(static_cast<llvm::ConstantInt *>(int_type), switch_int);

  { // case kBool:
    irb->SetInsertPoint(switch_bool);
    // *slot_ptr = (char) parseBool_f()
    llvm::Value *parsed_value = irb->CreateCall(parseBool_f, {});
    irb->CreateStore(
        irb->CreateZExt(parsed_value, code_generator->GetType<char>()),
        slot_ptr);
    irb->CreateBr(switch_term);
  }

  { // case kInt:
    irb->SetInsertPoint(switch_int);
    // *(int*)slot_ptr = parseInt_f()
    llvm::Value *parsed_value = irb->CreateCall(parseInt_f, {});
    irb->CreateStore(
        parsed_value,
        irb->CreateBitCast(slot_ptr, code_generator->GetType<int *>()));
    irb->CreateBr(switch_term);
  }

  // switch-term block
  irb->SetInsertPoint(switch_term);

  // kOffsets_index++
  llvm::Value *next_kOffsets_index =
      irb->CreateAdd(kOffsets_index, code_generator->GetConstant(1));
  irb->CreateBr(loop_start);
  kOffsets_index->addIncoming(next_kOffsets_index, switch_term);

  // loop term block
  irb->SetInsertPoint(loop_term);
  irb->CreateRetVoid();

  // Verification
  assert(!llvm::verifyFunction(*mt_function));
  assert(!llvm::verifyModule(*code_generator->module()));

  bool prepare_ok = code_generator->PrepareForExecution(
             gpcodegen::CodeGenerator::OptimizationLevel::kDefault, true);
  assert(prepare_ok);

  return code_generator->GetFunctionPointer<void, char *>("materializeTuple");
}
}

int main() {
  bool init_ok = gpcodegen::CodeGenerator::InitializeGlobal();
  assert(init_ok);

  std::printf("Testing static compiled version:\n");
  gpcodegen::CodeGenerator code_generator("materializeTuple");
  testMaterializeTuple(materializeTuple);

  std::printf("Testing JIT compiled version:\n");
  auto materializeTupleCompiled =
      GenerateCodeForMaterializeTuple(&code_generator);
  testMaterializeTuple(materializeTupleCompiled);
  return 0;
}
