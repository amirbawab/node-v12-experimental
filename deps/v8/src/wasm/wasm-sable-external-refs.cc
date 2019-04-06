#include "src/wasm/wasm-sable-external-refs.h"
#include "src/base/ieee754.h"
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

#define FOREACH_NON_TEMPLATE_FUNCTION(V) \
    V(print_help, "Print help message", "", "", ___, 0) \
    V(exp, "Exponential function", "x:f64", "f64", ___, 9)

#define FOREACH_TEMPLATE_FUNCTION(V) \
  V(matrix_multiplication, "Matrix multiplication for i32", "mat1:i32 mat2:i32 res:i32 m:i32 n:i32 p:i32", "", I32, 1) \
  V(matrix_multiplication, "Matrix multiplication for i64", "mat1:i32 mat2:i32 res:i32 m:i32 n:i32 p:i32", "", I64, 2) \
  V(matrix_multiplication, "Matrix multiplication for f32", "mat1:i32 mat2:i32 res:i32 m:i32 n:i32 p:i32", "", F32, 3) \
  V(matrix_multiplication, "Matrix multiplication for f64", "mat1:i32 mat2:i32 res:i32 m:i32 n:i32 p:i32", "", F64, 4) \
  V(print_mem, "Print linear memory as i32", "offset:i32 size:i32", "", I32, 5) \
  V(print_mem, "Print linear memory as i64", "offset:i32 size:i32", "", I64, 6) \
  V(print_mem, "Print linear memory as f32", "offset:i32 size:i32", "", F32, 7) \
  V(print_mem, "Print linear memory as f65", "offset:i32 size:i32", "", F64, 8)

#define FOR_ALL_FUNCTIONS(V) \
  FOREACH_NON_TEMPLATE_FUNCTION(V) \
  FOREACH_TEMPLATE_FUNCTION(V)

/************************
 * Matrix Multiplication
 ************************/
template <class T>
void call_matrix_multiplication(byte* memByte, byte* dataByte) {
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

/********
 * Print
 ********/
template<class T>
void call_print_mem(byte* memByte, byte* dataByte) {
  I32 offset = ReadValueAndAdvance<I32>(&dataByte);
  I32 size = ReadValueAndAdvance<I32>(&dataByte);
  for(int i=0; i < size; ++i) {
    std::cout << *(reinterpret_cast<T*>(memByte + offset + i * sizeof(T))) << " ";
  }
  std::cout << std::endl;
}

/*****************
 * Math functions
 *****************/
void call_exp(byte* memByte, byte* dataByte) {
  F64 x = ReadValueAndAdvance<F64>(&dataByte);
  WriteValue<F64>(reinterpret_cast<F64*>(dataByte), base::ieee754::exp(x));
}

/*********************
 * Print help message
 *********************/
void call_print_help(byte* memByte, byte* dataByte) {
  std::cout << "Native functions:" << std::endl;
#define PRINT_MESSAGE(function, description, params, rets, type, id) \
  std::cout << "  " << id << ": " << #function << " - " << description << std::endl; \
  if(strlen(params) > 0) { \
    std::cout << "        params: " << params << std::endl; \
  } \
  if(strlen(rets) > 0) { \
    std::cout << "        returns: " << rets << std::endl; \
  }
  FOR_ALL_FUNCTIONS(PRINT_MESSAGE)
#undef PRINT_MESSAGE
}

/*******************
 * Native functions
 *******************/

int native_function_gateway(uint32_t functionId, Address mem, Address data) {
  // Reinterpret as byte addresses
  byte* dataByte = reinterpret_cast<byte*>(data);
  byte* memByte = reinterpret_cast<byte*>(mem);
  switch (functionId) {
#define SWITCH_CALL_SABLE_FUNCTION(function, description, params, rets, type, id) \
    case id: \
      call_##function<type>(memByte, dataByte); \
      return 1;
  FOREACH_TEMPLATE_FUNCTION(SWITCH_CALL_SABLE_FUNCTION)
#undef SWITCH_CALL_SABLE_FUNCTION

#define SWITCH_CALL_SABLE_FUNCTION(function, description, params, rets, type, id) \
    case id: \
      call_##function(memByte, dataByte); \
      return 1;
    FOREACH_NON_TEMPLATE_FUNCTION(SWITCH_CALL_SABLE_FUNCTION)
#undef SWITCH_CALL_SABLE_FUNCTION
      default:
      return 0;
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
