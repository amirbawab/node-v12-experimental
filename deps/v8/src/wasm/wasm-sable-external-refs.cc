#include "src/wasm/wasm-sable-external-refs.h"
#include "src/signature.h"
#include "src/base/ieee754.h"
#include <vector>
#include <iostream>

namespace v8 {
namespace internal {
namespace wasm {

/********************
 * Helper functions
 ********************/
template <class T>
inline T ReadValue(byte* address) {
  return ReadUnalignedValue<T>(reinterpret_cast<Address>(address));
}

template <class T>
inline T ReadValueAndAdvance(byte** address) {
  T val = ReadValue<T>(*address);
  *address += sizeof(T);
  return val;
}

template <class T>
inline void WriteValue(T* address, T val) {
  WriteUnalignedValue<T>(reinterpret_cast<Address>(address), val);
}

template <class T>
inline void WriteValueAndAdvance(T** address, T val) {
  WriteValue(*address, val);
  *address += sizeof(T);
}

/*********************
 * Defines
 *********************/
#define ___ void
#define I32 int32_t
#define I64 int64_t
#define F32 float
#define F64 double

// all native functions should be declared
// in FOREACH_NATIVE_FUNCTION_[NON_]TEMPLATE
#define FOREACH_NATIVE_FUNCTION_NON_TEMPLATE(V, P, R) \
  V(exp, "f64.exp", ___, P(kWasmF64), R(kWasmF64))

#define FOREACH_NATIVE_FUNCTION_TEMPLATE(V, P, R) \
  V(matrix_multiplication, "i32.mat_mul", I32, P(kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32), R()) \
  V(matrix_multiplication, "i64.mat_mul", I64, P(kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32), R()) \
  V(matrix_multiplication, "f32.mat_mul", F32, P(kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32), R()) \
  V(matrix_multiplication, "f64.mat_mul", F64, P(kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32), R()) \
  V(print_mem, "i32.print", I32, P(kWasmI32, kWasmI32), R()) \
  V(print_mem, "i64.print", I64, P(kWasmI32, kWasmI32), R()) \
  V(print_mem, "f32.print", F32, P(kWasmI32, kWasmI32), R()) \
  V(print_mem, "f64.print", F64, P(kWasmI32, kWasmI32), R())

#define FOREACH_NATIVE_FUNCTION(V, P, R) \
  FOREACH_NATIVE_FUNCTION_NON_TEMPLATE(V, P, R) \
  FOREACH_NATIVE_FUNCTION_TEMPLATE(V, P, R)

// declare functions prototype
#define NATIVE_FUNCTION_PROTOTYPE(function, name, type, param, ret) \
  void f_##function(byte*,byte*);
  FOREACH_NATIVE_FUNCTION_NON_TEMPLATE(NATIVE_FUNCTION_PROTOTYPE, ___, ___)
#undef NATIVE_FUNCTION_PROTOTYPE

#define NATIVE_FUNCTION_PROTOTYPE(function, name, type, param, ret) \
  template<class T> \
  void f_##function(byte*,byte*);
  FOREACH_NATIVE_FUNCTION_TEMPLATE(NATIVE_FUNCTION_PROTOTYPE, ___, ___)
#undef NATIVE_FUNCTION_PROTOTYPE

// use an enum to give each function
// a unique index
enum NativeFunction {
#define NATIVE_FUNCTION_ENUM(function, name, type, param, ret) \
  k##function##_##type,
FOREACH_NATIVE_FUNCTION(NATIVE_FUNCTION_ENUM, ___, ___)
#undef NATIVE_FUNCTION_ENUM
};

// structure to hold the definition
// of a native function
typedef void (*native)(byte*, byte*);
struct NativeFunctionDefinition {
  native func;
  NativeFunction id;
  std::vector<ValueType> params;
  std::vector<ValueType> rets;
  bool sig_match(FunctionSig *sig) {
    if(params.size() != sig->parameter_count() || rets.size() != sig->return_count()) {
      return false;
    }
    for(size_t i=0; i < params.size(); i++) {
      if(params[i] != sig->GetParam(i)) {
        return false;
      }
    }
    for(size_t i=0; i < rets.size(); i++) {
      if(rets[i] != sig->GetReturn(i)) {
        return false;
      }
    }
    return true;
  }
};

// store all native functions into
// a global array
NativeFunctionDefinition g_functions[] = {
#define ARGS_TO_VECTOR(...) {__VA_ARGS__}
#define NATIVE_FUNCTION_ARRAY(function, name, type, param, ret) \
  {f_##function, k##function##_##type, param, ret},
  FOREACH_NATIVE_FUNCTION_NON_TEMPLATE(NATIVE_FUNCTION_ARRAY, ARGS_TO_VECTOR, ARGS_TO_VECTOR)
#undef NATIVE_FUNCTION_ARRAY

#define NATIVE_FUNCTION_ARRAY(function, name, type, param, ret) \
  {f_##function<type>, k##function##_##type, param, ret},
  FOREACH_NATIVE_FUNCTION_TEMPLATE(NATIVE_FUNCTION_ARRAY, ARGS_TO_VECTOR, ARGS_TO_VECTOR)
#undef NATIVE_FUNCTION_ARRAY
#undef ARGS_TO_VECTOR
};

/*******************
 * Native functions
 ******************/

template <class T>
void f_matrix_multiplication(byte* memByte, byte* dataByte) {
  I32 mat1Offset = ReadValueAndAdvance<I32>(&dataByte);
  I32 mat2Offset = ReadValueAndAdvance<I32>(&dataByte);
  I32 resOffset = ReadValueAndAdvance<I32>(&dataByte);
  I32 m = ReadValueAndAdvance<I32>(&dataByte);
  I32 n = ReadValueAndAdvance<I32>(&dataByte);
  I32 p = ReadValueAndAdvance<I32>(&dataByte);

  T* mat1 = reinterpret_cast<T*>(memByte + mat1Offset);
  T* mat2 = reinterpret_cast<T*>(memByte + mat2Offset);
  T* res = reinterpret_cast<T*>(memByte + resOffset);

  for(int r=0; r < m; ++r) {
    for(int c=0; c < p; ++c) {
      T resCell = 0;
      for(int cr=0; cr < n; ++cr) {
        resCell += *(mat1 + r * n + cr) * *(mat2 + cr * p + c);
      }
      WriteValue<T>(res + r * p + c, resCell);
    }
  }
}

template<class T>
void f_print_mem(byte* memByte, byte* dataByte) {
  I32 offset = ReadValueAndAdvance<I32>(&dataByte);
  I32 size = ReadValueAndAdvance<I32>(&dataByte);
  for(int i=0; i < size; ++i) {
    std::cout << *(reinterpret_cast<T*>(memByte + offset + i * sizeof(T))) << " ";
  }
  std::cout << std::endl;
}

void f_exp(byte* memByte, byte* dataByte) {
  F64 x = ReadValueAndAdvance<F64>(&dataByte);
  WriteValue<F64>(reinterpret_cast<F64*>(dataByte), base::ieee754::exp(x));
}

/*******************************
 * External reference functions
 *******************************/

bool find_native_function(const char* find_name, FunctionSig* sig, int *index) {
#define FIND_NATIVE_FUNCTION(function, name, type, param, ret) \
  if(strcmp(name, find_name) == 0) { \
    *index = k##function##_##type; \
    return g_functions[*index].sig_match(sig);\
  }
  FOREACH_NATIVE_FUNCTION(FIND_NATIVE_FUNCTION, ___, ___)
#undef FIND_NATIVE_FUNCTION
  return false;
}

int native_function_gateway(uint32_t functionId, Address mem, Address data) {
  (*g_functions[functionId].func)(reinterpret_cast<byte*>(mem), reinterpret_cast<byte*>(data));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
