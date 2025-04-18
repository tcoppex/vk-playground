#ifndef SHADERS_SHARED_LINEARIZE_DEPTH_GLSL_
#define SHADERS_SHARED_LINEARIZE_DEPTH_GLSL_

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
  return linearizeDepth_Vulkan(z, zNear, zFar);
}

// params:
//  X : z_near
//  Y : z_far
//  Z : z_far / (z_far - z_near)
//  W : - z_near * (params.Z)
float linearizeDepth(vec4 params, float z) {
  return linearizeDepthFast_Vulkan(z, params.z, params.w);
}

// ----------------------------------------------------------------------------

float normalizedLinearDepth(in float z, in float zNear, in float zFar) {
  return (z - zNear) / (zFar - zNear);
}

float normalizedPerspectiveDepth(in float z, in float inv_zNear, in float inv_zFar) {
  return abs(normalizedLinearDepth(1.0 / z, inv_zNear, inv_zFar));
}

// ----------------------------------------------------------------------------

#endif // SHADERS_SHARED_LINEARIZE_DEPTH_GLSL_
