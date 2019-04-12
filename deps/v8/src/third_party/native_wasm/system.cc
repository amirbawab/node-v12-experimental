#include "src/third_party/native_wasm/system.h"
#include <chrono>
#include <iostream>

namespace v8 {
namespace internal {
namespace wasm {
namespace native {

template<class T>
void nf_print_mem(std::vector<native::Argument> args, std::vector<native::Return> rets, native::Memory memory) {
  DCHECK_EQ(2, args.size());
  DCHECK_EQ(0, rets.size());

  int32_t offset = args[0].Get<int32_t>();
  int32_t size = args[1].Get<int32_t>();
  for(int i=0; i < size; ++i) {
    std::cout << memory.Read<T>(offset) << " ";
  }
  std::cout << std::endl;
}

// explicit instantiation of nf_print_mem template
template void nf_print_mem<int32_t>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);
template void nf_print_mem<int64_t>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);
template void nf_print_mem<float>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);
template void nf_print_mem<double>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);

void nf_time_ms(std::vector<native::Argument> args, std::vector<native::Return> rets, native::Memory memory) {
  DCHECK_EQ(0, args.size());
  DCHECK_EQ(1, rets.size());
  rets[0].Set<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}

template <class T>
void nf_print_stack(std::vector<native::Argument> args, std::vector<native::Return> rets, native::Memory memory) {
  DCHECK_EQ(1, args.size());
  DCHECK_EQ(0, rets.size());
  std::cout << args[0].Get<T>() << std::endl;
}

// explicit instantiation of nf_print_stack template
template void nf_print_stack<int32_t>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);
template void nf_print_stack<int64_t>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);
template void nf_print_stack<float>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);
template void nf_print_stack<double>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);

}
}
}
}
