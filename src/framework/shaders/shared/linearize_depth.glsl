#ifndef SHADERS_SHARED_LINEARIZE_DEPTH_GLSL_
#define SHADERS_SHARED_LINEARIZE_DEPTH_GLSL_

// ----------------------------------------------------------------------------

#ifndef USE_OPENGL
# define USE_OPENGL  0
#endif

// ----------------------------------------------------------------------------
// for NDC in [-1, 1] (OpenGL).

float linearizeDepth_OpenGL(float z, float zNear, float zFar) {
  return (2 * zNear) / (zNear + zFar + z * (zNear - zFar));
}

// ----------------------------------------------------------------------------
// for NDC in [0, 1] (Vulkan / DirectX).

float linearizeDepth_Vulkan(float z, float zNear, float zFar) {
  return (zNear * zFar) / (zFar + z * (zNear - zFar));
}

float linearizeDepthFast_Vulkan(float z, float A, float B) {
  return B / (z - A);
}

// ----------------------------------------------------------------------------

float linearizeDepth(float z, float zNear, float zFar) {
#if USE_OPENGL
  return linearizeDepth_OpenGL(z, zNear, zFar);
#else
  return linearizeDepth_Vulkan(z, zNear, zFar);
#endif
}

// params:
//  X : z_near
//  Y : z_far
//  Z : A = z_far / (z_far - z_near)
//  W : B = - z_near * A
float linearizeDepth(vec4 params, float z) {
#if USE_OPENGL
  return linearizeDepth_OpenGL(z, params.x, params.y);
#else
  return linearizeDepthFast_Vulkan(z, params.z, params.w);
#endif
}

// ----------------------------------------------------------------------------

float normalizedLinearDepth(in float z, in float zNear, in float zFar) {
  return (z - zNear) / (zFar - zNear);
}

// float normalizedPerspectiveDepth(in float z, in float zNear, in float zFar) {
//   return abs((1. / z - 1. / zNear) / ((1. / zFar) - (1. / zNear)));
// }

float normalizedPerspectiveDepth(in float z, in float inv_zNear, in float inv_zFar) {
  return abs(normalizedLinearDepth(1.0 / z, inv_zNear, inv_zFar));
}

// ----------------------------------------------------------------------------

#endif // SHADERS_SHARED_LINEARIZE_DEPTH_GLSL_
