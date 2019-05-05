// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
#define V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_

// Do only include this header for implementing new Interface of the
// WasmFullDecoder.

#include "src/base/platform/elapsed-timer.h"
#include "src/bit-vector.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

struct WasmGlobal;
struct WasmException;

#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_INST_FORMAT "  @%-8d #%-20s|"

// Return the evaluation of `condition` if validate==true, DCHECK that it's
// true and always return true otherwise.
#define VALIDATE(condition)       \
  (validate ? (condition) : [&] { \
    DCHECK(condition);            \
    return true;                  \
  }())

#define RET_ON_PROTOTYPE_OPCODE(feat)                                          \
  DCHECK(!this->module_ || this->module_->origin == kWasmOrigin);              \
  if (!this->enabled_.feat) {                                                  \
    this->error("Invalid opcode (enable with --experimental-wasm-" #feat ")"); \
  } else {                                                                     \
    this->detected_->feat = true;                                              \
  }

#define CHECK_PROTOTYPE_OPCODE(feat)                                           \
  DCHECK(!this->module_ || this->module_->origin == kWasmOrigin);              \
  if (!this->enabled_.feat) {                                                  \
    this->error("Invalid opcode (enable with --experimental-wasm-" #feat ")"); \
    break;                                                                     \
  } else {                                                                     \
    this->detected_->feat = true;                                              \
  }

#define OPCODE_ERROR(opcode, message)                                 \
  (this->errorf(this->pc_, "%s: %s", WasmOpcodes::OpcodeName(opcode), \
                (message)))

#define ATOMIC_OP_LIST(V)                \
  V(AtomicWake, Uint32)                  \
  V(I32AtomicWait, Uint32)               \
  V(I64AtomicWait, Uint32)               \
  V(I32AtomicLoad, Uint32)               \
  V(I64AtomicLoad, Uint64)               \
  V(I32AtomicLoad8U, Uint8)              \
  V(I32AtomicLoad16U, Uint16)            \
  V(I64AtomicLoad8U, Uint8)              \
  V(I64AtomicLoad16U, Uint16)            \
  V(I64AtomicLoad32U, Uint32)            \
  V(I32AtomicAdd, Uint32)                \
  V(I32AtomicAdd8U, Uint8)               \
  V(I32AtomicAdd16U, Uint16)             \
  V(I64AtomicAdd, Uint64)                \
  V(I64AtomicAdd8U, Uint8)               \
  V(I64AtomicAdd16U, Uint16)             \
  V(I64AtomicAdd32U, Uint32)             \
  V(I32AtomicSub, Uint32)                \
  V(I64AtomicSub, Uint64)                \
  V(I32AtomicSub8U, Uint8)               \
  V(I32AtomicSub16U, Uint16)             \
  V(I64AtomicSub8U, Uint8)               \
  V(I64AtomicSub16U, Uint16)             \
  V(I64AtomicSub32U, Uint32)             \
  V(I32AtomicAnd, Uint32)                \
  V(I64AtomicAnd, Uint64)                \
  V(I32AtomicAnd8U, Uint8)               \
  V(I32AtomicAnd16U, Uint16)             \
  V(I64AtomicAnd8U, Uint8)               \
  V(I64AtomicAnd16U, Uint16)             \
  V(I64AtomicAnd32U, Uint32)             \
  V(I32AtomicOr, Uint32)                 \
  V(I64AtomicOr, Uint64)                 \
  V(I32AtomicOr8U, Uint8)                \
  V(I32AtomicOr16U, Uint16)              \
  V(I64AtomicOr8U, Uint8)                \
  V(I64AtomicOr16U, Uint16)              \
  V(I64AtomicOr32U, Uint32)              \
  V(I32AtomicXor, Uint32)                \
  V(I64AtomicXor, Uint64)                \
  V(I32AtomicXor8U, Uint8)               \
  V(I32AtomicXor16U, Uint16)             \
  V(I64AtomicXor8U, Uint8)               \
  V(I64AtomicXor16U, Uint16)             \
  V(I64AtomicXor32U, Uint32)             \
  V(I32AtomicExchange, Uint32)           \
  V(I64AtomicExchange, Uint64)           \
  V(I32AtomicExchange8U, Uint8)          \
  V(I32AtomicExchange16U, Uint16)        \
  V(I64AtomicExchange8U, Uint8)          \
  V(I64AtomicExchange16U, Uint16)        \
  V(I64AtomicExchange32U, Uint32)        \
  V(I32AtomicCompareExchange, Uint32)    \
  V(I64AtomicCompareExchange, Uint64)    \
  V(I32AtomicCompareExchange8U, Uint8)   \
  V(I32AtomicCompareExchange16U, Uint16) \
  V(I64AtomicCompareExchange8U, Uint8)   \
  V(I64AtomicCompareExchange16U, Uint16) \
  V(I64AtomicCompareExchange32U, Uint32)

#define ATOMIC_STORE_OP_LIST(V) \
  V(I32AtomicStore, Uint32)     \
  V(I64AtomicStore, Uint64)     \
  V(I32AtomicStore8U, Uint8)    \
  V(I32AtomicStore16U, Uint16)  \
  V(I64AtomicStore8U, Uint8)    \
  V(I64AtomicStore16U, Uint16)  \
  V(I64AtomicStore32U, Uint32)

// Helpers for decoding different kinds of immediates which follow bytecodes.
template <Decoder::ValidateFlag validate>
struct LocalIndexImmediate {
  uint32_t index;
  ValueType type = kWasmStmt;
  uint32_t length;

  inline LocalIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "local index");
  }
};

template <Decoder::ValidateFlag validate>
struct ExceptionIndexImmediate {
  uint32_t index;
  const WasmException* exception = nullptr;
  uint32_t length;

  inline ExceptionIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "exception index");
  }
};

template <Decoder::ValidateFlag validate>
struct ImmI32Immediate {
  int32_t value;
  uint32_t length;
  inline ImmI32Immediate(Decoder* decoder, const byte* pc) {
    value = decoder->read_i32v<validate>(pc + 1, &length, "immi32");
  }
};

template <Decoder::ValidateFlag validate>
struct ImmI64Immediate {
  int64_t value;
  uint32_t length;
  inline ImmI64Immediate(Decoder* decoder, const byte* pc) {
    value = decoder->read_i64v<validate>(pc + 1, &length, "immi64");
  }
};

template <Decoder::ValidateFlag validate>
struct ImmF32Immediate {
  float value;
  uint32_t length = 4;
  inline ImmF32Immediate(Decoder* decoder, const byte* pc) {
    // Avoid bit_cast because it might not preserve the signalling bit of a NaN.
    uint32_t tmp = decoder->read_u32<validate>(pc + 1, "immf32");
    memcpy(&value, &tmp, sizeof(value));
  }
};

template <Decoder::ValidateFlag validate>
struct ImmF64Immediate {
  double value;
  uint32_t length = 8;
  inline ImmF64Immediate(Decoder* decoder, const byte* pc) {
    // Avoid bit_cast because it might not preserve the signalling bit of a NaN.
    uint64_t tmp = decoder->read_u64<validate>(pc + 1, "immf64");
    memcpy(&value, &tmp, sizeof(value));
  }
};

template <Decoder::ValidateFlag validate>
struct GlobalIndexImmediate {
  uint32_t index;
  ValueType type = kWasmStmt;
  const WasmGlobal* global = nullptr;
  uint32_t length;

  inline GlobalIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "global index");
  }
};

template <Decoder::ValidateFlag validate>
struct BlockTypeImmediate {
  uint32_t length = 1;
  ValueType type = kWasmStmt;
  uint32_t sig_index = 0;
  FunctionSig* sig = nullptr;

  inline BlockTypeImmediate(const WasmFeatures& enabled, Decoder* decoder,
                            const byte* pc) {
    uint8_t val = decoder->read_u8<validate>(pc + 1, "block type");
    if (!decode_local_type(val, &type)) {
      // Handle multi-value blocks.
      if (!VALIDATE(enabled.mv)) {
        decoder->error(pc + 1, "invalid block type");
        return;
      }
      if (!VALIDATE(decoder->ok())) return;
      int32_t index =
          decoder->read_i32v<validate>(pc + 1, &length, "block arity");
      if (!VALIDATE(length > 0 && index >= 0)) {
        decoder->error(pc + 1, "invalid block type index");
        return;
      }
      sig_index = static_cast<uint32_t>(index);
    }
  }

  // Decode a byte representing a local type. Return {false} if the encoded
  // byte was invalid or the start of a type index.
  inline bool decode_local_type(uint8_t val, ValueType* result) {
    switch (static_cast<ValueTypeCode>(val)) {
      case kLocalVoid:
        *result = kWasmStmt;
        return true;
      case kLocalI32:
        *result = kWasmI32;
        return true;
      case kLocalI64:
        *result = kWasmI64;
        return true;
      case kLocalF32:
        *result = kWasmF32;
        return true;
      case kLocalF64:
        *result = kWasmF64;
        return true;
      case kLocalS128:
        *result = kWasmS128;
        return true;
      case kLocalAnyFunc:
        *result = kWasmAnyFunc;
        return true;
      case kLocalAnyRef:
        *result = kWasmAnyRef;
        return true;
      default:
        *result = kWasmVar;
        return false;
    }
  }

  uint32_t in_arity() const {
    if (type != kWasmVar) return 0;
    return static_cast<uint32_t>(sig->parameter_count());
  }
  uint32_t out_arity() const {
    if (type == kWasmStmt) return 0;
    if (type != kWasmVar) return 1;
    return static_cast<uint32_t>(sig->return_count());
  }
  ValueType in_type(uint32_t index) {
    DCHECK_EQ(kWasmVar, type);
    return sig->GetParam(index);
  }
  ValueType out_type(uint32_t index) {
    if (type == kWasmVar) return sig->GetReturn(index);
    DCHECK_NE(kWasmStmt, type);
    DCHECK_EQ(0, index);
    return type;
  }
};

template <Decoder::ValidateFlag validate>
struct BranchDepthImmediate {
  uint32_t depth;
  uint32_t length;
  inline BranchDepthImmediate(Decoder* decoder, const byte* pc) {
    depth = decoder->read_u32v<validate>(pc + 1, &length, "branch depth");
  }
};

template <Decoder::ValidateFlag validate>
struct CallIndirectImmediate {
  uint32_t table_index;
  uint32_t sig_index;
  FunctionSig* sig = nullptr;
  uint32_t length = 0;
  inline CallIndirectImmediate(Decoder* decoder, const byte* pc) {
    uint32_t len = 0;
    sig_index = decoder->read_u32v<validate>(pc + 1, &len, "signature index");
    if (!VALIDATE(decoder->ok())) return;
    table_index = decoder->read_u8<validate>(pc + 1 + len, "table index");
    if (!VALIDATE(table_index == 0)) {
      decoder->errorf(pc + 1 + len, "expected table index 0, found %u",
                      table_index);
    }
    length = 1 + len;
  }
};

template <Decoder::ValidateFlag validate>
struct CallNativeImmediate {
  uint32_t sig_index;
  FunctionSig* sig = nullptr;
  uint32_t length;
  inline CallNativeImmediate(Decoder* decoder, const byte* pc) {
    sig_index = decoder->read_u32v<validate>(pc + 1, &length, "signature index");
  }
};


template <Decoder::ValidateFlag validate>
struct CallFunctionImmediate {
  uint32_t index;
  FunctionSig* sig = nullptr;
  uint32_t length;
  inline CallFunctionImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "function index");
  }
};

template <Decoder::ValidateFlag validate>
struct CallNativeFunctionImmediate {
  uint32_t index;
  const WasmNative* native = nullptr;
  uint32_t length;
  inline CallNativeFunctionImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u32v<validate>(pc + 1, &length, "native function index");
  }
};

template <Decoder::ValidateFlag validate>
struct MemoryIndexImmediate {
  uint32_t index;
  uint32_t length = 1;
  inline MemoryIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u8<validate>(pc + 1, "memory index");
    if (!VALIDATE(index == 0)) {
      decoder->errorf(pc + 1, "expected memory index 0, found %u", index);
    }
  }
};

template <Decoder::ValidateFlag validate>
struct TableIndexImmediate {
  uint32_t index;
  unsigned length = 1;
  inline TableIndexImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_u8<validate>(pc + 1, "table index");
    if (!VALIDATE(index == 0)) {
      decoder->errorf(pc + 1, "expected table index 0, found %u", index);
    }
  }
};

