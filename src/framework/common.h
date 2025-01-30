#ifndef HELLOVK_FRAMEWORK_COMMON_H
#define HELLOVK_FRAMEWORK_COMMON_H

/* -------------------------------------------------------------------------- */

#include <cassert>
#include <cmath>
#include <cstdio> //
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>
#include <span>
#include <vector>
#include <concepts>
#include <type_traits>
#include <iterator> // For std::back_inserter

// linear algebra.
#include "lina/lina.h"
using namespace lina::aliases;

// Utility functions.
#include "framework/utils/utils.h"

/* -------------------------------------------------------------------------- */

/* -- c++ features -- */

template<typename T>
concept SpanConvertible = requires(T t) { std::span(t); };

// Wrap an array to accept enum class as indexer.
// Original code from Daniel P. Wright.
template<typename T, typename Indexer>
class EnumArray : public std::array<T, static_cast<size_t>(Indexer::kCount)> {
  using super = std::array<T, static_cast<size_t>(Indexer::kCount)>;

 public:
  constexpr EnumArray(std::initializer_list<T> il) {
    assert( il.size() == super::size());
    std::copy(il.begin(), il.end(), super::begin());
  }

  T&       operator[](Indexer i)       { return super::at((size_t)i); }
  const T& operator[](Indexer i) const { return super::at((size_t)i); }

  EnumArray() : super() {}
  using super::operator[];
};

/* -------------------------------------------------------------------------- */

#endif // HELLOVK_FRAMEWORK_COMMON_H
