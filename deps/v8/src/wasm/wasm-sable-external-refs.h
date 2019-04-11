// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_
#define V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_

#include "src/wasm/value-type.h"
#include "src/v8memory.h"

namespace v8 {
namespace internal {
namespace wasm {

// find native function and set its index
bool find_native_function(const char* find_name, FunctionSig* sig, int *index);

// entry point to calling a native function
// this gateway function will call the appropriate
// function based on the given function id
int native_function_gateway(int funcId, Address mem, Address data);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_