template <Decoder::ValidateFlag validate>
struct BranchTableImmediate {
  uint32_t table_count;
  const byte* start;
  const byte* table;
  inline BranchTableImmediate(Decoder* decoder, const byte* pc) {
    DCHECK_EQ(kExprBrTable, decoder->read_u8<validate>(pc, "opcode"));
    start = pc + 1;
    uint32_t len = 0;
    table_count = decoder->read_u32v<validate>(pc + 1, &len, "table count");
    table = pc + 1 + len;
  }
};

// A helper to iterate over a branch table.
template <Decoder::ValidateFlag validate>
class BranchTableIterator {
 public:
  uint32_t cur_index() { return index_; }
  bool has_next() { return VALIDATE(decoder_->ok()) && index_ <= table_count_; }
  uint32_t next() {
    DCHECK(has_next());
    index_++;
    uint32_t length;
    uint32_t result =
        decoder_->read_u32v<validate>(pc_, &length, "branch table entry");
    pc_ += length;
    return result;
  }
  // length, including the length of the {BranchTableImmediate}, but not the
  // opcode.
  uint32_t length() {
    while (has_next()) next();
    return static_cast<uint32_t>(pc_ - start_);
  }
  const byte* pc() { return pc_; }

  BranchTableIterator(Decoder* decoder,
                      const BranchTableImmediate<validate>& imm)
      : decoder_(decoder),
        start_(imm.start),
        pc_(imm.table),
        table_count_(imm.table_count) {}

 private:
  Decoder* decoder_;
  const byte* start_;
  const byte* pc_;
  uint32_t index_ = 0;    // the current index.
  uint32_t table_count_;  // the count of entries, not including default.
};

template <Decoder::ValidateFlag validate>
struct MemoryAccessImmediate {
  uint32_t alignment;
  uint32_t offset;
  uint32_t length = 0;
  inline MemoryAccessImmediate(Decoder* decoder, const byte* pc,
                               uint32_t max_alignment) {
    uint32_t alignment_length;
    alignment =
        decoder->read_u32v<validate>(pc + 1, &alignment_length, "alignment");
    if (!VALIDATE(alignment <= max_alignment)) {
      decoder->errorf(pc + 1,
                      "invalid alignment; expected maximum alignment is %u, "
                      "actual alignment is %u",
                      max_alignment, alignment);
    }
    if (!VALIDATE(decoder->ok())) return;
    uint32_t offset_length;
    offset = decoder->read_u32v<validate>(pc + 1 + alignment_length,
                                          &offset_length, "offset");
    length = alignment_length + offset_length;
  }
};

// Immediate for SIMD lane operations.
template <Decoder::ValidateFlag validate>
struct SimdLaneImmediate {
  uint8_t lane;
  uint32_t length = 1;

  inline SimdLaneImmediate(Decoder* decoder, const byte* pc) {
    lane = decoder->read_u8<validate>(pc + 2, "lane");
  }
};

// Immediate for SIMD shift operations.
template <Decoder::ValidateFlag validate>
struct SimdShiftImmediate {
  uint8_t shift;
  uint32_t length = 1;

  inline SimdShiftImmediate(Decoder* decoder, const byte* pc) {
    shift = decoder->read_u8<validate>(pc + 2, "shift");
  }
};

// Immediate for SIMD S8x16 shuffle operations.
template <Decoder::ValidateFlag validate>
struct Simd8x16ShuffleImmediate {
  uint8_t shuffle[kSimd128Size] = {0};

  inline Simd8x16ShuffleImmediate(Decoder* decoder, const byte* pc) {
    for (uint32_t i = 0; i < kSimd128Size; ++i) {
      shuffle[i] = decoder->read_u8<validate>(pc + 2 + i, "shuffle");
      if (!VALIDATE(decoder->ok())) return;
    }
  }
};

template <Decoder::ValidateFlag validate>
struct MemoryInitImmediate {
  MemoryIndexImmediate<validate> memory;
  uint32_t data_segment_index = 0;
  unsigned length = 0;

  inline MemoryInitImmediate(Decoder* decoder, const byte* pc)
      : memory(decoder, pc + 1) {
    if (!VALIDATE(decoder->ok())) return;
    uint32_t len = 0;
    data_segment_index = decoder->read_i32v<validate>(
        pc + 2 + memory.length, &len, "data segment index");
    length = memory.length + len;
  }
};

template <Decoder::ValidateFlag validate>
struct MemoryDropImmediate {
  uint32_t index;
  unsigned length;

  inline MemoryDropImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_i32v<validate>(pc + 2, &length, "data segment index");
  }
};

template <Decoder::ValidateFlag validate>
struct TableInitImmediate {
  TableIndexImmediate<validate> table;
  uint32_t elem_segment_index = 0;
  unsigned length = 0;

  inline TableInitImmediate(Decoder* decoder, const byte* pc)
      : table(decoder, pc + 1) {
    if (!VALIDATE(decoder->ok())) return;
    uint32_t len = 0;
    elem_segment_index = decoder->read_i32v<validate>(
        pc + 2 + table.length, &len, "elem segment index");
    length = table.length + len;
  }
};

template <Decoder::ValidateFlag validate>
struct TableDropImmediate {
  uint32_t index;
  unsigned length;

  inline TableDropImmediate(Decoder* decoder, const byte* pc) {
    index = decoder->read_i32v<validate>(pc + 2, &length, "elem segment index");
  }
};

// An entry on the value stack.
struct ValueBase {
  const byte* pc = nullptr;
  ValueType type = kWasmStmt;

  ValueBase(const byte* pc, ValueType type) : pc(pc), type(type) {}
};

template <typename Value>
struct Merge {
  uint32_t arity = 0;
  union {  // Either multiple values or a single value.
    Value* array;
    Value first;
  } vals = {nullptr};  // Initialize {array} with {nullptr}.

  // Tracks whether this merge was ever reached. Uses precise reachability, like
  // Reachability::kReachable.
  bool reached;

  Merge(bool reached = false) : reached(reached) {}

  Value& operator[](uint32_t i) {
    DCHECK_GT(arity, i);
    return arity == 1 ? vals.first : vals.array[i];
  }
};

enum ControlKind : uint8_t {
  kControlIf,
  kControlIfElse,
  kControlBlock,
  kControlLoop,
  kControlTry,
  kControlTryCatch
};

enum Reachability : uint8_t {
  // reachable code.
  kReachable,
  // reachable code in unreachable block (implies normal validation).
  kSpecOnlyReachable,
  // code unreachable in its own block (implies polymorphic validation).
  kUnreachable
};

// An entry on the control stack (i.e. if, block, loop, or try).
template <typename Value>
struct ControlBase {
  ControlKind kind = kControlBlock;
  uint32_t stack_depth = 0;  // stack height at the beginning of the construct.
  const uint8_t* pc = nullptr;
  Reachability reachability = kReachable;

  // Values merged into the start or end of this control construct.
  Merge<Value> start_merge;
  Merge<Value> end_merge;

  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(ControlBase);

  ControlBase(ControlKind kind, uint32_t stack_depth, const uint8_t* pc,
              Reachability reachability)
      : kind(kind),
        stack_depth(stack_depth),
        pc(pc),
        reachability(reachability),
        start_merge(reachability == kReachable) {}

  // Check whether the current block is reachable.
  bool reachable() const { return reachability == kReachable; }

  // Check whether the rest of the block is unreachable.
  // Note that this is different from {!reachable()}, as there is also the
  // "indirect unreachable state", for which both {reachable()} and
  // {unreachable()} return false.
  bool unreachable() const { return reachability == kUnreachable; }

  // Return the reachability of new control structs started in this block.
  Reachability innerReachability() const {
    return reachability == kReachable ? kReachable : kSpecOnlyReachable;
  }

  bool is_if() const { return is_onearmed_if() || is_if_else(); }
  bool is_onearmed_if() const { return kind == kControlIf; }
  bool is_if_else() const { return kind == kControlIfElse; }
  bool is_block() const { return kind == kControlBlock; }
  bool is_loop() const { return kind == kControlLoop; }
  bool is_incomplete_try() const { return kind == kControlTry; }
  bool is_try_catch() const { return kind == kControlTryCatch; }
  bool is_try() const { return is_incomplete_try() || is_try_catch(); }

  inline Merge<Value>* br_merge() {
    return is_loop() ? &this->start_merge : &this->end_merge;
  }
};

// This is the list of callback functions that an interface for the
// WasmFullDecoder should implement.
// F(Name, args...)
#define INTERFACE_FUNCTIONS(F)                                                \
  /* General: */                                                              \
  F(StartFunction)                                                            \
  F(StartFunctionBody, Control* block)                                        \
  F(FinishFunction)                                                           \
  F(OnFirstError)                                                             \
  F(NextInstruction, WasmOpcode)                                              \
  /* Control: */                                                              \
  F(Block, Control* block)                                                    \
  F(Loop, Control* block)                                                     \
  F(Try, Control* block)                                                      \
  F(Catch, Control* block, Value* exception)                                  \
  F(If, const Value& cond, Control* if_block)                                 \
  F(FallThruTo, Control* c)                                                   \
  F(PopControl, Control* block)                                               \
  F(EndControl, Control* block)                                               \
  /* Instructions: */                                                         \
  F(UnOp, WasmOpcode opcode, const Value& value, Value* result)               \
  F(BinOp, WasmOpcode opcode, const Value& lhs, const Value& rhs,             \
    Value* result)                                                            \
  F(I32Const, Value* result, int32_t value)                                   \
  F(I64Const, Value* result, int64_t value)                                   \
  F(F32Const, Value* result, float value)                                     \
  F(F64Const, Value* result, double value)                                    \
  F(RefNull, Value* result)                                                   \
  F(Drop, const Value& value)                                                 \
  F(DoReturn, Vector<Value> values)                                           \
  F(GetLocal, Value* result, const LocalIndexImmediate<validate>& imm)        \
  F(SetLocal, const Value& value, const LocalIndexImmediate<validate>& imm)   \
  F(TeeLocal, const Value& value, Value* result,                              \
    const LocalIndexImmediate<validate>& imm)                                 \
  F(GetGlobal, Value* result, const GlobalIndexImmediate<validate>& imm)      \
  F(SetGlobal, const Value& value, const GlobalIndexImmediate<validate>& imm) \
  F(Unreachable)                                                              \
  F(Select, const Value& cond, const Value& fval, const Value& tval,          \
    Value* result)                                                            \
  F(Br, Control* target)                                                      \
  F(BrIf, const Value& cond, uint32_t depth)                                  \
  F(BrTable, const BranchTableImmediate<validate>& imm, const Value& key)     \
  F(Else, Control* if_block)                                                  \
  F(LoadMem, LoadType type, const MemoryAccessImmediate<validate>& imm,       \
    const Value& index, Value* result)                                        \
  F(StoreMem, StoreType type, const MemoryAccessImmediate<validate>& imm,     \
    const Value& index, const Value& value)                                   \
  F(CurrentMemoryPages, Value* result)                                        \
  F(MemoryGrow, const Value& value, Value* result)                            \
  F(CallDirect, const CallFunctionImmediate<validate>& imm,                   \
    const Value args[], Value returns[])                                      \
  F(CallNative, const CallNativeFunctionImmediate<validate> &imm,             \
    const Value args[], Value returns[])                                      \
  F(CallIndirect, const Value& index,                                         \
    const CallIndirectImmediate<validate>& imm, const Value args[],           \
    Value returns[])                                                          \
  F(SimdOp, WasmOpcode opcode, Vector<Value> args, Value* result)             \
  F(SimdLaneOp, WasmOpcode opcode, const SimdLaneImmediate<validate>& imm,    \
    const Vector<Value> inputs, Value* result)                                \
  F(SimdShiftOp, WasmOpcode opcode, const SimdShiftImmediate<validate>& imm,  \
    const Value& input, Value* result)                                        \
  F(Simd8x16ShuffleOp, const Simd8x16ShuffleImmediate<validate>& imm,         \
    const Value& input0, const Value& input1, Value* result)                  \
  F(Throw, const ExceptionIndexImmediate<validate>& imm,                      \
    const Vector<Value>& args)                                                \
  F(Rethrow, const Value& exception)                                          \
  F(BrOnException, const Value& exception,                                    \
    const ExceptionIndexImmediate<validate>& imm, uint32_t depth,             \
    Vector<Value> values)                                                     \
  F(AtomicOp, WasmOpcode opcode, Vector<Value> args,                          \
    const MemoryAccessImmediate<validate>& imm, Value* result)                \
  F(MemoryInit, const MemoryInitImmediate<validate>& imm, const Value& dst,   \
    const Value& src, const Value& size)                                      \
  F(MemoryDrop, const MemoryDropImmediate<validate>& imm)                     \
  F(MemoryCopy, const MemoryIndexImmediate<validate>& imm, const Value& dst,  \
    const Value& src, const Value& size)                                      \
  F(MemoryFill, const MemoryIndexImmediate<validate>& imm, const Value& dst,  \
    const Value& value, const Value& size)                                    \
  F(TableInit, const TableInitImmediate<validate>& imm, Vector<Value> args)   \
  F(TableDrop, const TableDropImmediate<validate>& imm)                       \
  F(TableCopy, const TableIndexImmediate<validate>& imm, Vector<Value> args)  \
  F(Offset32, const Value& base, const Value& index, const Value& scale,      \
    Value* result)

