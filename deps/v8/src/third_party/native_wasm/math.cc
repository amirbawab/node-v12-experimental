#include "src/third_party/native_wasm/math.h"
#include "src/base/ieee754.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace native {

template <class T>
void nf_matrix_multiplication(std::vector<native::Argument> args, std::vector<native::Return> rets,
                              native::Memory memory) {
  DCHECK_EQ(6, args.size());
  DCHECK_EQ(0, rets.size());

  int32_t mat1Offset = args[0].Get<int32_t>();
  int32_t mat2Offset = args[1].Get<int32_t>();
  int32_t resOffset = args[2].Get<int32_t>();
  int32_t m = args[3].Get<int32_t>();
  int32_t n = args[4].Get<int32_t>();
  int32_t p = args[5].Get<int32_t>();

  if(!memory.SafeAccess<T>(mat1Offset) || !memory.SafeAccess<T>(resOffset + m * p)) {
    // TODO trap
    return;
  }

  for(int r=0; r < m; ++r) {
    for(int c=0; c < p; ++c) {
      T resCell = 0;
      for(int cr=0; cr < n; ++cr) {
        resCell += memory.ReadUnsafe<T>(mat1Offset + (r * n + cr) * sizeof(T)) *
            memory.ReadUnsafe<T>(mat2Offset + (cr * p + c) * sizeof(T));
      }
      memory.WriteUnsafe<T>(resOffset + (r * p + c) * sizeof(T), resCell);
    }
  }
}

// explicit instantiation of nf_matrix_multiplication template
template void nf_matrix_multiplication<int32_t>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);
template void nf_matrix_multiplication<int64_t>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);
template void nf_matrix_multiplication<float>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);
template void nf_matrix_multiplication<double>(std::vector<native::Argument> args, std::vector<native::Return> rets,
    native::Memory memory);

void nf_exp(std::vector<native::Argument> args, std::vector<native::Return> rets, native::Memory memory) {
  DCHECK_EQ(1, args.size());
  DCHECK_EQ(1, rets.size());

  rets[0].Set<double>(base::ieee754::exp(args[0].Get<double>()));
}

}
}
}
}
