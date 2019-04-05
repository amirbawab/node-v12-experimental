#include "src/wasm/wasm-sable-external-refs.h"
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

/************************
 * Matrix Multiplication
 ************************/
template <class T>
void call_matrix_multiplication(byte* memByte, byte* dataByte) {
  int32_t mat1Offset = ReadValueAndAdvance<int32_t>(&dataByte);
  int32_t mat2Offset = ReadValueAndAdvance<int32_t>(&dataByte);
  int32_t resOffset = ReadValueAndAdvance<int32_t>(&dataByte);
  int32_t m = ReadValueAndAdvance<int32_t>(&dataByte);
  int32_t n = ReadValueAndAdvance<int32_t>(&dataByte);
  int32_t p = ReadValueAndAdvance<int32_t>(&dataByte);

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
  int32_t offset = ReadValueAndAdvance<int32_t>(&dataByte);
  int32_t size = ReadValueAndAdvance<int32_t>(&dataByte);
  for(int i=0; i < size; ++i) {
    std::cout << *(reinterpret_cast<T*>(memByte + offset + i * sizeof(T))) << " ";
  }
  std::cout << std::endl;
}

/*********************
 * Defines
 *********************/
#define ___ void

#define FOREACH_NON_TEMPLATE_FUNCTION(V) \
    V(print_help, "Print help message", "", "", ___, 0)

#define FOREACH_TEMPLATE_FUNCTION(V) \
  V(matrix_multiplication, "Matrix multiplication for i32", "mat1:i32 mat2:i32 res:i32 m:i32 n:i32 p:i32", "", int32_t, 1) \
  V(matrix_multiplication, "Matrix multiplication for i64", "mat1:i32 mat2:i32 res:i32 m:i32 n:i32 p:i32", "", int64_t, 2) \
  V(matrix_multiplication, "Matrix multiplication for f32", "mat1:i32 mat2:i32 res:i32 m:i32 n:i32 p:i32", "", float, 3) \
  V(matrix_multiplication, "Matrix multiplication for f64", "mat1:i32 mat2:i32 res:i32 m:i32 n:i32 p:i32", "", double, 4) \
  V(print_mem, "Print linear memory as i32", "offset:i32 size:i32", "", int32_t, 5) \
  V(print_mem, "Print linear memory as i64", "offset:i32 size:i32", "", int64_t, 6) \
  V(print_mem, "Print linear memory as f32", "offset:i32 size:i32", "", float, 7) \
  V(print_mem, "Print linear memory as f65", "offset:i32 size:i32", "", double, 8)

#define FOR_ALL_FUNCTIONS(V) \
  FOREACH_NON_TEMPLATE_FUNCTION(V) \
  FOREACH_TEMPLATE_FUNCTION(V)

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
      break;
  FOREACH_TEMPLATE_FUNCTION(SWITCH_CALL_SABLE_FUNCTION)
#undef SWITCH_CALL_SABLE_FUNCTION

#define SWITCH_CALL_SABLE_FUNCTION(function, description, params, rets, type, id) \
    case id: \
      call_##function(memByte, dataByte); \
      break;
    FOREACH_NON_TEMPLATE_FUNCTION(SWITCH_CALL_SABLE_FUNCTION)
#undef SWITCH_CALL_SABLE_FUNCTION
      default:
      return 0;
  }
  return 1;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