// Generic Wasm bytecode decoder with utilities for decoding immediates,
// lengths, etc.
template <Decoder::ValidateFlag validate>
class WasmDecoder : public Decoder {
 public:
  WasmDecoder(const WasmModule* module, const WasmFeatures& enabled,
              WasmFeatures* detected, FunctionSig* sig, const byte* start,
              const byte* end, uint32_t buffer_offset = 0)
      : Decoder(start, end, buffer_offset),
        module_(module),
        enabled_(enabled),
        detected_(detected),
        sig_(sig),
        local_types_(nullptr) {}
  const WasmModule* module_;
  const WasmFeatures enabled_;
  WasmFeatures* detected_;
  FunctionSig* sig_;

  ZoneVector<ValueType>* local_types_;

  uint32_t total_locals() const {
    return local_types_ == nullptr
               ? 0
               : static_cast<uint32_t>(local_types_->size());
  }

  static bool DecodeLocals(const WasmFeatures& enabled, Decoder* decoder,
                           const FunctionSig* sig,
                           ZoneVector<ValueType>* type_list) {
    DCHECK_NOT_NULL(type_list);
    DCHECK_EQ(0, type_list->size());
    // Initialize from signature.
    if (sig != nullptr) {
      type_list->assign(sig->parameters().begin(), sig->parameters().end());
    }
    // Decode local declarations, if any.
    uint32_t entries = decoder->consume_u32v("local decls count");
    if (decoder->failed()) return false;

    TRACE("local decls count: %u\n", entries);
    while (entries-- > 0 && VALIDATE(decoder->ok()) && decoder->more()) {
      uint32_t count = decoder->consume_u32v("local count");
      if (decoder->failed()) return false;

      DCHECK_LE(type_list->size(), kV8MaxWasmFunctionLocals);
      if (count > kV8MaxWasmFunctionLocals - type_list->size()) {
        decoder->error(decoder->pc() - 1, "local count too large");
        return false;
      }
      byte code = decoder->consume_u8("local type");
      if (decoder->failed()) return false;

      ValueType type;
      switch (code) {
        case kLocalI32:
          type = kWasmI32;
          break;
        case kLocalI64:
          type = kWasmI64;
          break;
        case kLocalF32:
          type = kWasmF32;
          break;
        case kLocalF64:
          type = kWasmF64;
          break;
        case kLocalAnyRef:
          if (enabled.anyref) {
            type = kWasmAnyRef;
            break;
          }
          decoder->error(decoder->pc() - 1, "invalid local type");
          return false;
        case kLocalExceptRef:
          if (enabled.eh) {
            type = kWasmExceptRef;
            break;
          }
          decoder->error(decoder->pc() - 1, "invalid local type");
          return false;
        case kLocalS128:
          if (enabled.simd) {
            type = kWasmS128;
            break;
          }
          V8_FALLTHROUGH;
        default:
          decoder->error(decoder->pc() - 1, "invalid local type");
          return false;
      }
      type_list->insert(type_list->end(), count, type);
    }
    DCHECK(decoder->ok());
    return true;
  }

  static BitVector* AnalyzeLoopAssignment(Decoder* decoder, const byte* pc,
                                          uint32_t locals_count, Zone* zone) {
    if (pc >= decoder->end()) return nullptr;
    if (*pc != kExprLoop) return nullptr;

    // The number of locals_count is augmented by 2 so that 'locals_count - 2'
    // can be used to track mem_size, and 'locals_count - 1' to track mem_start.
    BitVector* assigned = new (zone) BitVector(locals_count, zone);
    int depth = 0;
    // Iteratively process all AST nodes nested inside the loop.
    while (pc < decoder->end() && VALIDATE(decoder->ok())) {
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
      uint32_t length = 1;
      switch (opcode) {
        case kExprLoop:
        case kExprIf:
        case kExprBlock:
        case kExprTry:
          length = OpcodeLength(decoder, pc);
          depth++;
          break;
        case kExprSetLocal:  // fallthru
        case kExprTeeLocal: {
          LocalIndexImmediate<validate> imm(decoder, pc);
          if (assigned->length() > 0 &&
              imm.index < static_cast<uint32_t>(assigned->length())) {
            // Unverified code might have an out-of-bounds index.
            assigned->Add(imm.index);
          }
          length = 1 + imm.length;
          break;
        }
        case kExprMemoryGrow:
        case kExprCallFunction:
        case kExprCallIndirect:
          // Add instance cache nodes to the assigned set.
          // TODO(titzer): make this more clear.
          assigned->Add(locals_count - 1);
          length = OpcodeLength(decoder, pc);
          break;
        case kExprEnd:
          depth--;
          break;
        default:
          length = OpcodeLength(decoder, pc);
          break;
      }
      if (depth <= 0) break;
      pc += length;
    }
    return VALIDATE(decoder->ok()) ? assigned : nullptr;
  }

  inline bool Validate(const byte* pc, LocalIndexImmediate<validate>& imm) {
    if (!VALIDATE(imm.index < total_locals())) {
      errorf(pc + 1, "invalid local index: %u", imm.index);
      return false;
    }
    imm.type = local_types_ ? local_types_->at(imm.index) : kWasmStmt;
    return true;
  }

  inline bool Complete(const byte* pc, ExceptionIndexImmediate<validate>& imm) {
    if (!VALIDATE(module_ != nullptr &&
                  imm.index < module_->exceptions.size())) {
      return false;
    }
    imm.exception = &module_->exceptions[imm.index];
    return true;
  }

  inline bool Validate(const byte* pc, ExceptionIndexImmediate<validate>& imm) {
    if (!Complete(pc, imm)) {
      errorf(pc + 1, "Invalid exception index: %u", imm.index);
      return false;
    }
    return true;
  }

  inline bool Validate(const byte* pc, GlobalIndexImmediate<validate>& imm) {
    if (!VALIDATE(module_ != nullptr && imm.index < module_->globals.size())) {
      errorf(pc + 1, "invalid global index: %u", imm.index);
      return false;
    }
    imm.global = &module_->globals[imm.index];
    imm.type = imm.global->type;
    return true;
  }

  inline bool Complete(const byte* pc, CallFunctionImmediate<validate>& imm) {
    if (!VALIDATE(module_ != nullptr &&
                  imm.index < module_->functions.size())) {
      return false;
    }
    imm.sig = module_->functions[imm.index].sig;
    return true;
  }

  inline bool Validate(const byte* pc, CallFunctionImmediate<validate>& imm) {
    if (Complete(pc, imm)) {
      return true;
    }
    errorf(pc + 1, "invalid function index: %u", imm.index);
    return false;
  }

  inline bool Complete(const byte* pc, CallIndirectImmediate<validate>& imm) {
    if (!VALIDATE(module_ != nullptr &&
                  imm.sig_index < module_->signatures.size())) {
      return false;
    }
    imm.sig = module_->signatures[imm.sig_index];
    return true;
  }

  inline bool Complete(const byte* pc, CallNativeFunctionImmediate<validate>& imm) {
    if (!VALIDATE(module_ != nullptr &&
                  imm.index < module_->natives.size())) {
      return false;
    }
    imm.native = &module_->natives[imm.index];
    return true;
  }

  inline bool Validate(const byte* pc, CallIndirectImmediate<validate>& imm) {
    if (!VALIDATE(module_ != nullptr && !module_->tables.empty())) {
      error("function table has to exist to execute call_indirect");
      return false;
    }
    if (!Complete(pc, imm)) {
      errorf(pc + 1, "invalid signature index: #%u", imm.sig_index);
      return false;
    }
    return true;
  }

  inline bool Validate(const byte* pc, CallNativeFunctionImmediate<validate>& imm) {
    if (!Complete(pc, imm)) {
      errorf(pc + 1, "invalid native index: #%u", imm.index);
      return false;
    }
    return true;
  }

  inline bool Validate(const byte* pc, BranchDepthImmediate<validate>& imm,
                       size_t control_depth) {
    if (!VALIDATE(imm.depth < control_depth)) {
      errorf(pc + 1, "invalid branch depth: %u", imm.depth);
      return false;
    }
    return true;
  }

  bool Validate(const byte* pc, BranchTableImmediate<validate>& imm,
                size_t block_depth) {
    if (!VALIDATE(imm.table_count < kV8MaxWasmFunctionSize)) {
      errorf(pc + 1, "invalid table count (> max function size): %u",
             imm.table_count);
      return false;
    }
    return checkAvailable(imm.table_count);
  }

  inline bool Validate(const byte* pc, WasmOpcode opcode,
                       SimdLaneImmediate<validate>& imm) {
    uint8_t num_lanes = 0;
    switch (opcode) {
      case kExprF32x4ExtractLane:
      case kExprF32x4ReplaceLane:
      case kExprI32x4ExtractLane:
      case kExprI32x4ReplaceLane:
        num_lanes = 4;
        break;
      case kExprI16x8ExtractLane:
      case kExprI16x8ReplaceLane:
        num_lanes = 8;
        break;
      case kExprI8x16ExtractLane:
      case kExprI8x16ReplaceLane:
        num_lanes = 16;
        break;
      default:
        UNREACHABLE();
        break;
    }
    if (!VALIDATE(imm.lane >= 0 && imm.lane < num_lanes)) {
      error(pc_ + 2, "invalid lane index");
      return false;
    } else {
      return true;
    }
  }

  inline bool Validate(const byte* pc, WasmOpcode opcode,
                       SimdShiftImmediate<validate>& imm) {
    uint8_t max_shift = 0;
    switch (opcode) {
      case kExprI32x4Shl:
      case kExprI32x4ShrS:
      case kExprI32x4ShrU:
        max_shift = 32;
        break;
      case kExprI16x8Shl:
      case kExprI16x8ShrS:
      case kExprI16x8ShrU:
        max_shift = 16;
        break;
      case kExprI8x16Shl:
      case kExprI8x16ShrS:
      case kExprI8x16ShrU:
        max_shift = 8;
        break;
      default:
        UNREACHABLE();
        break;
    }
    if (!VALIDATE(imm.shift >= 0 && imm.shift < max_shift)) {
      error(pc_ + 2, "invalid shift amount");
      return false;
    } else {
      return true;
    }
  }

  inline bool Validate(const byte* pc,
                       Simd8x16ShuffleImmediate<validate>& imm) {
    uint8_t max_lane = 0;
    for (uint32_t i = 0; i < kSimd128Size; ++i) {
      max_lane = std::max(max_lane, imm.shuffle[i]);
    }
    // Shuffle indices must be in [0..31] for a 16 lane shuffle.
    if (!VALIDATE(max_lane <= 2 * kSimd128Size)) {
      error(pc_ + 2, "invalid shuffle mask");
      return false;
    }
    return true;
  }

  inline bool Complete(BlockTypeImmediate<validate>& imm) {
    if (imm.type != kWasmVar) return true;
    if (!VALIDATE((module_ && imm.sig_index < module_->signatures.size()))) {
      return false;
    }
    imm.sig = module_->signatures[imm.sig_index];
    return true;
  }

  inline bool Validate(BlockTypeImmediate<validate>& imm) {
    if (!Complete(imm)) {
      errorf(pc_, "block type index %u out of bounds (%zu signatures)",
             imm.sig_index, module_ ? module_->signatures.size() : 0);
      return false;
    }
    return true;
  }

  inline bool Validate(MemoryIndexImmediate<validate>& imm) {
    if (!VALIDATE(module_ != nullptr && module_->has_memory)) {
      errorf(pc_ + 1, "memory instruction with no memory");
      return false;
    }
    return true;
  }

  inline bool Validate(MemoryInitImmediate<validate>& imm) {
    if (!Validate(imm.memory)) return false;
    if (!VALIDATE(module_ != nullptr &&
                  imm.data_segment_index <
                      module_->num_declared_data_segments)) {
      errorf(pc_ + 2, "invalid data segment index: %u", imm.data_segment_index);
      return false;
    }
    return true;
  }

