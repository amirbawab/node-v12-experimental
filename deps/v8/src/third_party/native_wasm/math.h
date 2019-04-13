#ifndef WASM_NATIVE_WASM_MATH_H
#define WASM_NATIVE_WASM_MATH_H

#include "src/wasm/wasm-sable-external-refs.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace native {

// matrix multiplication
template <class T>
void nf_matrix_multiplication(std::vector<native::Argument> args, std::vector<native::Return> rets,
                              native::Memory memory);

// exp(x)
void nf_exp(std::vector<native::Argument> args, std::vector<native::Return> rets, native::Memory memory);

} // namespace native
} // namespace wasm
} // namespace internal
} // namespace v8

#endif
