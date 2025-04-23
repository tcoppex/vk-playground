#ifndef HELLO_VK_FRAMEWORK_SCENE_PRIVATE_GLTF_LOADER_H_
#define HELLO_VK_FRAMEWORK_SCENE_PRIVATE_GLTF_LOADER_H_

#include "framework/common.h"
#include "framework/scene/resources.h"
#include "framework/utils/cgltf_wrapper.h"

/* -------------------------------------------------------------------------- */

namespace internal::gltf_loader {

using PointerToIndexMap_t = std::unordered_map<void const*, uint32_t>;
using PointerToSamplerMap_t = std::unordered_map<void const*, VkSampler>;

// [probably better to use rather than PointerToIndexMap_t]
template<typename T>
using PointerToResourceMap_t = std::unordered_map<void const*, std::shared_ptr<T>>;

/* -------------------------------------------------------------------------- */

PointerToIndexMap_t ExtractImages(
  cgltf_data const* data,
  scene::ResourceBuffer<scene::ImageData>& images
);

PointerToSamplerMap_t ExtractSamplers(
  cgltf_data const* data,
  SamplerPool& sampler_pool
);

PointerToIndexMap_t ExtractTextures(
  cgltf_data const* data,
  PointerToIndexMap_t const& image_indices,
  PointerToSamplerMap_t const& samplers_lut, //
  scene::ResourceBuffer<scene::Texture>& textures
);

PointerToIndexMap_t ExtractMaterials(
  cgltf_data const* data,
  PointerToIndexMap_t const& textures_indices,
  scene::ResourceBuffer<scene::Texture> const& textures,
  scene::ResourceBuffer<scene::Material>& materials
);

PointerToIndexMap_t ExtractSkeletons(
  cgltf_data const* data,
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