  inline bool Validate(MemoryDropImmediate<validate>& imm) {
    if (!VALIDATE(module_ != nullptr &&
                  imm.index < module_->num_declared_data_segments)) {
      errorf(pc_ + 2, "invalid data segment index: %u", imm.index);
      return false;
    }
    return true;
  }

  inline bool Validate(const byte* pc, TableIndexImmediate<validate>& imm) {
    if (!VALIDATE(module_ != nullptr && imm.index < module_->tables.size())) {
      errorf(pc_ + 1, "invalid table index: %u", imm.index);
      return false;
    }
    return true;
  }

  inline bool Validate(TableInitImmediate<validate>& imm) {
    if (!Validate(pc_ + 1, imm.table)) return false;
    if (!VALIDATE(module_ != nullptr &&
                  imm.elem_segment_index < module_->elem_segments.size())) {
      errorf(pc_ + 2, "invalid element segment index: %u",
             imm.elem_segment_index);
      return false;
    }
    return true;
  }

  inline bool Validate(TableDropImmediate<validate>& imm) {
    if (!VALIDATE(module_ != nullptr &&
                  imm.index < module_->elem_segments.size())) {
      errorf(pc_ + 2, "invalid element segment index: %u", imm.index);
      return false;
    }
    return true;
  }

  static uint32_t OpcodeLength(Decoder* decoder, const byte* pc) {
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    switch (opcode) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
      {
        MemoryAccessImmediate<validate> imm(decoder, pc, UINT32_MAX);
        return 1 + imm.length;
      }
      case kExprBr:
      case kExprBrIf: {
        BranchDepthImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprSetGlobal:
      case kExprGetGlobal: {
        GlobalIndexImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }

      case kExprCallFunction: {
        CallFunctionImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprCallNativeFunction: {
        CallNativeFunctionImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprCallIndirect: {
        CallIndirectImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }

      case kExprTry:
      case kExprIf:  // fall through
      case kExprLoop:
      case kExprBlock: {
        BlockTypeImmediate<validate> imm(kAllWasmFeatures, decoder, pc);
        return 1 + imm.length;
      }

      case kExprThrow: {
        ExceptionIndexImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }

      case kExprBrOnExn: {
        BranchDepthImmediate<validate> imm_br(decoder, pc);
        if (!VALIDATE(decoder->ok())) return 1 + imm_br.length;
        ExceptionIndexImmediate<validate> imm_idx(decoder, pc + imm_br.length);
        return 1 + imm_br.length + imm_idx.length;
      }

      case kExprSetLocal:
      case kExprTeeLocal:
      case kExprGetLocal: {
        LocalIndexImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprBrTable: {
        BranchTableImmediate<validate> imm(decoder, pc);
        BranchTableIterator<validate> iterator(decoder, imm);
        return 1 + iterator.length();
      }
      case kExprI32Const: {
        ImmI32Immediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprI64Const: {
        ImmI64Immediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprRefNull: {
        return 1;
      }
      case kExprMemoryGrow:
      case kExprMemorySize: {
        MemoryIndexImmediate<validate> imm(decoder, pc);
        return 1 + imm.length;
      }
      case kExprF32Const:
        return 5;
      case kExprF64Const:
        return 9;
      case kNumericPrefix: {
        byte numeric_index =
            decoder->read_u8<validate>(pc + 1, "numeric_index");
        if (!VALIDATE(decoder->ok())) return 2;
        WasmOpcode opcode =
            static_cast<WasmOpcode>(kNumericPrefix << 8 | numeric_index);
        switch (opcode) {
          case kExprI32SConvertSatF32:
          case kExprI32UConvertSatF32:
          case kExprI32SConvertSatF64:
          case kExprI32UConvertSatF64:
          case kExprI64SConvertSatF32:
          case kExprI64UConvertSatF32:
          case kExprI64SConvertSatF64:
          case kExprI64UConvertSatF64:
            return 2;
          case kExprMemoryInit: {
            MemoryInitImmediate<validate> imm(decoder, pc);
            return 2 + imm.length;
          }
          case kExprMemoryDrop: {
            MemoryDropImmediate<validate> imm(decoder, pc);
            return 2 + imm.length;
          }
          case kExprMemoryCopy:
          case kExprMemoryFill: {
            MemoryIndexImmediate<validate> imm(decoder, pc + 1);
            return 2 + imm.length;
          }
          case kExprTableInit: {
            TableInitImmediate<validate> imm(decoder, pc);
            return 2 + imm.length;
          }
          case kExprTableDrop: {
            TableDropImmediate<validate> imm(decoder, pc);
            return 2 + imm.length;
          }
          case kExprTableCopy: {
            TableIndexImmediate<validate> imm(decoder, pc + 1);
            return 2 + imm.length;
          }
          default:
            decoder->error(pc, "invalid numeric opcode");
            return 2;
        }
      }
      case kSimdPrefix: {
        byte simd_index = decoder->read_u8<validate>(pc + 1, "simd_index");
        if (!VALIDATE(decoder->ok())) return 2;
        WasmOpcode opcode =
            static_cast<WasmOpcode>(kSimdPrefix << 8 | simd_index);
        switch (opcode) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_SIMD_0_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          return 2;
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_SIMD_1_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          return 3;
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_SIMD_MEM_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          {
            MemoryAccessImmediate<validate> imm(decoder, pc + 1, UINT32_MAX);
            return 2 + imm.length;
          }
          // Shuffles require a byte per lane, or 16 immediate bytes.
          case kExprS8x16Shuffle:
            return 2 + kSimd128Size;
          default:
            decoder->error(pc, "invalid SIMD opcode");
            return 2;
        }
      }
      case kAtomicPrefix: {
        byte atomic_index = decoder->read_u8<validate>(pc + 1, "atomic_index");
        if (!VALIDATE(decoder->ok())) return 2;
        WasmOpcode opcode =
            static_cast<WasmOpcode>(kAtomicPrefix << 8 | atomic_index);
        switch (opcode) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_ATOMIC_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          {
            MemoryAccessImmediate<validate> imm(decoder, pc + 1, UINT32_MAX);
            return 2 + imm.length;
          }
          default:
            decoder->error(pc, "invalid Atomics opcode");
            return 2;
        }
      }
      default:
        return 1;
    }
  }

  std::pair<uint32_t, uint32_t> StackEffect(const byte* pc) {
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    // Handle "simple" opcodes with a fixed signature first.
    FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (!sig) sig = WasmOpcodes::AsmjsSignature(opcode);
    if (sig) return {sig->parameter_count(), sig->return_count()};

#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
    // clang-format off
    switch (opcode) {
      case kExprOffset32:
      case kExprSelect:
        return {3, 1};
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
        return {2, 0};
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      case kExprTeeLocal:
      case kExprMemoryGrow:
        return {1, 1};
      case kExprSetLocal:
      case kExprSetGlobal:
      case kExprDrop:
      case kExprBrIf:
      case kExprBrTable:
      case kExprIf:
      case kExprRethrow:
        return {1, 0};
      case kExprCatch:
      case kExprGetLocal:
      case kExprGetGlobal:
      case kExprI32Const:
      case kExprI64Const:
      case kExprF32Const:
      case kExprF64Const:
      case kExprRefNull:
      case kExprMemorySize:
        return {0, 1};
      case kExprCallNativeFunction: {
        CallNativeFunctionImmediate<validate> imm(this, pc);
        CHECK(Complete(pc, imm));
        return {imm.native->sig->parameter_count(), imm.native->sig->return_count()};
      }
      case kExprCallFunction: {
        CallFunctionImmediate<validate> imm(this, pc);
        CHECK(Complete(pc, imm));
        return {imm.sig->parameter_count(), imm.sig->return_count()};
      }
      case kExprCallIndirect: {
        CallIndirectImmediate<validate> imm(this, pc);
        CHECK(Complete(pc, imm));
        // Indirect calls pop an additional argument for the table index.
        return {imm.sig->parameter_count() + 1,
                imm.sig->return_count()};
      }
      case kExprThrow: {
        ExceptionIndexImmediate<validate> imm(this, pc);
        CHECK(Complete(pc, imm));
        DCHECK_EQ(0, imm.exception->sig->return_count());
        return {imm.exception->sig->parameter_count(), 0};
      }
      case kExprBr:
      case kExprBlock:
      case kExprLoop:
      case kExprEnd:
      case kExprElse:
      case kExprTry:
      case kExprBrOnExn:
      case kExprNop:
      case kExprReturn:
      case kExprUnreachable:
        return {0, 0};
      case kNumericPrefix:
      case kAtomicPrefix:
      case kSimdPrefix: {
        opcode = static_cast<WasmOpcode>(opcode << 8 | *(pc + 1));
        switch (opcode) {
          FOREACH_SIMD_1_OPERAND_1_PARAM_OPCODE(DECLARE_OPCODE_CASE)
            return {1, 1};
          FOREACH_SIMD_1_OPERAND_2_PARAM_OPCODE(DECLARE_OPCODE_CASE)
          FOREACH_SIMD_MASK_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
            return {2, 1};
          default: {
            sig = WasmOpcodes::Signature(opcode);
            if (sig) {
              return {sig->parameter_count(), sig->return_count()};
            }
          }
        }
        V8_FALLTHROUGH;
      }
      default:
        V8_Fatal(__FILE__, __LINE__, "unimplemented opcode: %x (%s)", opcode,
                 WasmOpcodes::OpcodeName(opcode));
        return {0, 0};
    }
#undef DECLARE_OPCODE_CASE
    // clang-format on
  }
};

#define CALL_INTERFACE(name, ...) interface_.name(this, ##__VA_ARGS__)
#define CALL_INTERFACE_IF_REACHABLE(name, ...)                 \
  do {                                                         \
    DCHECK(!control_.empty());                                 \
    if (VALIDATE(this->ok()) && control_.back().reachable()) { \
      interface_.name(this, ##__VA_ARGS__);                    \
    }                                                          \
  } while (false)
#define CALL_INTERFACE_IF_PARENT_REACHABLE(name, ...)           \
  do {                                                          \
    DCHECK(!control_.empty());                                  \
    if (VALIDATE(this->ok()) &&                                 \
        (control_.size() == 1 || control_at(1)->reachable())) { \
      interface_.name(this, ##__VA_ARGS__);                     \
    }                                                           \
  } while (false)

template <Decoder::ValidateFlag validate, typename Interface>
class WasmFullDecoder : public WasmDecoder<validate> {
  using Value = typename Interface::Value;
  using Control = typename Interface::Control;
  using MergeValues = Merge<Value>;

  // All Value types should be trivially copyable for performance. We push, pop,
  // and store them in local variables.
  ASSERT_TRIVIALLY_COPYABLE(Value);

 public:
  template <typename... InterfaceArgs>
  WasmFullDecoder(Zone* zone, const WasmModule* module,
                  const WasmFeatures& enabled, WasmFeatures* detected,
                  const FunctionBody& body, InterfaceArgs&&... interface_args)
      : WasmDecoder<validate>(module, enabled, detected, body.sig, body.start,
                              body.end, body.offset),
        zone_(zone),
        interface_(std::forward<InterfaceArgs>(interface_args)...),
        local_type_vec_(zone),
        stack_(zone),
        control_(zone),
        args_(zone) {
    this->local_types_ = &local_type_vec_;
  }

  Interface& interface() { return interface_; }

  bool Decode() {
    DCHECK(stack_.empty());
    DCHECK(control_.empty());

    base::ElapsedTimer decode_timer;
    if (FLAG_trace_wasm_decode_time) {
      decode_timer.Start();
    }

    if (this->end_ < this->pc_) {
      this->error("function body end < start");
      return false;
    }

    DCHECK_EQ(0, this->local_types_->size());
    WasmDecoder<validate>::DecodeLocals(this->enabled_, this, this->sig_,
                                        this->local_types_);
    CALL_INTERFACE(StartFunction);
    DecodeFunctionBody();
    if (!this->failed()) CALL_INTERFACE(FinishFunction);

    // Generate a better error message whether the unterminated control
    // structure is the function body block or an innner structure.
    if (control_.size() > 1) {
      this->error(control_.back().pc, "unterminated control structure");
    } else if (control_.size() == 1) {
      this->error("function body must end with \"end\" opcode");
    }

    if (this->failed()) return this->TraceFailed();

    if (FLAG_trace_wasm_decode_time) {
      double ms = decode_timer.Elapsed().InMillisecondsF();
      PrintF("wasm-decode %s (%0.3f ms)\n\n",
             VALIDATE(this->ok()) ? "ok" : "failed", ms);
    } else {
      TRACE("wasm-decode %s\n\n", VALIDATE(this->ok()) ? "ok" : "failed");
    }

    return true;
  }

  bool TraceFailed() {
    TRACE("wasm-error module+%-6d func+%d: %s\n\n", this->error_.offset(),
          this->GetBufferRelativeOffset(this->error_.offset()),
          this->error_.message().c_str());
    return false;
  }

  const char* SafeOpcodeNameAt(const byte* pc) {
    if (pc >= this->end_) return "<end>";
    return WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(*pc));
  }

  inline Zone* zone() const { return zone_; }

  inline uint32_t num_locals() const {
    return static_cast<uint32_t>(local_type_vec_.size());
  }

  inline ValueType GetLocalType(uint32_t index) {
    return local_type_vec_[index];
  }

  inline WasmCodePosition position() {
    int offset = static_cast<int>(this->pc_ - this->start_);
    DCHECK_EQ(this->pc_ - this->start_, offset);  // overflows cannot happen
    return offset;
  }

  inline uint32_t control_depth() const {
    return static_cast<uint32_t>(control_.size());
  }

  inline Control* control_at(uint32_t depth) {
    DCHECK_GT(control_.size(), depth);
    return &control_.back() - depth;
  }

  inline uint32_t stack_size() const {
    DCHECK_GE(kMaxUInt32, stack_.size());
    return static_cast<uint32_t>(stack_.size());
  }

  inline Value* stack_value(uint32_t depth) {
    DCHECK_LT(0, depth);
    DCHECK_GE(stack_.size(), depth);
    return &*(stack_.end() - depth);
  }

 private:
  Zone* zone_;

  Interface interface_;

  ZoneVector<ValueType> local_type_vec_;  // types of local variables.
  ZoneVector<Value> stack_;               // stack of values.
  ZoneVector<Control> control_;           // stack of blocks, loops, and ifs.
  ZoneVector<Value> args_;                // parameters of current block or call

  static Value UnreachableValue(const uint8_t* pc) {
    return Value{pc, kWasmVar};
  }

  bool CheckHasMemory() {
    if (!VALIDATE(this->module_->has_memory)) {
      this->error(this->pc_ - 1, "memory instruction with no memory");
      return false;
    }
    return true;
  }

  bool CheckHasSharedMemory() {
    if (!VALIDATE(this->module_->has_shared_memory)) {
      this->error(this->pc_ - 1, "Atomic opcodes used without shared memory");
      return false;
    }
    return true;
  }

  class TraceLine {
   public:
    static constexpr int kMaxLen = 512;
    ~TraceLine() {
      if (!FLAG_trace_wasm_decoder) return;
      PrintF("%.*s\n", len_, buffer_);
    }

    // Appends a formatted string.
    PRINTF_FORMAT(2, 3)
    void Append(const char* format, ...) {
      if (!FLAG_trace_wasm_decoder) return;
      va_list va_args;
      va_start(va_args, format);
      size_t remaining_len = kMaxLen - len_;
      Vector<char> remaining_msg_space(buffer_ + len_, remaining_len);
      int len = VSNPrintF(remaining_msg_space, format, va_args);
      va_end(va_args);
      len_ += len < 0 ? remaining_len : len;
    }

   private:
    char buffer_[kMaxLen];
    int len_ = 0;
  };

  // Decodes the body of a function.
  void DecodeFunctionBody() {
    TRACE("wasm-decode %p...%p (module+%u, %d bytes)\n",
          reinterpret_cast<const void*>(this->start()),
          reinterpret_cast<const void*>(this->end()), this->pc_offset(),
          static_cast<int>(this->end() - this->start()));

    // Set up initial function block.
    {
      auto* c = PushControl(kControlBlock);
      InitMerge(&c->start_merge, 0, [](uint32_t) -> Value { UNREACHABLE(); });
      InitMerge(&c->end_merge,
                static_cast<uint32_t>(this->sig_->return_count()),
                [&](uint32_t i) {
                  return Value{this->pc_, this->sig_->GetReturn(i)};
                });
      CALL_INTERFACE(StartFunctionBody, c);
    }

    while (this->pc_ < this->end_) {  // decoding loop.
      uint32_t len = 1;
      WasmOpcode opcode = static_cast<WasmOpcode>(*this->pc_);

      CALL_INTERFACE_IF_REACHABLE(NextInstruction, opcode);

#if DEBUG
      TraceLine trace_msg;
#define TRACE_PART(...) trace_msg.Append(__VA_ARGS__)
      if (!WasmOpcodes::IsPrefixOpcode(opcode)) {
        TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                   WasmOpcodes::OpcodeName(opcode));
      }
#else
#define TRACE_PART(...)
#endif

      switch (opcode) {
#define BUILD_SIMPLE_OPCODE(op, _, sig) \
  case kExpr##op:                       \
    BuildSimpleOperator_##sig(opcode);  \
    break;
        FOREACH_SIMPLE_OPCODE(BUILD_SIMPLE_OPCODE)
#undef BUILD_SIMPLE_OPCODE
        case kExprNop:
          break;
        case kExprBlock: {
          BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (!this->Validate(imm)) break;
          PopArgs(imm.sig);
          auto* block = PushControl(kControlBlock);
          SetBlockType(block, imm);
          CALL_INTERFACE_IF_REACHABLE(Block, block);
          PushMergeValues(block, &block->start_merge);
          len = 1 + imm.length;
          break;
        }
        case kExprRethrow: {
          CHECK_PROTOTYPE_OPCODE(eh);
          auto exception = Pop(0, kWasmExceptRef);
          CALL_INTERFACE_IF_REACHABLE(Rethrow, exception);
          EndControl();
          break;
        }
        case kExprThrow: {
          CHECK_PROTOTYPE_OPCODE(eh);
          ExceptionIndexImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          PopArgs(imm.exception->ToFunctionSig());
          CALL_INTERFACE_IF_REACHABLE(Throw, imm, VectorOf(args_));
          EndControl();
          break;
        }
        case kExprTry: {
          CHECK_PROTOTYPE_OPCODE(eh);
          BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (!this->Validate(imm)) break;
          PopArgs(imm.sig);
          auto* try_block = PushControl(kControlTry);
          SetBlockType(try_block, imm);
          len = 1 + imm.length;
          CALL_INTERFACE_IF_REACHABLE(Try, try_block);
          PushMergeValues(try_block, &try_block->start_merge);
          break;
        }
        case kExprCatch: {
          CHECK_PROTOTYPE_OPCODE(eh);
          if (!VALIDATE(!control_.empty())) {
            this->error("catch does not match any try");
            break;
          }
          Control* c = &control_.back();
          if (!VALIDATE(c->is_try())) {
            this->error("catch does not match any try");
            break;
          }
          if (!VALIDATE(c->is_incomplete_try())) {
            this->error("catch already present for try");
            break;
          }
          c->kind = kControlTryCatch;
          FallThruTo(c);
          stack_.erase(stack_.begin() + c->stack_depth, stack_.end());
          c->reachability = control_at(1)->innerReachability();
          auto* exception = Push(kWasmExceptRef);
          CALL_INTERFACE_IF_PARENT_REACHABLE(Catch, c, exception);
          break;
        }
        case kExprBrOnExn: {
          CHECK_PROTOTYPE_OPCODE(eh);
          BranchDepthImmediate<validate> imm_br(this, this->pc_);
          if (!this->Validate(this->pc_, imm_br, control_.size())) break;
          ExceptionIndexImmediate<validate> imm_idx(this,
                                                    this->pc_ + imm_br.length);
          if (!this->Validate(this->pc_ + imm_br.length, imm_idx)) break;
          Control* c = control_at(imm_br.depth);
          auto exception = Pop(0, kWasmExceptRef);
          const WasmExceptionSig* sig = imm_idx.exception->sig;
          size_t value_count = sig->parameter_count();
          // TODO(mstarzinger): This operand stack mutation is an ugly hack to
          // make both type checking here as well as environment merging in the
          // graph builder interface work out of the box. We should introduce
          // special handling for both and do minimal/no stack mutation here.
          for (size_t i = 0; i < value_count; ++i) Push(sig->GetParam(i));
          Vector<Value> values(stack_.data() + c->stack_depth, value_count);
          if (!TypeCheckBranch(c)) break;
          if (control_.back().reachable()) {
            CALL_INTERFACE(BrOnException, exception, imm_idx, imm_br.depth,
                           values);
            c->br_merge()->reached = true;
          }
          len = 1 + imm_br.length + imm_idx.length;
          for (size_t i = 0; i < value_count; ++i) Pop();
          auto* pexception = Push(kWasmExceptRef);
          *pexception = exception;
          break;
        }
        case kExprLoop: {
          BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (!this->Validate(imm)) break;
          PopArgs(imm.sig);
          auto* block = PushControl(kControlLoop);
          SetBlockType(&control_.back(), imm);
          len = 1 + imm.length;
          CALL_INTERFACE_IF_REACHABLE(Loop, block);
          PushMergeValues(block, &block->start_merge);
          break;
        }
        case kExprIf: {
          BlockTypeImmediate<validate> imm(this->enabled_, this, this->pc_);
          if (!this->Validate(imm)) break;
          auto cond = Pop(0, kWasmI32);
          PopArgs(imm.sig);
          if (!VALIDATE(this->ok())) break;
          auto* if_block = PushControl(kControlIf);
          SetBlockType(if_block, imm);
          CALL_INTERFACE_IF_REACHABLE(If, cond, if_block);
          len = 1 + imm.length;
          PushMergeValues(if_block, &if_block->start_merge);
          break;
        }
        case kExprElse: {
          if (!VALIDATE(!control_.empty())) {
            this->error("else does not match any if");
            break;
          }
          Control* c = &control_.back();
          if (!VALIDATE(c->is_if())) {
            this->error(this->pc_, "else does not match an if");
            break;
          }
          if (c->is_if_else()) {
            this->error(this->pc_, "else already present for if");
            break;
          }
          if (!TypeCheckFallThru(c)) break;
          c->kind = kControlIfElse;
          CALL_INTERFACE_IF_PARENT_REACHABLE(Else, c);
          if (c->reachable()) c->end_merge.reached = true;
          PushMergeValues(c, &c->start_merge);
          c->reachability = control_at(1)->innerReachability();
          break;
        }
        case kExprEnd: {
          if (!VALIDATE(!control_.empty())) {
            this->error("end does not match any if, try, or block");
            break;
          }
          Control* c = &control_.back();
          if (!VALIDATE(!c->is_incomplete_try())) {
            this->error(this->pc_, "missing catch or catch-all in try");
            break;
          }
          if (c->is_onearmed_if()) {
            if (!VALIDATE(c->end_merge.arity == c->start_merge.arity)) {
              this->error(
                  c->pc,
                  "start-arity and end-arity of one-armed if must match");
              break;
            }
          }

          if (!TypeCheckFallThru(c)) break;

          if (control_.size() == 1) {
            // If at the last (implicit) control, check we are at end.
            if (!VALIDATE(this->pc_ + 1 == this->end_)) {
              this->error(this->pc_ + 1, "trailing code after function end");
              break;
            }
            // The result of the block is the return value.
            TRACE_PART("\n" TRACE_INST_FORMAT, startrel(this->pc_),
                       "(implicit) return");
            DoReturn();
            control_.clear();
            break;
          }

          PopControl(c);
          break;
        }
        case kExprSelect: {
          auto cond = Pop(2, kWasmI32);
          auto fval = Pop();
          auto tval = Pop(0, fval.type);
          auto* result = Push(tval.type == kWasmVar ? fval.type : tval.type);
          CALL_INTERFACE_IF_REACHABLE(Select, cond, fval, tval, result);
          break;
        }
        case kExprOffset32: {
          auto scale = Pop(2, kWasmI32);
          auto index = Pop(1, kWasmI32);
          auto base = Pop(0, kWasmI32);
          auto* result = Push(kWasmI32);
          CALL_INTERFACE_IF_REACHABLE(Offset32, base, index, scale, result);
          break;
        }
        case kExprBr: {
          BranchDepthImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(this->pc_, imm, control_.size())) break;
          Control* c = control_at(imm.depth);
          if (!TypeCheckBranch(c)) break;
          if (imm.depth == control_.size() - 1) {
            DoReturn();
          } else if (control_.back().reachable()) {
            CALL_INTERFACE(Br, c);
            c->br_merge()->reached = true;
          }
          len = 1 + imm.length;
          EndControl();
          break;
        }
        case kExprBrIf: {
          BranchDepthImmediate<validate> imm(this, this->pc_);
          auto cond = Pop(0, kWasmI32);
          if (this->failed()) break;
          if (!this->Validate(this->pc_, imm, control_.size())) break;
          Control* c = control_at(imm.depth);
          if (!TypeCheckBranch(c)) break;
          if (control_.back().reachable()) {
            CALL_INTERFACE(BrIf, cond, imm.depth);
            c->br_merge()->reached = true;
          }
          len = 1 + imm.length;
          break;
        }
        case kExprBrTable: {
          BranchTableImmediate<validate> imm(this, this->pc_);
          BranchTableIterator<validate> iterator(this, imm);
          auto key = Pop(0, kWasmI32);
          if (this->failed()) break;
          if (!this->Validate(this->pc_, imm, control_.size())) break;
          uint32_t br_arity = 0;
          std::vector<bool> br_targets(control_.size());
          while (iterator.has_next()) {
            const uint32_t i = iterator.cur_index();
            const byte* pos = iterator.pc();
            uint32_t target = iterator.next();
            if (!VALIDATE(target < control_.size())) {
              this->errorf(pos,
                           "improper branch in br_table target %u (depth %u)",
                           i, target);
              break;
            }
            // Avoid redundant branch target checks.
            if (br_targets[target]) continue;
            br_targets[target] = true;
            // Check that label types match up.
            Control* c = control_at(target);
            uint32_t arity = c->br_merge()->arity;
            if (i == 0) {
              br_arity = arity;
            } else if (!VALIDATE(br_arity == arity)) {
              this->errorf(pos,
                           "inconsistent arity in br_table target %u"
                           " (previous was %u, this one %u)",
                           i, br_arity, arity);
            }
            if (!TypeCheckBranch(c)) break;
          }
          if (this->failed()) break;

          if (control_.back().reachable()) {
            CALL_INTERFACE(BrTable, imm, key);

            for (uint32_t depth = control_depth(); depth-- > 0;) {
              if (!br_targets[depth]) continue;
              control_at(depth)->br_merge()->reached = true;
            }
          }

          len = 1 + iterator.length();
          EndControl();
          break;
        }
        case kExprReturn: {
          if (!TypeCheckReturn()) break;
          DoReturn();
          EndControl();
          break;
        }
        case kExprUnreachable: {
          CALL_INTERFACE_IF_REACHABLE(Unreachable);
          EndControl();
          break;
        }
        case kExprI32Const: {
          ImmI32Immediate<validate> imm(this, this->pc_);
          auto* value = Push(kWasmI32);
          CALL_INTERFACE_IF_REACHABLE(I32Const, value, imm.value);
          len = 1 + imm.length;
          break;
        }
        case kExprI64Const: {
          ImmI64Immediate<validate> imm(this, this->pc_);
          auto* value = Push(kWasmI64);
          CALL_INTERFACE_IF_REACHABLE(I64Const, value, imm.value);
          len = 1 + imm.length;
          break;
        }
        case kExprF32Const: {
          ImmF32Immediate<validate> imm(this, this->pc_);
          auto* value = Push(kWasmF32);
          CALL_INTERFACE_IF_REACHABLE(F32Const, value, imm.value);
          len = 1 + imm.length;
          break;
        }
        case kExprF64Const: {
          ImmF64Immediate<validate> imm(this, this->pc_);
          auto* value = Push(kWasmF64);
          CALL_INTERFACE_IF_REACHABLE(F64Const, value, imm.value);
          len = 1 + imm.length;
          break;
        }
        case kExprRefNull: {
          CHECK_PROTOTYPE_OPCODE(anyref);
          auto* value = Push(kWasmAnyRef);
          CALL_INTERFACE_IF_REACHABLE(RefNull, value);
          len = 1;
          break;
        }
        case kExprGetLocal: {
          LocalIndexImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(this->pc_, imm)) break;
          auto* value = Push(imm.type);
          CALL_INTERFACE_IF_REACHABLE(GetLocal, value, imm);
          len = 1 + imm.length;
          break;
        }
        case kExprSetLocal: {
          LocalIndexImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(this->pc_, imm)) break;
          auto value = Pop(0, local_type_vec_[imm.index]);
          CALL_INTERFACE_IF_REACHABLE(SetLocal, value, imm);
          len = 1 + imm.length;
          break;
        }
        case kExprTeeLocal: {
          LocalIndexImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(this->pc_, imm)) break;
          auto value = Pop(0, local_type_vec_[imm.index]);
          auto* result = Push(value.type);
          CALL_INTERFACE_IF_REACHABLE(TeeLocal, value, result, imm);
          len = 1 + imm.length;
          break;
        }
        case kExprDrop: {
          auto value = Pop();
          CALL_INTERFACE_IF_REACHABLE(Drop, value);
          break;
        }
        case kExprGetGlobal: {
          GlobalIndexImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          auto* result = Push(imm.type);
          CALL_INTERFACE_IF_REACHABLE(GetGlobal, result, imm);
          break;
        }
        case kExprSetGlobal: {
          GlobalIndexImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          if (!VALIDATE(imm.global->mutability)) {
            this->errorf(this->pc_, "immutable global #%u cannot be assigned",
                         imm.index);
            break;
          }
          auto value = Pop(0, imm.type);
          CALL_INTERFACE_IF_REACHABLE(SetGlobal, value, imm);
          break;
        }
        case kExprI32LoadMem8S:
          len = 1 + DecodeLoadMem(LoadType::kI32Load8S);
          break;
        case kExprI32LoadMem8U:
          len = 1 + DecodeLoadMem(LoadType::kI32Load8U);
          break;
        case kExprI32LoadMem16S:
          len = 1 + DecodeLoadMem(LoadType::kI32Load16S);
          break;
        case kExprI32LoadMem16U:
          len = 1 + DecodeLoadMem(LoadType::kI32Load16U);
          break;
        case kExprI32LoadMem:
          len = 1 + DecodeLoadMem(LoadType::kI32Load);
          break;
        case kExprI64LoadMem8S:
          len = 1 + DecodeLoadMem(LoadType::kI64Load8S);
          break;
        case kExprI64LoadMem8U:
          len = 1 + DecodeLoadMem(LoadType::kI64Load8U);
          break;
        case kExprI64LoadMem16S:
          len = 1 + DecodeLoadMem(LoadType::kI64Load16S);
          break;
        case kExprI64LoadMem16U:
          len = 1 + DecodeLoadMem(LoadType::kI64Load16U);
          break;
        case kExprI64LoadMem32S:
          len = 1 + DecodeLoadMem(LoadType::kI64Load32S);
          break;
        case kExprI64LoadMem32U:
          len = 1 + DecodeLoadMem(LoadType::kI64Load32U);
          break;
        case kExprI64LoadMem:
          len = 1 + DecodeLoadMem(LoadType::kI64Load);
          break;
        case kExprF32LoadMem:
          len = 1 + DecodeLoadMem(LoadType::kF32Load);
          break;
        case kExprF64LoadMem:
          len = 1 + DecodeLoadMem(LoadType::kF64Load);
          break;
        case kExprI32StoreMem8:
          len = 1 + DecodeStoreMem(StoreType::kI32Store8);
          break;
        case kExprI32StoreMem16:
          len = 1 + DecodeStoreMem(StoreType::kI32Store16);
          break;
        case kExprI32StoreMem:
          len = 1 + DecodeStoreMem(StoreType::kI32Store);
          break;
        case kExprI64StoreMem8:
          len = 1 + DecodeStoreMem(StoreType::kI64Store8);
          break;
        case kExprI64StoreMem16:
          len = 1 + DecodeStoreMem(StoreType::kI64Store16);
          break;
        case kExprI64StoreMem32:
          len = 1 + DecodeStoreMem(StoreType::kI64Store32);
          break;
        case kExprI64StoreMem:
          len = 1 + DecodeStoreMem(StoreType::kI64Store);
          break;
        case kExprF32StoreMem:
          len = 1 + DecodeStoreMem(StoreType::kF32Store);
          break;
        case kExprF64StoreMem:
          len = 1 + DecodeStoreMem(StoreType::kF64Store);
          break;
        case kExprMemoryGrow: {
          if (!CheckHasMemory()) break;
          MemoryIndexImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          DCHECK_NOT_NULL(this->module_);
          if (!VALIDATE(this->module_->origin == kWasmOrigin)) {
            this->error("grow_memory is not supported for asmjs modules");
            break;
          }
          auto value = Pop(0, kWasmI32);
          auto* result = Push(kWasmI32);
          CALL_INTERFACE_IF_REACHABLE(MemoryGrow, value, result);
          break;
        }
        case kExprMemorySize: {
          if (!CheckHasMemory()) break;
          MemoryIndexImmediate<validate> imm(this, this->pc_);
          auto* result = Push(kWasmI32);
          len = 1 + imm.length;
          CALL_INTERFACE_IF_REACHABLE(CurrentMemoryPages, result);
          break;
        }
        case kExprCallNativeFunction: {
          CallNativeFunctionImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          PopArgs(imm.native->sig);
          auto* returns = PushReturns(imm.native->sig);
          CALL_INTERFACE_IF_REACHABLE(CallNative, imm, args_.data(), returns);
          break;
        }
        case kExprCallFunction: {
          CallFunctionImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          // TODO(clemensh): Better memory management.
          PopArgs(imm.sig);
          auto* returns = PushReturns(imm.sig);
          CALL_INTERFACE_IF_REACHABLE(CallDirect, imm, args_.data(), returns);
          break;
        }
        case kExprCallIndirect: {
          CallIndirectImmediate<validate> imm(this, this->pc_);
          len = 1 + imm.length;
          if (!this->Validate(this->pc_, imm)) break;
          auto index = Pop(0, kWasmI32);
          PopArgs(imm.sig);
          auto* returns = PushReturns(imm.sig);
          CALL_INTERFACE_IF_REACHABLE(CallIndirect, index, imm, args_.data(),
                                      returns);
          break;
        }
        case kNumericPrefix: {
          ++len;
          byte numeric_index =
              this->template read_u8<validate>(this->pc_ + 1, "numeric index");
          opcode = static_cast<WasmOpcode>(opcode << 8 | numeric_index);
          if (opcode < kExprMemoryInit) {
            CHECK_PROTOTYPE_OPCODE(sat_f2i_conversions);
          } else {
            CHECK_PROTOTYPE_OPCODE(bulk_memory);
          }
          TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                     WasmOpcodes::OpcodeName(opcode));
          len += DecodeNumericOpcode(opcode);
          break;
        }
        case kSimdPrefix: {
          CHECK_PROTOTYPE_OPCODE(simd);
          len++;
          byte simd_index =
              this->template read_u8<validate>(this->pc_ + 1, "simd index");
          opcode = static_cast<WasmOpcode>(opcode << 8 | simd_index);
          TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                     WasmOpcodes::OpcodeName(opcode));
          len += DecodeSimdOpcode(opcode);
          break;
        }
        case kAtomicPrefix: {
          CHECK_PROTOTYPE_OPCODE(threads);
          if (!CheckHasSharedMemory()) break;
          len++;
          byte atomic_index =
              this->template read_u8<validate>(this->pc_ + 1, "atomic index");
          opcode = static_cast<WasmOpcode>(opcode << 8 | atomic_index);
          TRACE_PART(TRACE_INST_FORMAT, startrel(this->pc_),
                     WasmOpcodes::OpcodeName(opcode));
          len += DecodeAtomicOpcode(opcode);
          break;
        }
// Note that prototype opcodes are not handled in the fastpath
// above this switch, to avoid checking a feature flag.
#define SIMPLE_PROTOTYPE_CASE(name, opc, sig) \
  case kExpr##name: /* fallthrough */
          FOREACH_SIMPLE_PROTOTYPE_OPCODE(SIMPLE_PROTOTYPE_CASE)
#undef SIMPLE_PROTOTYPE_CASE
          BuildSimplePrototypeOperator(opcode);
          break;
        default: {
          // Deal with special asmjs opcodes.
          if (this->module_ != nullptr &&
              this->module_->origin == kAsmJsOrigin) {
            FunctionSig* sig = WasmOpcodes::AsmjsSignature(opcode);
            if (sig) {
              BuildSimpleOperator(opcode, sig);
            }
          } else {
            this->error("Invalid opcode");
            return;
          }
        }
      }

