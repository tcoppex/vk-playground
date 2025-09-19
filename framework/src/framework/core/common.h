#ifndef VKFRAMEWORK_CORE_COMMON_H_
#define VKFRAMEWORK_CORE_COMMON_H_

/* -------------------------------------------------------------------------- */

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <vector>
#include <span>
#include <concepts>
#include <memory>
#include <string_view>

#include "lina/lina.h"
using namespace lina::aliases;
using uint = uint32_t; //

#include "framework/core/logger.h"

/* -------------------------------------------------------------------------- */
/* -- c++ features -- */

template<typename T, typename U>
concept DerivedFrom = std::is_base_of_v<U, T>;

template<typename T>
concept SpanConvertible = requires(T t) { std::span(t); };

// ----------------------------------------------------------------------------
/* -- custom type -- */

// Holds an abstraction over the main argument list.
struct ArgList : public std::span<char*> {
  ArgList(int argc, char *argv[])
    : std::span<char*>{argv, static_cast<size_t>(argc)}
  {}

  ArgList()
    : ArgList(0, nullptr)
  {}
};

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

struct NullType {};

// ----------------------------------------------------------------------------
/* -- constants -- */

static constexpr uint32_t kInvalidIndexU32{ std::numeric_limits<uint32_t>::max() };

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_CORE_COMMON_H_
