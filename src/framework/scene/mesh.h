#ifndef HELLO_VK_FRAMEWORK_SCENE_MESH_H
#define HELLO_VK_FRAMEWORK_SCENE_MESH_H

#include "framework/common.h"

#include "framework/utils/geometry.h"
#include "framework/scene/animation.h"
#include "framework/scene/material.h"
#include "framework/backend/types.h"      // for VertexInputDescriptor
#include "framework/renderer/pipeline.h"  // for PipelineVertexBufferDescriptors

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
    Mesh const* parent{};
    std::shared_ptr<Material> material{};
    DrawDescriptor draw_descriptor{};
  };

 public:
  Mesh() = default;

  void initialize_submesh_descriptors(AttributeLocationMap const& attribute_to_location);

  PipelineVertexBufferDescriptors get_vk_pipeline_vertex_buffer_descriptors() const;

  /* Call initialize_submesh_descriptors beforehand. */
  PipelineVertexBufferDescriptors get_vk_pipeline_vertex_buffer_descriptors(AttributeLocationMap const& attribute_to_location);

  VkIndexType get_vk_index_type() const;

  VkPrimitiveTopology get_vk_primitive_topology() const;

  DrawDescriptor const& get_draw_descriptor(uint32_t const index = 0u) const {
    return submeshes.at(index).draw_descriptor;
  }

 private:

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
