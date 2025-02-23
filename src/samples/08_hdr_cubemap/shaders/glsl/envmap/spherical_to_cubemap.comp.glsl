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
// Refs: http://paulbourke.net/panorama/cubemaps/
//
// ----------------------------------------------------------------------------

#include <shared/maths.glsl>

#include "../envmap_interop.h"

// ----------------------------------------------------------------------------

layout (set = 0, binding = kDescriptorSetBinding_Sampler)
uniform sampler2D uSphericalTex;

layout(rgba16f, binding = kDescriptorSetBinding_StorageImage)
writeonly uniform imageCube CubeMap;

layout(push_constant, scalar) uniform PushConstant_ {
  PushConstant pushConstant;
};

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

  if (!all(lessThan(coords, ivec3(resolution, resolution, 6)))) {
    return;
  }

  const vec3 view = cubemap_view_direction(coords, resolution);
  const vec2 spherical_coords = cubemap_to_spherical_coords(view);
  const vec4 data = texture( uSphericalTex, spherical_coords);

  imageStore(CubeMap, coords, data);
}

// ----------------------------------------------------------------------------