#if DEBUG
      if (FLAG_trace_wasm_decoder) {
        TRACE_PART(" ");
        for (Control& c : control_) {
          switch (c.kind) {
            case kControlIf:
              TRACE_PART("I");
              break;
            case kControlBlock:
              TRACE_PART("B");
              break;
            case kControlLoop:
              TRACE_PART("L");
              break;
            case kControlTry:
              TRACE_PART("T");
              break;
            default:
              break;
          }
          if (c.start_merge.arity) TRACE_PART("%u-", c.start_merge.arity);
          TRACE_PART("%u", c.end_merge.arity);
          if (!c.reachable()) TRACE_PART("%c", c.unreachable() ? '*' : '#');
        }
        TRACE_PART(" | ");
        for (size_t i = 0; i < stack_.size(); ++i) {
          auto& val = stack_[i];
          WasmOpcode opcode = static_cast<WasmOpcode>(*val.pc);
          if (WasmOpcodes::IsPrefixOpcode(opcode)) {
            opcode = static_cast<WasmOpcode>(opcode << 8 | *(val.pc + 1));
          }
          TRACE_PART(" %c@%d:%s", ValueTypes::ShortNameOf(val.type),
                     static_cast<int>(val.pc - this->start_),
                     WasmOpcodes::OpcodeName(opcode));
          // If the decoder failed, don't try to decode the immediates, as this
          // can trigger a DCHECK failure.
          if (this->failed()) continue;
          switch (opcode) {
            case kExprI32Const: {
              ImmI32Immediate<Decoder::kNoValidate> imm(this, val.pc);
              TRACE_PART("[%d]", imm.value);
              break;
            }
            case kExprGetLocal:
            case kExprSetLocal:
            case kExprTeeLocal: {
              LocalIndexImmediate<Decoder::kNoValidate> imm(this, val.pc);
              TRACE_PART("[%u]", imm.index);
              break;
            }
            case kExprGetGlobal:
            case kExprSetGlobal: {
              GlobalIndexImmediate<Decoder::kNoValidate> imm(this, val.pc);
              TRACE_PART("[%u]", imm.index);
              break;
            }
            default:
              break;
          }
        }
      }
