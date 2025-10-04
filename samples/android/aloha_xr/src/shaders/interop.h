#ifndef SHADERS_INTEROP_H_
#define SHADERS_INTEROP_H_

// ---------------------------------------------------------------------------

struct UniformCameraData {
  mat4 projectionMatrix;
  mat4 viewMatrix;
};

struct PushConstant {
  mat4 modelMatrix;
};

// ---------------------------------------------------------------------------

#endif