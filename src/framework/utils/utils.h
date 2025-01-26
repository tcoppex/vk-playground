#ifndef HELLOVK_FRAMEWORK_UTILS_H
#define HELLOVK_FRAMEWORK_UTILS_H

/* -------------------------------------------------------------------------- */
//
//    utils.h
//
//  A bunch of utility functions, not yet organized to be elsewhere.
//
/* -------------------------------------------------------------------------- */

#include <tuple>

#include "framework/common.h"
#include "framework/backend/types.h"

/* -------------------------------------------------------------------------- */

namespace utils {


char* ReadBinaryFile(const char* filename, size_t* filesize);

size_t AlignTo(size_t const byteLength, size_t const byteAlignment);

size_t AlignTo256(size_t const byteLength);


} // namespace "utils"

/* -------------------------------------------------------------------------- */

#endif