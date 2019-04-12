#include "src/wasm/wasm-sable-external-refs.h"
#include "src/signature.h"
#include <iostream>
#include <map>

namespace v8 {
namespace internal {
namespace wasm {
namespace native {

struct Function {
  std::string name;
  native_t func;
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

template <> int32_t Argument::Get<int32_t>() const {
  CHECK_EQ(type_, kWasmI32);
  return ReadUnalignedValue<int32_t>(val_add_);
}

template <> int64_t Argument::Get<int64_t>() const {
  CHECK_EQ(type_, kWasmI64);
  return ReadUnalignedValue<int64_t>(val_add_);
}

template <> float Argument::Get<float>() const {
  CHECK_EQ(type_, kWasmF32);
  return ReadUnalignedValue<float>(val_add_);
}

template <> double Argument::Get<double>() const {
  CHECK_EQ(type_, kWasmF64);
  return ReadUnalignedValue<double>(val_add_);
}

template <> void Return::Set<int32_t>(int32_t val) {
  CHECK_EQ(type_, kWasmI32);
  WriteUnalignedValue<int32_t>(ret_add_, val);
}

template <> void Return::Set<int64_t>(int64_t val) {
  CHECK_EQ(type_, kWasmI64);
  WriteUnalignedValue<int64_t>(ret_add_, val);
}

template <> void Return::Set<float>(float val) {
  CHECK_EQ(type_, kWasmF32);
  WriteUnalignedValue<float>(ret_add_, val);
}

template <> void Return::Set<double>(double val) {
  CHECK_EQ(type_, kWasmF64);
  WriteUnalignedValue<double>(ret_add_, val);
}

/*******************
 * Native functions
 ******************/

//template <class T>
//void f_matrix_multiplication(byte* memByte, byte* dataByte) {
//  I32 mat1Offset = ReadValueAndAdvance<I32>(&dataByte);
//  I32 mat2Offset = ReadValueAndAdvance<I32>(&dataByte);
//  I32 resOffset = ReadValueAndAdvance<I32>(&dataByte);
//  I32 m = ReadValueAndAdvance<I32>(&dataByte);
//  I32 n = ReadValueAndAdvance<I32>(&dataByte);
//  I32 p = ReadValueAndAdvance<I32>(&dataByte);
//
//  T* mat1 = reinterpret_cast<T*>(memByte + mat1Offset);
//  T* mat2 = reinterpret_cast<T*>(memByte + mat2Offset);
//  T* res = reinterpret_cast<T*>(memByte + resOffset);
//
//  for(int r=0; r < m; ++r) {
//    for(int c=0; c < p; ++c) {
//      T resCell = 0;
//      for(int cr=0; cr < n; ++cr) {
//        resCell += *(mat1 + r * n + cr) * *(mat2 + cr * p + c);
//      }
//      WriteValue<T>(res + r * p + c, resCell);
//    }
//  }
//}
//
//
//void f_exp(byte* memByte, byte* dataByte) {
//  F64 x = ReadValueAndAdvance<F64>(&dataByte);
//  WriteValue<F64>(reinterpret_cast<F64*>(dataByte), base::ieee754::exp(x));
//}
//

/*******************************
 * External reference functions
 *******************************/
std::vector<Function> g_func_vec;
std::map<std::string, size_t> g_func_map;

bool register_native_function(std::string name, native_t func,
                              std::vector<ValueType> args, std::vector<ValueType> rets) {
  if(g_func_map.find(name) != g_func_map.end()) {
    return false;
  }
  g_func_map[name] = g_func_vec.size();
  g_func_vec.emplace_back(Function{
    std::move(name), std::move(func), std::move(args), std::move(rets)
  });
  return true;
}

bool find_native_function(const char* find_name, FunctionSig* sig, int *out_index) {
  int funcId;
  if(g_func_map.find(find_name) == g_func_map.end() || !g_func_vec[funcId = g_func_map[find_name]].sig_match(sig)) {
    return false;
  }
  *out_index = funcId;
  return true;
}

int native_function_gateway(int funcId, Address mem, Address data) {
  CHECK_GE(funcId, 0);
  CHECK_LT(funcId, g_func_vec.size());
  const Function &func = g_func_vec[funcId];
  arg_vec_t args;
  ret_vec_t rets;
  byte* byteData = reinterpret_cast<byte*>(data);
  for(size_t i=0; i < func.params.size(); ++i) {
    args.emplace_back(Argument{
        reinterpret_cast<Address>(byteData), func.params[i]
    });
    byteData += ValueTypes::ElementSizeInBytes(func.params[i]);
  }
  for(size_t i=0; i < func.rets.size(); ++i) {
    rets.emplace_back(Return{
        reinterpret_cast<Address>(byteData), func.rets[i]
    });
    byteData += ValueTypes::ElementSizeInBytes(func.rets[i]);
  }
  (*func.func)(args, rets, Memory{mem, mem /*TODO*/});
  return 0;
}

}  // namespace native
}  // namespace wasm
}  // namespace internal
}  // namespace v8
