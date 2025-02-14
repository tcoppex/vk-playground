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

/* -------------------------------------------------------------------------- */

namespace utils {

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

char* ReadBinaryFile(const char* filename, size_t* filesize);

std::string ExtractBasename(std::string_view const& path);

size_t AlignTo(size_t const byteLength, size_t const byteAlignment);

size_t AlignTo256(size_t const byteLength);

} // namespace "utils"

/* -------------------------------------------------------------------------- */

#endif