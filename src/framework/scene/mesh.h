#ifndef VKFRAMEWORK_SCENE_MESH_H
#define VKFRAMEWORK_SCENE_MESH_H

#include "framework/common.h"

#include "framework/core/geometry.h"
#include "framework/backend/types.h"      // for VertexInputDescriptor
#include "framework/renderer/pipeline.h"  // for PipelineVertexBufferDescriptors
#include "framework/scene/animation.h"

namespace scene {

struct HostResources; //
struct MaterialRef;

/* -------------------------------------------------------------------------- */

struct Mesh : Geometry {
 public:
  struct SubMesh {
    Mesh const* parent{};
    DrawDescriptor draw_descriptor{};
    MaterialRef const* material_ref{};
  };

  struct BufferInfo {
    uint64_t vertex_offset{}; // (should probably be an array of binding count size)
    uint64_t index_offset{};
    uint64_t vertex_size{};
    uint64_t index_size{};
  };

 public:
  Mesh() = default;

  /* Bind mesh attributes to pipeline attributes location. */
  void initialize_submesh_descriptors(AttributeLocationMap const& attribute_to_location);

  /* Defines offset to actual data from external buffers. */
  void set_buffer_info(BufferInfo const& buffer_info) {
    buffer_info_ = {
      .vertex_offset = buffer_info.vertex_offset,
      .index_offset = buffer_info.index_offset,
      .vertex_size = get_vertices().size(),
      .index_size = get_indices().size(),
    };
  }

  /* Set pointer to global resources. */
  void set_resources_ptr(HostResources const* R);

  /* Return the mesh world transform. */
  mat4 const& world_matrix() const; //

 public:
  uint32_t transform_index{};
  std::vector<SubMesh> submeshes{};

 private:
  HostResources const* resources_ptr_{};
  BufferInfo buffer_info_{};

  /* ------- Renderer specifics ------- */

 public:
  PipelineVertexBufferDescriptors pipeline_vertex_buffer_descriptors() const;
  VkIndexType vk_index_type() const;
  VkPrimitiveTopology vk_primitive_topology() const;

 private:
  VkFormat vk_format(AttributeType const attrib_type) const;

  VertexInputDescriptor create_vertex_input_descriptors(
    AttributeOffsetMap const& attribute_to_offset,
    AttributeLocationMap const& attribute_to_location
  ) const;
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // VKFRAMEWORK_SCENE_MESH_H
