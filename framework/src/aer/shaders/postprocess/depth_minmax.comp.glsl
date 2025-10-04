#version 460

// ----------------------------------------------------------------------------

layout(set = 0, binding = 0, rgba8)
uniform readonly image2D inImages[];

layout(binding = 1) buffer outValue {
  uint minmax[2];
};

// ----------------------------------------------------------------------------

layout(
  local_size_x = 32,
  local_size_y = 32
) in;

void main() {
  ivec2 size = imageSize(inImages[0]);
  ivec2 pt = ivec2(gl_GlobalInvocationID.xy);

  if ((pt.x >= size.x) || (pt.y >= size.y)) {
    return;
  }

  float fdepth = imageLoad(inImages[0], pt).z;

  if (fdepth > 0) {
    uint idepth = floatBitsToInt(fdepth);
    atomicMin(minmax[0], idepth);
    atomicMax(minmax[1], idepth);
  }
}

// ----------------------------------------------------------------------------
