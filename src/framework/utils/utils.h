#ifndef UTILS_UTILS_H
#define UTILS_UTILS_H

/* -------------------------------------------------------------------------- */
//
//    utils.h
//
//  A bunch of utility functions, not yet organized to be elsewhere.
//
/* -------------------------------------------------------------------------- */

#include <cstdint>
#include <cstdlib>

#include <string_view>
#include <vector>
#include <future>
#include <functional>

/* -------------------------------------------------------------------------- */

namespace utils {

// --- structs ---

struct FileReader {
  static
  bool Read(std::string_view filename, std::vector<uint8_t>& out);

  bool read(std::string_view filename) {
    return FileReader::Read(filename, buffer);
  }

  void clear() {
    buffer.clear();
  }

  std::vector<uint8_t> buffer;
};

// --- constexpr functions ---

constexpr uint32_t Log2_u32(uint32_t x) {
  uint32_t result = 0;
  while (x > 1) {
    x >>= 1;
    ++result;
  }
  return result;
}

// --- template functions ---

size_t HashCombine(size_t seed, auto const& value) {
  return seed ^ (std::hash<std::decay_t<decltype(value)>>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

template <typename T>
inline auto RunTaskGeneric = [](auto&& fn) -> std::future<T> {
  return std::async(std::launch::async, std::forward<decltype(fn)>(fn));
};

// --- functions ---

char* ReadBinaryFile(const char* filename, size_t* filesize);

std::string ExtractBasename(std::string_view filename);

std::string ExtractExtension(std::string_view filename);

size_t AlignTo(size_t const byteLength, size_t const byteAlignment);

size_t AlignTo256(size_t const byteLength);

// ----------------------------------------------------------------------------

} // namespace "utils"

/* -------------------------------------------------------------------------- */

#endif