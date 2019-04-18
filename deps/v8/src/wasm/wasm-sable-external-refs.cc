#include "src/wasm/wasm-sable-external-refs.h"
#include "src/signature.h"
#include "src/base/ieee754.h"
#include <vector>
#include <iostream>
#include <chrono>

namespace v8 {
namespace internal {
namespace wasm {

/********************
 * Helper functions
 ********************/
template <class T>
inline T ReadValue(T* address) {
  return ReadUnalignedValue<T>(reinterpret_cast<Address>(address));
}

template <class T>
inline T ReadValueAndAdvance(T** address) {
  T val = ReadValue<T>(*address);
  (*address)++;
  return val;
}

template <class T>
inline void WriteValue(T* address, T val) {
  WriteUnalignedValue<T>(reinterpret_cast<Address>(address), val);
}

template <class T>
inline void WriteValueAndAdvance(T** address, T val) {
  WriteValue(*address, val);
  (*address)++;
}

/*******************
 * Native functions
 ******************/

template <class T>
void matrix_multiplication_wrapper(Address mem, Address data) {
  I32* stack = reinterpret_cast<I32*>(data);
  byte* lmem = reinterpret_cast<byte*>(mem);

  I32 mat1Offset = *(stack);
  I32 mat2Offset = *(stack += 1);
  I32 resOffset = *(stack += 1);
  I32 m = *(stack += 1);
  I32 n = *(stack += 1);
  I32 p = *(stack += 1);

  T* mat1 = reinterpret_cast<T*>(lmem + mat1Offset);
  T* mat2 = reinterpret_cast<T*>(lmem + mat2Offset);
  T* res = reinterpret_cast<T*>(lmem + resOffset);

  for(int r=0; r < m; ++r) {
    for(int c=0; c < p; ++c) {
      T resCell = 0;
      for(int cr=0; cr < n; ++cr) {
        resCell += *(mat1 + r * n + cr) * *(mat2 + cr * p + c);
      }
      *(res + r * p + c) = resCell;
    }
  }
}
template void matrix_multiplication_wrapper<I32>(Address, Address);
template void matrix_multiplication_wrapper<I64>(Address, Address);
template void matrix_multiplication_wrapper<F32>(Address, Address);
template void matrix_multiplication_wrapper<F64>(Address, Address);

template<class T>
void print_memory_wrapper(Address mem, Address data) {
  I32* stack = reinterpret_cast<I32*>(data);

  I32 offset = ReadValueAndAdvance<I32>(&stack);
  I32 size = ReadValueAndAdvance<I32>(&stack);
  T* lmem = reinterpret_cast<T*>(reinterpret_cast<byte*>(mem) + offset);
  for(int i=0; i < size; ++i) {
    std::cout << ReadValue(lmem + i) << " ";
  }
  std::cout << std::endl;
}
template void print_memory_wrapper<I32>(Address, Address);
template void print_memory_wrapper<I64>(Address, Address);
template void print_memory_wrapper<F32>(Address, Address);
template void print_memory_wrapper<F64>(Address, Address);

void exp_wrapper(Address mem, Address data) {
  F64 x = ReadValueAndAdvance<F64>(reinterpret_cast<F64**>(&data));
  WriteValue<F64>(reinterpret_cast<F64*>(data), base::ieee754::exp(x));
}

template <class T>
void add_wrapper(Address mem, Address data) {
  T* stack = reinterpret_cast<T*>(data);
  T lhs = ReadValueAndAdvance<T>(&stack);
  T rhs = ReadValueAndAdvance<T>(&stack);
  WriteValue<T>(stack, lhs + rhs);
}
template void add_wrapper<I32>(Address, Address);

void time_ms_wrapper(Address mem, Address data) {
  WriteValue<I64>(reinterpret_cast<I64*>(data), std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}

template <class T>
void print_stack_wrapper(Address mem, Address data) {
  std::cout << ReadValue<T>(reinterpret_cast<T*>(data)) << std::endl;
}
template void print_stack_wrapper<I32>(Address, Address);
template void print_stack_wrapper<I64>(Address, Address);
template void print_stack_wrapper<F32>(Address, Address);
template void print_stack_wrapper<F64>(Address, Address);

/**********************
 * Benchmark functions
 *********************/
void do_nothing_wrapper(Address mem, Address data) {}
void do_nothing_i_wrapper(Address mem, Address data) {}
void do_nothing_l_wrapper(Address mem, Address data) {}
void do_nothing_f_wrapper(Address mem, Address data) {}
void do_nothing_d_wrapper(Address mem, Address data) {}
void do_nothing_I_wrapper(Address mem, Address data) {}
void do_nothing_L_wrapper(Address mem, Address data) {}
void do_nothing_F_wrapper(Address mem, Address data) {}
void do_nothing_D_wrapper(Address mem, Address data) {}
void do_nothing_iI_wrapper(Address mem, Address data) {}
void do_nothing_lL_wrapper(Address mem, Address data) {}
void do_nothing_fF_wrapper(Address mem, Address data) {}
void do_nothing_dD_wrapper(Address mem, Address data) {}

/*******************************
 * External reference functions
 *******************************/

bool find_native_function(const char* find_name, FunctionSig* sig, ExternalReference(**ref)()) {
#define ARGS_TO_VECTOR(...) {__VA_ARGS__}
#define FIND_NATIVE_FUNCTION(function, name, type, p, r) \
  if(strcmp(name, find_name) == 0) { \
    std::vector<ValueType> params = p; \
    std::vector<ValueType> rets = r; \
    *ref = ExternalReference::wasm_##function; \
    if(params.size() != sig->parameter_count() || rets.size() != sig->return_count()) { \
      return false; \
    } \
    for(size_t i=0; i < params.size(); i++) { \
      if(params[i] != sig->GetParam(i)) { \
        return false; \
      } \
    } \
    for(size_t i=0; i < rets.size(); i++) { \
      if(rets[i] != sig->GetReturn(i)) { \
        return false; \
      } \
    } \
    return true; \
  }
FOREACH_NATIVE_FUNCTION_NON_TEMPLATE(FIND_NATIVE_FUNCTION, ARGS_TO_VECTOR, ARGS_TO_VECTOR)
#undef FIND_NATIVE_FUNCTION

#define FIND_NATIVE_FUNCTION(function, name, type, p, r) \
  if(strcmp(name, find_name) == 0) { \
    std::vector<ValueType> params = p; \
    std::vector<ValueType> rets = r; \
    *ref = ExternalReference::wasm_##function##_##type; \
    if(params.size() != sig->parameter_count() || rets.size() != sig->return_count()) { \
      return false; \
    } \
    for(size_t i=0; i < params.size(); i++) { \
      if(params[i] != sig->GetParam(i)) { \
        return false; \
      } \
    } \
    for(size_t i=0; i < rets.size(); i++) { \
      if(rets[i] != sig->GetReturn(i)) { \
        return false; \
      } \
    } \
    return true; \
  }
FOREACH_NATIVE_FUNCTION_TEMPLATE(FIND_NATIVE_FUNCTION, ARGS_TO_VECTOR, ARGS_TO_VECTOR)
#undef FIND_NATIVE_FUNCTION
#undef ARGS_TO_VECTOR
  return false;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