#endif
      this->pc_ += len;
    }  // end decode loop
    if (!VALIDATE(this->pc_ == this->end_) && this->ok()) {
      this->error("Beyond end of code");
    }
  }

  void EndControl() {
    DCHECK(!control_.empty());
    auto* current = &control_.back();
    stack_.erase(stack_.begin() + current->stack_depth, stack_.end());
    CALL_INTERFACE_IF_REACHABLE(EndControl, current);
    current->reachability = kUnreachable;
  }

  template<typename func>
  void InitMerge(Merge<Value>* merge, uint32_t arity, func get_val) {
    merge->arity = arity;
    if (arity == 1) {
      merge->vals.first = get_val(0);
    } else if (arity > 1) {
      merge->vals.array = zone_->NewArray<Value>(arity);
      for (uint32_t i = 0; i < arity; i++) {
        merge->vals.array[i] = get_val(i);
      }
    }
  }

  void SetBlockType(Control* c, BlockTypeImmediate<validate>& imm) {
    DCHECK_EQ(imm.in_arity(), this->args_.size());
    const byte* pc = this->pc_;
    Value* args = this->args_.data();
    InitMerge(&c->end_merge, imm.out_arity(), [pc, &imm](uint32_t i) {
      return Value{pc, imm.out_type(i)};
    });
    InitMerge(&c->start_merge, imm.in_arity(),
              [args](uint32_t i) { return args[i]; });
  }

  // Pops arguments as required by signature into {args_}.
  V8_INLINE void PopArgs(FunctionSig* sig) {
    int count = sig ? static_cast<int>(sig->parameter_count()) : 0;
    args_.resize(count, UnreachableValue(nullptr));
    for (int i = count - 1; i >= 0; --i) {
      args_[i] = Pop(i, sig->GetParam(i));
    }
  }

  ValueType GetReturnType(FunctionSig* sig) {
    DCHECK_GE(1, sig->return_count());
    return sig->return_count() == 0 ? kWasmStmt : sig->GetReturn();
  }

  Control* PushControl(ControlKind kind) {
    Reachability reachability =
        control_.empty() ? kReachable : control_.back().innerReachability();
    control_.emplace_back(kind, stack_size(), this->pc_, reachability);
    return &control_.back();
  }

  void PopControl(Control* c) {
    DCHECK_EQ(c, &control_.back());
    CALL_INTERFACE_IF_PARENT_REACHABLE(PopControl, c);

    // A loop just leaves the values on the stack.
    if (!c->is_loop()) PushMergeValues(c, &c->end_merge);

    bool parent_reached =
        c->reachable() || c->end_merge.reached || c->is_onearmed_if();
    control_.pop_back();
    // If the parent block was reachable before, but the popped control does not
    // return to here, this block becomes "spec only reachable".
    if (!parent_reached && control_.back().reachable()) {
      control_.back().reachability = kSpecOnlyReachable;
    }
  }

  int DecodeLoadMem(LoadType type, int prefix_len = 0) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessImmediate<validate> imm(this, this->pc_ + prefix_len,
                                        type.size_log_2());
    auto index = Pop(0, kWasmI32);
    auto* result = Push(type.value_type());
    CALL_INTERFACE_IF_REACHABLE(LoadMem, type, imm, index, result);
    return imm.length;
  }

  int DecodeStoreMem(StoreType store, int prefix_len = 0) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessImmediate<validate> imm(this, this->pc_ + prefix_len,
                                        store.size_log_2());
    auto value = Pop(1, store.value_type());
    auto index = Pop(0, kWasmI32);
    CALL_INTERFACE_IF_REACHABLE(StoreMem, store, imm, index, value);
    return imm.length;
  }

  uint32_t SimdExtractLane(WasmOpcode opcode, ValueType type) {
    SimdLaneImmediate<validate> imm(this, this->pc_);
    if (this->Validate(this->pc_, opcode, imm)) {
      Value inputs[] = {Pop(0, kWasmS128)};
      auto* result = Push(type);
      CALL_INTERFACE_IF_REACHABLE(SimdLaneOp, opcode, imm, ArrayVector(inputs),
                                  result);
    }
    return imm.length;
  }

  uint32_t SimdReplaceLane(WasmOpcode opcode, ValueType type) {
    SimdLaneImmediate<validate> imm(this, this->pc_);
    if (this->Validate(this->pc_, opcode, imm)) {
      Value inputs[2] = {UnreachableValue(this->pc_),
                         UnreachableValue(this->pc_)};
      inputs[1] = Pop(1, type);
      inputs[0] = Pop(0, kWasmS128);
      auto* result = Push(kWasmS128);
      CALL_INTERFACE_IF_REACHABLE(SimdLaneOp, opcode, imm, ArrayVector(inputs),
                                  result);
    }
    return imm.length;
  }

  uint32_t SimdShiftOp(WasmOpcode opcode) {
    SimdShiftImmediate<validate> imm(this, this->pc_);
    if (this->Validate(this->pc_, opcode, imm)) {
      auto input = Pop(0, kWasmS128);
      auto* result = Push(kWasmS128);
      CALL_INTERFACE_IF_REACHABLE(SimdShiftOp, opcode, imm, input, result);
    }
    return imm.length;
  }

  uint32_t Simd8x16ShuffleOp() {
    Simd8x16ShuffleImmediate<validate> imm(this, this->pc_);
    if (this->Validate(this->pc_, imm)) {
      auto input1 = Pop(1, kWasmS128);
      auto input0 = Pop(0, kWasmS128);
      auto* result = Push(kWasmS128);
      CALL_INTERFACE_IF_REACHABLE(Simd8x16ShuffleOp, imm, input0, input1,
                                  result);
    }
    return 16;
  }

  uint32_t DecodeSimdOpcode(WasmOpcode opcode) {
    uint32_t len = 0;
    switch (opcode) {
      case kExprF32x4ExtractLane: {
        len = SimdExtractLane(opcode, kWasmF32);
        break;
      }
      case kExprI32x4ExtractLane:
      case kExprI16x8ExtractLane:
      case kExprI8x16ExtractLane: {
        len = SimdExtractLane(opcode, kWasmI32);
        break;
      }
      case kExprF32x4ReplaceLane: {
        len = SimdReplaceLane(opcode, kWasmF32);
        break;
      }
      case kExprI32x4ReplaceLane:
      case kExprI16x8ReplaceLane:
      case kExprI8x16ReplaceLane: {
        len = SimdReplaceLane(opcode, kWasmI32);
        break;
      }
      case kExprI32x4Shl:
      case kExprI32x4ShrS:
      case kExprI32x4ShrU:
      case kExprI16x8Shl:
      case kExprI16x8ShrS:
      case kExprI16x8ShrU:
      case kExprI8x16Shl:
      case kExprI8x16ShrS:
      case kExprI8x16ShrU: {
        len = SimdShiftOp(opcode);
        break;
      }
      case kExprS8x16Shuffle: {
        len = Simd8x16ShuffleOp();
        break;
      }
      case kExprS128LoadMem:
        len = DecodeLoadMem(LoadType::kS128Load, 1);
        break;
      case kExprS128StoreMem:
        len = DecodeStoreMem(StoreType::kS128Store, 1);
        break;
      default: {
        FunctionSig* sig = WasmOpcodes::Signature(opcode);
        if (!VALIDATE(sig != nullptr)) {
          this->error("invalid simd opcode");
          break;
        }
        PopArgs(sig);
        auto* results =
            sig->return_count() == 0 ? nullptr : Push(GetReturnType(sig));
        CALL_INTERFACE_IF_REACHABLE(SimdOp, opcode, VectorOf(args_), results);
      }
    }
    return len;
  }

  uint32_t DecodeAtomicOpcode(WasmOpcode opcode) {
    uint32_t len = 0;
    ValueType ret_type;
    FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (sig != nullptr) {
      MachineType memtype;
      switch (opcode) {
#define CASE_ATOMIC_STORE_OP(Name, Type) \
  case kExpr##Name: {                    \
    memtype = MachineType::Type();       \
    ret_type = kWasmStmt;                \
    break;                               \
  }
        ATOMIC_STORE_OP_LIST(CASE_ATOMIC_STORE_OP)
#undef CASE_ATOMIC_OP
#define CASE_ATOMIC_OP(Name, Type) \
  case kExpr##Name: {              \
    memtype = MachineType::Type(); \
    ret_type = GetReturnType(sig); \
    break;                         \
  }
        ATOMIC_OP_LIST(CASE_ATOMIC_OP)
#undef CASE_ATOMIC_OP
        default:
          this->error("invalid atomic opcode");
          return 0;
      }
      MemoryAccessImmediate<validate> imm(
          this, this->pc_ + 1, ElementSizeLog2Of(memtype.representation()));
      len += imm.length;
      PopArgs(sig);
      auto result = ret_type == kWasmStmt ? nullptr : Push(GetReturnType(sig));
      CALL_INTERFACE_IF_REACHABLE(AtomicOp, opcode, VectorOf(args_), imm,
                                  result);
    } else {
      this->error("invalid atomic opcode");
    }
    return len;
  }

  unsigned DecodeNumericOpcode(WasmOpcode opcode) {
    unsigned len = 0;
    FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (sig != nullptr) {
      switch (opcode) {
        case kExprI32SConvertSatF32:
        case kExprI32UConvertSatF32:
        case kExprI32SConvertSatF64:
        case kExprI32UConvertSatF64:
        case kExprI64SConvertSatF32:
        case kExprI64UConvertSatF32:
        case kExprI64SConvertSatF64:
        case kExprI64UConvertSatF64:
          BuildSimpleOperator(opcode, sig);
          break;
        case kExprMemoryInit: {
          MemoryInitImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(imm)) break;
          len += imm.length;
          auto size = Pop(2, sig->GetParam(2));
          auto src = Pop(1, sig->GetParam(1));
          auto dst = Pop(0, sig->GetParam(0));
          CALL_INTERFACE_IF_REACHABLE(MemoryInit, imm, dst, src, size);
          break;
        }
        case kExprMemoryDrop: {
          MemoryDropImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(imm)) break;
          len += imm.length;
          CALL_INTERFACE_IF_REACHABLE(MemoryDrop, imm);
          break;
        }
        case kExprMemoryCopy: {
          MemoryIndexImmediate<validate> imm(this, this->pc_ + 1);
          if (!this->Validate(imm)) break;
          len += imm.length;
          auto size = Pop(2, sig->GetParam(2));
          auto src = Pop(1, sig->GetParam(1));
          auto dst = Pop(0, sig->GetParam(0));
          CALL_INTERFACE_IF_REACHABLE(MemoryCopy, imm, dst, src, size);
          break;
        }
        case kExprMemoryFill: {
          MemoryIndexImmediate<validate> imm(this, this->pc_ + 1);
          if (!this->Validate(imm)) break;
          len += imm.length;
          auto size = Pop(2, sig->GetParam(2));
          auto value = Pop(1, sig->GetParam(1));
          auto dst = Pop(0, sig->GetParam(0));
          CALL_INTERFACE_IF_REACHABLE(MemoryFill, imm, dst, value, size);
          break;
        }
        case kExprTableInit: {
          TableInitImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(imm)) break;
          len += imm.length;
          PopArgs(sig);
          CALL_INTERFACE_IF_REACHABLE(TableInit, imm, VectorOf(args_));
          break;
        }
        case kExprTableDrop: {
          TableDropImmediate<validate> imm(this, this->pc_);
          if (!this->Validate(imm)) break;
          len += imm.length;
          CALL_INTERFACE_IF_REACHABLE(TableDrop, imm);
          break;
        }
        case kExprTableCopy: {
          TableIndexImmediate<validate> imm(this, this->pc_ + 1);
          if (!this->Validate(this->pc_ + 1, imm)) break;
          len += imm.length;
          PopArgs(sig);
          CALL_INTERFACE_IF_REACHABLE(TableCopy, imm, VectorOf(args_));
          break;
        }
        default:
          this->error("invalid numeric opcode");
          break;
      }
    } else {
      this->error("invalid numeric opcode");
    }
    return len;
  }

  void DoReturn() {
    size_t return_count = this->sig_->return_count();
    DCHECK_GE(stack_.size(), return_count);
    Vector<Value> return_values =
        return_count == 0
            ? Vector<Value>{}
            : Vector<Value>{&*(stack_.end() - return_count), return_count};

    CALL_INTERFACE_IF_REACHABLE(DoReturn, return_values);
  }

  inline Value* Push(ValueType type) {
    DCHECK_NE(kWasmStmt, type);
    stack_.emplace_back(this->pc_, type);
    return &stack_.back();
  }

  void PushMergeValues(Control* c, Merge<Value>* merge) {
    DCHECK_EQ(c, &control_.back());
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);
    stack_.erase(stack_.begin() + c->stack_depth, stack_.end());
    if (merge->arity == 1) {
      stack_.push_back(merge->vals.first);
    } else {
      for (uint32_t i = 0; i < merge->arity; i++) {
        stack_.push_back(merge->vals.array[i]);
      }
    }
    DCHECK_EQ(c->stack_depth + merge->arity, stack_.size());
  }

  Value* PushReturns(FunctionSig* sig) {
    size_t return_count = sig->return_count();
    if (return_count == 0) return nullptr;
    size_t old_size = stack_.size();
    for (size_t i = 0; i < return_count; ++i) {
      Push(sig->GetReturn(i));
    }
    return stack_.data() + old_size;
  }

  V8_INLINE Value Pop(int index, ValueType expected) {
    auto val = Pop();
    if (!VALIDATE(val.type == expected || val.type == kWasmVar ||
                  expected == kWasmVar)) {
      this->errorf(val.pc, "%s[%d] expected type %s, found %s of type %s",
                   SafeOpcodeNameAt(this->pc_), index,
                   ValueTypes::TypeName(expected), SafeOpcodeNameAt(val.pc),
                   ValueTypes::TypeName(val.type));
    }
    return val;
  }

  V8_INLINE Value Pop() {
    DCHECK(!control_.empty());
    uint32_t limit = control_.back().stack_depth;
    if (stack_.size() <= limit) {
      // Popping past the current control start in reachable code.
      if (!VALIDATE(control_.back().unreachable())) {
        this->errorf(this->pc_, "%s found empty stack",
                     SafeOpcodeNameAt(this->pc_));
      }
      return UnreachableValue(this->pc_);
    }
    auto val = stack_.back();
    stack_.pop_back();
    return val;
  }

  int startrel(const byte* ptr) { return static_cast<int>(ptr - this->start_); }

  void FallThruTo(Control* c) {
    DCHECK_EQ(c, &control_.back());
    if (!TypeCheckFallThru(c)) return;
    if (!c->reachable()) return;

    if (!c->is_loop()) CALL_INTERFACE(FallThruTo, c);
    c->end_merge.reached = true;
  }

  bool TypeCheckMergeValues(Control* c, Merge<Value>* merge) {
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);
    DCHECK_GE(stack_.size(), c->stack_depth + merge->arity);
    // The computation of {stack_values} is only valid if {merge->arity} is >0.
    DCHECK_LT(0, merge->arity);
    Value* stack_values = &*(stack_.end() - merge->arity);
    // Typecheck the topmost {merge->arity} values on the stack.
    for (uint32_t i = 0; i < merge->arity; ++i) {
      Value& val = stack_values[i];
      Value& old = (*merge)[i];
      if (val.type == old.type) continue;
      // If {val.type} is polymorphic, which results from unreachable, make
      // it more specific by using the merge value's expected type.
      // If it is not polymorphic, this is a type error.
      if (!VALIDATE(val.type == kWasmVar)) {
        this->errorf(this->pc_, "type error in merge[%u] (expected %s, got %s)",
                     i, ValueTypes::TypeName(old.type),
                     ValueTypes::TypeName(val.type));
        return false;
      }
      val.type = old.type;
    }

    return true;
  }

  bool TypeCheckFallThru(Control* c) {
    DCHECK_EQ(c, &control_.back());
    if (!validate) return true;
    uint32_t expected = c->end_merge.arity;
    DCHECK_GE(stack_.size(), c->stack_depth);
    uint32_t actual = static_cast<uint32_t>(stack_.size()) - c->stack_depth;
    // Fallthrus must match the arity of the control exactly.
    if (!InsertUnreachablesIfNecessary(expected, actual) || actual > expected) {
      this->errorf(
          this->pc_,
          "expected %u elements on the stack for fallthru to @%d, found %u",
          expected, startrel(c->pc), actual);
      return false;
    }
    if (expected == 0) return true;  // Fast path.

    return TypeCheckMergeValues(c, &c->end_merge);
  }

  bool TypeCheckBranch(Control* c) {
    // Branches must have at least the number of values expected; can have more.
    uint32_t expected = c->br_merge()->arity;
    if (expected == 0) return true;  // Fast path.
    DCHECK_GE(stack_.size(), control_.back().stack_depth);
    uint32_t actual =
        static_cast<uint32_t>(stack_.size()) - control_.back().stack_depth;
    if (!InsertUnreachablesIfNecessary(expected, actual)) {
      this->errorf(this->pc_,
                   "expected %u elements on the stack for br to @%d, found %u",
                   expected, startrel(c->pc), actual);
      return false;
    }
    return TypeCheckMergeValues(c, c->br_merge());
  }

  bool TypeCheckReturn() {
    // Returns must have at least the number of values expected; can have more.
    uint32_t num_returns = static_cast<uint32_t>(this->sig_->return_count());
    DCHECK_GE(stack_.size(), control_.back().stack_depth);
    uint32_t actual =
        static_cast<uint32_t>(stack_.size()) - control_.back().stack_depth;
    if (!InsertUnreachablesIfNecessary(num_returns, actual)) {
      this->errorf(this->pc_,
                   "expected %u elements on the stack for return, found %u",
                   num_returns, actual);
      return false;
    }

    // Typecheck the topmost {num_returns} values on the stack.
    if (num_returns == 0) return true;
    // This line requires num_returns > 0.
    Value* stack_values = &*(stack_.end() - num_returns);
    for (uint32_t i = 0; i < num_returns; ++i) {
      auto& val = stack_values[i];
      ValueType expected_type = this->sig_->GetReturn(i);
      if (val.type == expected_type) continue;
      // If {val.type} is polymorphic, which results from unreachable,
      // make it more specific by using the return's expected type.
      // If it is not polymorphic, this is a type error.
      if (!VALIDATE(val.type == kWasmVar)) {
        this->errorf(this->pc_,
                     "type error in return[%u] (expected %s, got %s)", i,
                     ValueTypes::TypeName(expected_type),
                     ValueTypes::TypeName(val.type));
        return false;
      }
      val.type = expected_type;
    }
    return true;
  }

  inline bool InsertUnreachablesIfNecessary(uint32_t expected,
                                            uint32_t actual) {
    if (V8_LIKELY(actual >= expected)) {
      return true;  // enough actual values are there.
    }
    if (!VALIDATE(control_.back().unreachable())) {
      // There aren't enough values on the stack.
      return false;
    }
    // A slow path. When the actual number of values on the stack is less
    // than the expected number of values and the current control is
    // unreachable, insert unreachable values below the actual values.
    // This simplifies {TypeCheckMergeValues}.
    auto pos = stack_.begin() + (stack_.size() - actual);
    stack_.insert(pos, expected - actual, UnreachableValue(this->pc_));
    return true;
  }

  void onFirstError() override {
    this->end_ = this->pc_;  // Terminate decoding loop.
    TRACE(" !%s\n", this->error_.message().c_str());
    CALL_INTERFACE(OnFirstError);
  }

  void BuildSimplePrototypeOperator(WasmOpcode opcode) {
    if (WasmOpcodes::IsSignExtensionOpcode(opcode)) {
      RET_ON_PROTOTYPE_OPCODE(se);
    }
    if (WasmOpcodes::IsAnyRefOpcode(opcode)) {
      RET_ON_PROTOTYPE_OPCODE(anyref);
    }
    FunctionSig* sig = WasmOpcodes::Signature(opcode);
    BuildSimpleOperator(opcode, sig);
  }

  void BuildSimpleOperator(WasmOpcode opcode, FunctionSig* sig) {
    switch (sig->parameter_count()) {
      case 1: {
        auto val = Pop(0, sig->GetParam(0));
        auto* ret =
            sig->return_count() == 0 ? nullptr : Push(sig->GetReturn(0));
        CALL_INTERFACE_IF_REACHABLE(UnOp, opcode, val, ret);
        break;
      }
      case 2: {
        auto rval = Pop(1, sig->GetParam(1));
        auto lval = Pop(0, sig->GetParam(0));
        auto* ret =
            sig->return_count() == 0 ? nullptr : Push(sig->GetReturn(0));
        CALL_INTERFACE_IF_REACHABLE(BinOp, opcode, lval, rval, ret);
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  void BuildSimpleOperator(WasmOpcode opcode, ValueType return_type,
                           ValueType arg_type) {
    auto val = Pop(0, arg_type);
    auto* ret = return_type == kWasmStmt ? nullptr : Push(return_type);
    CALL_INTERFACE_IF_REACHABLE(UnOp, opcode, val, ret);
  }

  void BuildSimpleOperator(WasmOpcode opcode, ValueType return_type,
                           ValueType lhs_type, ValueType rhs_type) {
    auto rval = Pop(1, rhs_type);
    auto lval = Pop(0, lhs_type);
    auto* ret = return_type == kWasmStmt ? nullptr : Push(return_type);
    CALL_INTERFACE_IF_REACHABLE(BinOp, opcode, lval, rval, ret);
  }

#define DEFINE_SIMPLE_SIG_OPERATOR(sig, ...)          \
  void BuildSimpleOperator_##sig(WasmOpcode opcode) { \
    BuildSimpleOperator(opcode, __VA_ARGS__);         \
  }
  FOREACH_SIGNATURE(DEFINE_SIMPLE_SIG_OPERATOR)
#undef DEFINE_SIMPLE_SIG_OPERATOR
};

#undef CALL_INTERFACE
#undef CALL_INTERFACE_IF_REACHABLE
#undef CALL_INTERFACE_IF_PARENT_REACHABLE

class EmptyInterface {
 public:
  static constexpr Decoder::ValidateFlag validate = Decoder::kValidate;
  using Value = ValueBase;
  using Control = ControlBase<Value>;
  using FullDecoder = WasmFullDecoder<validate, EmptyInterface>;

#define DEFINE_EMPTY_CALLBACK(name, ...) \
  void name(FullDecoder* decoder, ##__VA_ARGS__) {}
  INTERFACE_FUNCTIONS(DEFINE_EMPTY_CALLBACK)
#undef DEFINE_EMPTY_CALLBACK
};

#undef TRACE
#undef TRACE_INST_FORMAT
#undef VALIDATE
#undef CHECK_PROTOTYPE_OPCODE
#undef OPCODE_ERROR

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_BODY_DECODER_IMPL_H_
