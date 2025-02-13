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

namespace scene {

/* -------------------------------------------------------------------------- */

struct Resources {
  template<typename T> using ResourceMap = std::unordered_map<std::string, std::shared_ptr<T>>;

  ResourceMap<Image> images{};
  ResourceMap<Material> materials{};
  ResourceMap<AnimationClip> animations{};
  ResourceMap<Skeleton> skeletons{};

  std::vector<std::shared_ptr<Mesh>> meshes{}; //
  std::vector<backend::Image> textures{}; //

  uint32_t vertex_buffer_size{0u};
  uint32_t index_buffer_size{0u};
  uint32_t total_image_size{0u};

  Resources() = default;

  void release(std::shared_ptr<ResourceAllocator> allocator);

  Resources(std::string_view const& filename) {
    load_from_file(filename);
  }

  bool load_from_file(std::string_view const& filename);

  void initialize_submesh_descriptors(Mesh::AttributeLocationMap const& attribute_to_location);
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_RESOURCES_H
