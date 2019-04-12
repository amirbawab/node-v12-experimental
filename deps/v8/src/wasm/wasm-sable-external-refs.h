// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_
#define V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_

#include "src/wasm/value-type.h"
#include "src/v8memory.h"
#include <vector>

namespace v8 {
namespace internal {
namespace wasm {
namespace native {

class Memory {
public:
  Memory(Address s, Address e) : start_(reinterpret_cast<byte*>(s)), end_(reinterpret_cast<byte*>(e)) {}
  template <class T>
  void Write(int offset, T val) {
    CHECK_LT(start_ + offset + sizeof(T), end_);
    WriteUnalignedValue<T>(reinterpret_cast<Address>(start_ + offset), val);
  }
  template <class T>
  T Read(int offset) {
    CHECK_LT(start_ + offset + sizeof(T), end_);
    return ReadUnalignedValue<T>(reinterpret_cast<Address>(start_ + offset));
  }
private:
  byte* start_ = nullptr;
  byte* end_ = nullptr;
};

class Argument {
public:
  Argument(Address add, ValueType type) : val_add_(add), type_(type) {}
  template <class T> T Get() const;
private:
  Address val_add_;
  ValueType type_;
};

class Return {
public:
  Return(Address add, ValueType type) : ret_add_(add), type_(type) {}
  template <class T> void Set(T val);
  ValueType type() const { return type_; }
private:
  ValueType type_;
  Address ret_add_;
};

typedef std::vector<Argument> arg_vec_t;
typedef std::vector<Return> ret_vec_t;
typedef void (*native_t)(arg_vec_t , ret_vec_t, Memory);

// initialize environment
void init_native();

// register a native function
bool register_native_function(std::string name, native_t func, std::vector<ValueType> args, std::vector<ValueType> rets);

// find native function and set its index
bool find_native_function(const char* find_name, FunctionSig* sig, int *out_index);

// entry point to calling a native function
// this gateway function will call the appropriate
// function based on the given function id
int native_function_gateway(int funcId, Address mem, Address data);

}  // namespace native
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_SABLE_EXTERNAL_REFS_H_
