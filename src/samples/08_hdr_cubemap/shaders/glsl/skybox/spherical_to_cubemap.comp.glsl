#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

// ----------------------------------------------------------------------------
//
// Create a cubemap texture from a spherical image.
//
// The shader expects a grid of the resolution of the cubemap in XY, and the
// number of faces (6) on Z.
//
// ref : http://paulbourke.net/panorama/cubemaps/#3
//
// ----------------------------------------------------------------------------

#include "../skybox_interop.h" //
#include "../shared.glsl"

// ----------------------------------------------------------------------------

layout (set = 0, binding = kDescriptorSetBinding_Sampler)
uniform sampler2D uSphericalTex;

layout(rgba16f, binding = kDescriptorSetBinding_StorageImage)
writeonly
uniform imageCube CubeMap;

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

// ----------------------------------------------------------------------------

vec2 cubemap_to_spherical_coords(in vec3 v) {
  const vec2 kInvPi = vec2(0.15915494, 0.318309886);
  const vec2 uv = vec2(atan(-v.z, v.x), asin(-v.y));
  return fma(uv, kInvPi, vec2(0.5));
}

// ----------------------------------------------------------------------------

layout(
  local_size_x = kCompute_SphericalTransform_kernelSize_x,
  local_size_y = kCompute_SphericalTransform_kernelSize_y,
  local_size_z = 1
) in;

void main()
{
  const ivec3 coords = ivec3(gl_GlobalInvocationID.xyz);

  const int resolution = int(pushConstant.mapResolution);

  // Check boundaries.
  if (!all(lessThan(coords, ivec3(resolution, resolution, 6)))) {
    return;
  }

  vec3 view = view_from_coords(coords, resolution);

  // Spherical (equirectangular) map sample.
  const vec2 spherical_coords = cubemap_to_spherical_coords(view);
  const vec4 data = texture( uSphericalTex, spherical_coords).rgba;

  imageStore(CubeMap, coords, data);
}

// ----------------------------------------------------------------------------
