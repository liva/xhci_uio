#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static inline void panic(const char *str) {
  fprintf(stderr, "%s\n", str);
  exit(-1);
}

template<class T>
bool IsFlagSet(T val, T flag) {
  return (val & flag) != 0;
}

template<class T>
bool IsFlagClear(T val, T flag) {
  return (val & flag) == 0;
}

template<class T>
T MaskValue(T val, int len, int offset) {
  return (val >> offset) & ((1 << len) - 1);
}

template<class V, class T>
T MaskValue(T val) {
  return (val >> V::kOffset) & ((1 << V::kLen) - 1);
}

template<class T, class Y>
T GenerateValue(Y val, int len, int offset) {
  assert(val <= (1 << len) - 1);
  return val << offset;
}

template<class V, class T, class Y>
T GenerateValue(Y val) {
  assert(val <= (1 << V::kLen) - 1);
  return val << V::kOffset;
}

template<class T>
constexpr T GenerateMask(int len, int offset) {
  return ((static_cast<T>(1) << len) - 1) << offset;
}

template<class V, class T>
constexpr T GenerateMask() {
  return ((static_cast<T>(1) << V::kLen) - 1) << V::kOffset;
}

enum class ReturnState {
  kSuccess,
  kErrUnknown,
  kErrNoHwResource,
  kErrNoSwResource,
};

#define RETURN_IF_ERR(x) do { ReturnState err = (x); if ((err) != ReturnState::kSuccess) { return err; }} while(0)
