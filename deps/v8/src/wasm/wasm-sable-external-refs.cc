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
  auto mat1Offset = ReadValueAndAdvance<int32_t>(&dataByte);
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

/*******************
 * Native functions
 *******************/

#define FOREACH_NON_TEMPLATE_FUNCTION(V) \
    /*non-template functions*/

#define FOREACH_TEMPLATE_FUNCTION(V) \
  V(matrix_multiplication, int32_t, 0) \
  V(matrix_multiplication, int64_t, 1) \
  V(matrix_multiplication, float, 2) \
  V(matrix_multiplication, double, 3) \
  V(print_mem, int32_t, 4) \
  V(print_mem, int64_t, 5) \
  V(print_mem, float, 6) \
  V(print_mem, double, 7)

#define FOR_ALL_FUNCTIONS(V) \
  FOREACH_NON_TEMPLATE_FUNCTION(V) \
  FOREACH_TEMPLATE_FUNCTION(V)

int native_function_gateway(uint32_t functionId, Address mem, Address data) {
  // Reinterpret as byte addresses
  byte* dataByte = reinterpret_cast<byte*>(data);
  byte* memByte = reinterpret_cast<byte*>(mem);
  switch (functionId) {
#define SWITCH_CALL_SABLE_FUNCTION(function, type, id) \
    case id: \
      call_##function<type>(memByte, dataByte); \
      break;
  FOREACH_TEMPLATE_FUNCTION(SWITCH_CALL_SABLE_FUNCTION)
#undef SWITCH_CALL_SABLE_FUNCTION

#define SWITCH_CALL_SABLE_FUNCTION(function, id) \
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
