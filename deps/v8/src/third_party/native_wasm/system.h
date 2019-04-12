#ifndef WASM_NATIVE_WASM_SYSTEM_H
#define WASM_NATIVE_WASM_SYSTEM_H

#include "src/wasm/wasm-sable-external-refs.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace native {

// print stack content
template <class T>
void nf_print_stack(std::vector<native::Argument> args, std::vector<native::Return> rets, native::Memory memory);

// get time in ms
void nf_time_ms(std::vector<native::Argument> args, std::vector<native::Return> rets, native::Memory memory);

// print linear memory
template<class T>
void nf_print_mem(std::vector<native::Argument> args, std::vector<native::Return> rets, native::Memory memory);

} // namespace native
} // namespace wasm
} // namespace internal
} // namespace v8

#endif
