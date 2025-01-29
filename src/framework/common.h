#ifndef HELLOVK_FRAMEWORK_COMMON_H
#define HELLOVK_FRAMEWORK_COMMON_H

/* -------------------------------------------------------------------------- */

#include <cassert> //
#include <cmath> //
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

// (linear algebra)
#include "framework/lina.h"
using namespace lina::aliases;

// (miscs utility functions)
#include "framework/utils/utils.h"

/* -------------------------------------------------------------------------- */

// c++ features

template<typename T>
concept SpanConvertible = requires(T t) { std::span(t); };

/* -------------------------------------------------------------------------- */

#endif // HELLOVK_FRAMEWORK_COMMON_H
