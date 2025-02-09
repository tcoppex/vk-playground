#ifndef HELLO_VK_FRAMEWORK_SCENE_MESH_H
#define HELLO_VK_FRAMEWORK_SCENE_MESH_H

#include "framework/common.h"
#include "framework/utils/geometry.h"
#include "framework/backend/types.h" // for VertexInputDescriptor

#include "framework/scene/animation.h"
#include "framework/scene/material.h"

#include "framework/backend/command_encoder.h"

namespace scene {

/* -------------------------------------------------------------------------- */

//
//  [WIP]
//
//  SubMeshes might be kept as shared_ptr with a list of all of them on the
//  Renderer / Resources instance.
//
//  For now a 'Mesh' handle shared transform info (world matrix, skeleton),
//  but technically the rendering are done per materials, so per submeshes
//

struct Mesh : Geometry {
 public:
  struct SubMesh {
    std::weak_ptr<Mesh const> parent{};
    std::shared_ptr<Material> material{};
    DrawDescriptor draw_descriptor{};
  };

 public:
  Mesh() = default;

  VkPrimitiveTopology get_vk_primitive_topology() const;

  void initialize_submesh_descriptors(AttributeLocationMap const& attribute_to_location);

 private:
  VkIndexType get_vk_index_type() const;

  VkFormat get_vk_format(AttributeType const attrib_type) const;

  VertexInputDescriptor create_vertex_input_descriptors(AttributeOffsetMap const& attribute_to_offset, AttributeLocationMap const& attribute_to_location) const;

 public:
  mat4f world_matrix{linalg::identity};
  std::shared_ptr<Skeleton> skeleton{};
  std::vector<SubMesh> submeshes{};

  /* Offset from general buffers */
  uint64_t vertex_offset{}; // (should probably be an array of binding count size)
  uint32_t vertex_size{}; //
  uint64_t index_offset{};
  uint32_t index_size{};
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_MESH_H
