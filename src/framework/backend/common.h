#ifndef HELLOVK_FRAMEWORK_BACKEND_COMMON_H
#define HELLOVK_FRAMEWORK_BACKEND_COMMON_H

/* -------------------------------------------------------------------------- */

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
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

#include "volk.h"

#include "framework/lina.h"
using namespace lina::aliases;

#include "framework/utils.h"

/* -------------------------------------------------------------------------- */

#ifdef NDEBUG
# define CHECK_VK(res)  res
#else
# define CHECK_VK(res)  utils::CheckVKResult(res, __FILE__, __LINE__, true)
#endif

/* -------------------------------------------------------------------------- */

#endif
