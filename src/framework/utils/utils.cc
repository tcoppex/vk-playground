#include "framework/utils/utils.h"

/* -------------------------------------------------------------------------- */

namespace utils {

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

size_t AlignTo(size_t const byteLength, size_t const byteAlignment) {
  return (byteLength + byteAlignment - 1) / byteAlignment * byteAlignment;
}

size_t AlignTo256(size_t const byteLength) {
  return AlignTo(byteLength, 256);
}

} // namespace "utils"

/* -------------------------------------------------------------------------- */