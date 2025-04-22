#ifndef HELLO_VK_FRAMEWORK_SCENE_PRIVATE_GLTF_LOADER_H_
#define HELLO_VK_FRAMEWORK_SCENE_PRIVATE_GLTF_LOADER_H_

#include "framework/common.h"
#include "framework/scene/resources.h"
#include "framework/utils/cgltf_wrapper.h"

/* -------------------------------------------------------------------------- */

namespace internal::gltf_loader {

using PointerToIndexMap_t = std::unordered_map<void const*, uint32_t>;
using PointerToStringMap_t = std::unordered_map<void const*, std::string>;
using PointerToSamplerMap_t = std::unordered_map<void const*, VkSampler>;

/* -------------------------------------------------------------------------- */

void ExtractImages(
  cgltf_data const* data,
  PointerToIndexMap_t& image_indices,
  scene::ResourceBuffer<scene::ImageData>& images
);

void ExtractSamplers(
  cgltf_data const* data,
  SamplerPool& sampler_pool,
  PointerToSamplerMap_t& samplers_lut
);

void ExtractTextures(
  cgltf_data const* data,
  PointerToIndexMap_t const& image_indices,
  PointerToSamplerMap_t const& samplers_lut, //
  PointerToIndexMap_t& textures_indices,
  scene::ResourceBuffer<scene::Texture>& textures
);

void ExtractMaterials(
  cgltf_data const* data,
  PointerToIndexMap_t const& textures_indices,
  scene::ResourceBuffer<scene::Texture> const& textures,
  PointerToIndexMap_t& materials_indices,
  scene::ResourceBuffer<scene::Material>& materials
);

void ExtractSkeletons(
  cgltf_data const* data,
  PointerToIndexMap_t& skeleton_indices,
  scene::ResourceBuffer<scene::Skeleton>& skeletons
);

void ExtractMeshes(
  cgltf_data const* data,
  PointerToIndexMap_t const& materials_indices,
  scene::ResourceBuffer<scene::Material> const& materials,
  PointerToIndexMap_t const& skeleton_indices,
  scene::ResourceBuffer<scene::Skeleton>const& skeletons,
  scene::ResourceBuffer<scene::Mesh>& meshes,
  bool const bRestructureAttribs
);

void ExtractAnimations(
  cgltf_data const* data,
  std::string const& basename,
  scene::ResourceMap<scene::Skeleton>& skeletons_map,
  scene::ResourceMap<scene::AnimationClip>& animations_map
);

} // namespace internal::gltf_loader

/* -------------------------------------------------------------------------- */

#endif // HELLO_VK_FRAMEWORK_SCENE_PRIVATE_GLTF_LOADER_H_