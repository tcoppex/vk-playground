#include "framework/utils/utils.h"

#include <cstdio>
#include <iostream>
#include <fstream>

/* -------------------------------------------------------------------------- */

namespace utils {

bool FileReader::Read(std::string_view filename, std::vector<uint8_t>& out) {
  constexpr std::size_t kMaxReadFileSize{1 << 27};

  std::ifstream file(filename.data(), std::ios::binary | std::ios::ate);
  if (!file) {
    std::cerr << "[WARNING] \"" << filename << "\" not found." << std::endl;
    return false;
  }

  std::size_t filesize = file.tellg();
  if (filesize > kMaxReadFileSize) {
    std::cerr << "[WARNING] Read size exceed for \"" << filename << "\"." << std::endl;
    return false;
  } else if (filesize == static_cast<std::size_t>(-1)) {
    std::cerr << "[ERROR] Unable to determine file size for \"" << filename << "\"." << std::endl;
    return false;
  }

  out.clear();
  out.resize(filesize);

  file.seekg(0, std::ios::beg);
  if (!file.read(reinterpret_cast<char*>(out.data()), filesize)) {
    std::cerr << "[ERROR] Failed to read file \"" << filename << "\"." << std::endl;
    return false;
  }

  return true;
}

// ----------------------------------------------------------------------------

char* ReadBinaryFile(const char *filename, size_t *filesize) {
  FILE *fd = fopen(filename, "rb");
  if (!fd) {
    return nullptr;
  }

  fseek(fd, 0L, SEEK_END);
  *filesize = ftell(fd);
  fseek(fd, 0L, SEEK_SET);

  char* shader_binary = new char[*filesize];
  size_t const ret = fread(shader_binary, *filesize, 1u, fd);
  fclose(fd);

  if (ret != 1u) {
    delete [] shader_binary;
    shader_binary = nullptr;
  }

  return shader_binary;
}

// ----------------------------------------------------------------------------

std::string ExtractBasename(std::string_view const& path) {
  size_t start = path.find_last_of("/\\");
  start = (start == std::string_view::npos) ? 0 : start + 1;
  size_t end = path.find_last_of('.', path.length());
  end = (end == std::string_view::npos || end < start) ? path.length() : end;
  return std::string(path.substr(start, end - start));
}

// ----------------------------------------------------------------------------

size_t AlignTo(size_t const byteLength, size_t const byteAlignment) {
  return (byteLength + byteAlignment - 1) / byteAlignment * byteAlignment;
}

size_t AlignTo256(size_t const byteLength) {
  return AlignTo(byteLength, 256);
}

} // namespace "utils"

/* -------------------------------------------------------------------------- */