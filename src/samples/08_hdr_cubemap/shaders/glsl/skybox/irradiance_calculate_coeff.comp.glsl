#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

// ----------------------------------------------------------------------------
//
// Calculate the irradiance spherical harmonics coefficients of an environment map.
//
// cf. Ravi Ramamoorthi & Pat Hanrahan's technique as described in their 2001 paper
//    "An Efficient Representation for Irradiance Environment Maps"
//
// ----------------------------------------------------------------------------

#include <shared/maths.glsl>

#include "../skybox_interop.h"
#include "../shared.glsl"

// ----------------------------------------------------------------------------

layout (set = 0, binding = kDescriptorSetBinding_Sampler)
uniform samplerCube inDiffuseEnvmap;

layout(scalar, binding = kDescriptorSetBinding_IrradianceSHCoeff_StorageBuffer)
writeonly buffer SHCoeffBuffer_ {
  SHCoeff OutSHCoeffs[];
};

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

float Y0(in vec3 n)  { return 0.282095f; }                               /* L_00 */
float Y1(in vec3 n)  { return 0.488603f * n.y; }                         /* L_1-1 */
float Y2(in vec3 n)  { return 0.488603f * n.z; }                         /* L_10 */
float Y3(in vec3 n)  { return 0.488603f * n.x; }                         /* L_11 */
float Y4(in vec3 n)  { return 1.092548f * n.x*n.y; }                     /* L_2-2 */
float Y5(in vec3 n)  { return 1.092548f * n.y*n.z; }                     /* L_2-1 */
float Y6(in vec3 n)  { return 0.315392f * (3.0f*n.z*n.z - 1.0f); }       /* L_20 */
float Y7(in vec3 n)  { return 1.092548f * n.x*n.z; }                     /* L_21 */
float Y8(in vec3 n)  { return 0.546274f * (n.x*n.x - n.y*n.y); }


vec3 get_direction(float u, float v, int faceId) {
  const vec3 texeldirs[6] = {
    vec3( +1.0f, -v, -u),  // +X
    vec3( -1.0f, -v, +u),  // -X
    vec3( +u, +1.0f, +v),  // +Y
    vec3( +u, -1.0f, -v),  // -Y
    vec3( +u, -v, +1.0f),  // +Z
    vec3( -u, -v, -1.0f),  // -Z
  };
  return normalize( texeldirs[faceId] );
}

float area(float x, float y) {
  return atan(x * y, sqrt(x * x + y * y + 1.0f));
}

float calculate_solid_angle(float u, float v, float texelSize) {
  float x0 = u - texelSize;
  float y0 = v - texelSize;
  float x1 = u + texelSize;
  float y1 = v + texelSize;
  float solidAngle = (area(x0,y0) + area(x1,y1))
                   - (area(x0,y1) + area(x1,y0))
                   ;
  return solidAngle;
}

// ----------------------------------------------------------------------------

layout(
  local_size_x = kCompute_IrradianceSHCoeff_kernelSize_x,
  local_size_y = kCompute_IrradianceSHCoeff_kernelSize_y,
  local_size_z = 1
) in;

void main() {
  const ivec3 coords = ivec3(gl_GlobalInvocationID);
  const int resolution = int(pushConstant.mapResolution);

  // Check boundaries.
  if (!all(lessThan(coords, ivec3(resolution, resolution, 1)))) {
    return;
  }

  const ivec3 gridDim   = ivec3(gl_NumWorkGroups);
  const ivec3 blockDim  = ivec3(gl_WorkGroupSize);

  const int gid = coords.x
                + coords.y * gridDim.x * blockDim.x
                + coords.z * gridDim.x * blockDim.x * gridDim.y * blockDim.y
                ;

  SHCoeff shc;

  // --------------

  // vec3 view = view_from_coords(coords, resolution);
  // // World-space basis from the view direction.
  // const mat3 basis_ws = basis_from_view(view);

  // TODO : each pixel must loop each face for its contribution


  float texelSize = 1.0f / float(resolution);
  float u = 2.0f * (float(coords.x) + 0.5f) * texelSize - 1.0f;
  float v = 2.0f * (float(coords.y) + 0.5f) * texelSize - 1.0f;

  const float solidAngle = calculate_solid_angle(u, v, texelSize);

  for (int i = 0; i < 9; ++i) {
    shc.data[i] = vec4(0.0);
  }

  for (int faceId = 0; faceId < 6; ++faceId)
  {
    const vec3 direction = get_direction(u, v, faceId);
    // vec3 direction = view_from_coords(ivec3(coords.xy, faceId), resolution);

    const vec3 radiance = texture(inDiffuseEnvmap, direction).rgb;

    const vec3 lambda = radiance * solidAngle;

    shc.data[0].xyz += lambda * Y0( direction );
    shc.data[1].xyz += lambda * Y1( direction );
    shc.data[2].xyz += lambda * Y2( direction );
    shc.data[3].xyz += lambda * Y3( direction );
    shc.data[4].xyz += lambda * Y4( direction );
    shc.data[5].xyz += lambda * Y5( direction );
    shc.data[6].xyz += lambda * Y6( direction );
    shc.data[7].xyz += lambda * Y7( direction );
    shc.data[8].xyz += lambda * Y8( direction );

    shc.data[0].w += solidAngle;
  }

  // --------------

  // [debug]
  // for (int i=0; i<9; ++i) {
  //   shc.data[i] = vec4(1.0);
  // }

  // --------------

  OutSHCoeffs[gid] = shc;
}

// ----------------------------------------------------------------------------
