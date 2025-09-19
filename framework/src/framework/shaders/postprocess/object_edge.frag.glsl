#version 460

// ----------------------------------------------------------------------------

layout(set = 0, binding = 0) uniform sampler2D inChannels[];

layout(location = 0) in vec2 vTexCoord;

layout(location = 0) out vec4 fragColor;

// ----------------------------------------------------------------------------

#define EDGE_LOWER                0
#define EDGE_GREATER              1
#define EDGE_DIFFERENT_WEIGHTED   2
#define EDGE_DIFFERENT            3

// ----------------------------------------------------------------------------

layout(push_constant) uniform params_ {
  int mode;
};

vec4 calculateRegionBorder() {
  ivec2 size       = textureSize(inChannels[0], 0);
  ivec2 texelCoord = ivec2(vec2(size) * vTexCoord.st);

  int A = floatBitsToInt(texelFetchOffset(inChannels[0], texelCoord, 0, ivec2(-1.0, +1.0)).w);
  int B = floatBitsToInt(texelFetchOffset(inChannels[0], texelCoord, 0, ivec2(+0.0, +1.0)).w);
  int C = floatBitsToInt(texelFetchOffset(inChannels[0], texelCoord, 0, ivec2(+1.0, +1.0)).w);
  int D = floatBitsToInt(texelFetchOffset(inChannels[0], texelCoord, 0, ivec2(-1.0, +0.0)).w);
  int X = floatBitsToInt(texelFetchOffset(inChannels[0], texelCoord, 0, ivec2(+0.0, +0.0)).w);
  int E = floatBitsToInt(texelFetchOffset(inChannels[0], texelCoord, 0, ivec2(+1.0, +0.0)).w);
  int F = floatBitsToInt(texelFetchOffset(inChannels[0], texelCoord, 0, ivec2(-1.0, -1.0)).w);
  int G = floatBitsToInt(texelFetchOffset(inChannels[0], texelCoord, 0, ivec2(+0.0, -1.0)).w);
  int H = floatBitsToInt(texelFetchOffset(inChannels[0], texelCoord, 0, ivec2(+1.0, -1.0)).w);

  switch(mode)
  {
    case EDGE_LOWER:
      if (X < A || X < B || X < C || X < D || X < E || X < F || X < G || X < H) {
        return vec4(1);
      }
    break;

    case EDGE_GREATER:
      if (X > A || X > B || X > C || X > D || X > E || X > F || X > G || X > H) {
        return vec4(1);
      }
    break;

    case EDGE_DIFFERENT_WEIGHTED:
      return vec4((int(X != A) + int(X != C) + int(X != F) + int(X != H)) * (1. / 6.)
                + (int(X != B) + int(X != D) + int(X != E) + int(X != G)) * (1. / 3.));

    case EDGE_DIFFERENT:
      if (X != A || X != B || X != C || X != D || X != E || X != F || X != G || X != H) {
        return vec4(1);
      }
    break;
  }

  return vec4(0);
}

// ----------------------------------------------------------------------------

void main() {
  fragColor = calculateRegionBorder();
}

// ----------------------------------------------------------------------------
