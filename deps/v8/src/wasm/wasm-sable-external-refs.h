// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_
#define V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_

#include "src/v8memory.h"

namespace v8 {
namespace internal {
namespace wasm {

int native_function_gateway(uint32_t functionId, Address mem, Address data);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_
