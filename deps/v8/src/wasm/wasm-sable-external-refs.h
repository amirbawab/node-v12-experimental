// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_
#define V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_

#include "src/external-reference.h"
#include "src/wasm/value-type.h"
#include "src/v8memory.h"

namespace v8 {
namespace internal {
namespace wasm {

typedef void (*native_t)(Address, Address);

// find native function and set its index
bool find_native_function(const char* find_name, FunctionSig* sig, ExternalReference (**ref)());

#ifdef ___
#error "___ already defined"
#endif
#ifdef I32
#error "I32 already defined"
#endif
#ifdef I64
#error "I64 already defined"
#endif
#ifdef F32
#error "F32 already defined"
#endif
#ifdef F64
#error "F64 already defined"
#endif

#define ___ void
#define I32 int32_t
#define I64 int64_t
#define F32 float
#define F64 double

// all native functions should be declared
// in FOREACH_NATIVE_FUNCTION_[NON_]TEMPLATE
#define FOREACH_NATIVE_FUNCTION_NON_TEMPLATE(V, P, R) \
  V(time_ms, "i64.time_ms", ___, P(), R(kWasmI64)) \
  V(exp, "f64.exp", ___, P(kWasmF64), R(kWasmF64)) \

#define FOREACH_NATIVE_FUNCTION_TEMPLATE(V, P, R) \
  V(add, "i32.add", I32, P(kWasmI32, kWasmI32), R(kWasmI32)) \
  V(matrix_multiplication, "i32.mat_mul", I32, P(kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32), R()) \
  V(matrix_multiplication, "i64.mat_mul", I64, P(kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32), R()) \
  V(matrix_multiplication, "f32.mat_mul", F32, P(kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32), R()) \
  V(matrix_multiplication, "f64.mat_mul", F64, P(kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32), R()) \
  V(print_stack, "i32.print_s", I32, P(kWasmI32), R()) \
  V(print_stack, "i64.print_s", I64, P(kWasmI64), R()) \
  V(print_stack, "f32.print_s", F32, P(kWasmF32), R()) \
  V(print_stack, "f64.print_s", F64, P(kWasmF64), R()) \
  V(print_memory, "i32.print_m", I32, P(kWasmI32, kWasmI32), R()) \
  V(print_memory, "i64.print_m", I64, P(kWasmI32, kWasmI32), R()) \
  V(print_memory, "f32.print_m", F32, P(kWasmI32, kWasmI32), R()) \
  V(print_memory, "f64.print_m", F64, P(kWasmI32, kWasmI32), R()) \

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_
