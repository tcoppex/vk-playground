#ifndef HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H
#define HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H

#include <unordered_map>

#include "stb/stb_image.h"

#include "framework/common.h"
#include "framework/scene/animation.h"
#include "framework/scene/image.h"
#include "framework/scene/material.h"
#include "framework/scene/mesh.h"

class Context;
class ResourceAllocator;

/* -------------------------------------------------------------------------- */

namespace scene {

struct Resources {
  static bool constexpr kRestructureAttribs = true;

  template<typename T>
  using ResourceMap = std::unordered_map<std::string, std::shared_ptr<T>>;

  ResourceMap<Image> images{};
  ResourceMap<Material> materials{};
  ResourceMap<AnimationClip> animations{};
  ResourceMap<Skeleton> skeletons{};

  std::vector<std::shared_ptr<Mesh>> meshes{}; //
  // std::vector<Sampler> samplers{}; //

  // NOTE :
  // we should rather extract image first, THEN textures (ie. Image + Samplers).
  // Here we treat 'textures' as images directly, which is bad.
  std::vector<backend::Image> textures{}; //

  backend::Buffer vertex_buffer;
  backend::Buffer index_buffer;

  uint32_t vertex_buffer_size{0u};
  uint32_t index_buffer_size{0u};
  uint32_t total_image_size{0u};

  Resources() = default;

  Resources(std::string_view const& filename) {
    load_from_file(filename);
  }

  void release(std::shared_ptr<ResourceAllocator> allocator);

  bool load_from_file(std::string_view const& filename, bool bRestructureAttribs = kRestructureAttribs);

  void reset_meshes_device_buffer_info();

  /* Bind mesh attributes to pipeline locations. */
  void initialize_submesh_descriptors(Mesh::AttributeLocationMap const& attribute_to_location);

  void upload_to_device(Context const& context);
};

}  // namespace scene

/* -------------------------------------------------------------------------- */

using GLTFScene = std::shared_ptr<scene::Resources>;

#endif // HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H
