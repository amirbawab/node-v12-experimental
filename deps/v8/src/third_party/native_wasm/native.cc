#include "src/wasm/wasm-sable-external-refs.h"
#include "src/third_party/native_wasm/system.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace native {

void init_native() {
  register_native_function("i32.print_s", nf_print_stack<int32_t>, {kWasmI32}, {});
  register_native_function("i64.print_s", nf_print_stack<int64_t>, {kWasmI64}, {});
  register_native_function("f32.print_s", nf_print_stack<float>, {kWasmF32}, {});
  register_native_function("f64.print_s", nf_print_stack<double>, {kWasmF64}, {});

  register_native_function("i64.time_ms", nf_time_ms, {}, {kWasmI64});

  register_native_function("i32.print_m", nf_print_mem<int32_t>, {kWasmI32, kWasmI32}, {});
  register_native_function("i64.print_m", nf_print_mem<int64_t>, {kWasmI32, kWasmI32}, {});
  register_native_function("f32.print_m", nf_print_mem<float>, {kWasmI32, kWasmI32}, {});
  register_native_function("f64.print_m", nf_print_mem<double>, {kWasmI32, kWasmI32}, {});
}

}
}
}
}
