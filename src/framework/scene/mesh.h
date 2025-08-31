#ifndef VKFRAMEWORK_SCENE_MESH_H
#define VKFRAMEWORK_SCENE_MESH_H

#include "framework/common.h"

#include "framework/core/geometry.h"
#include "framework/backend/types.h"      // for VertexInputDescriptor
#include "framework/renderer/pipeline.h"  // for PipelineVertexBufferDescriptors
#include "framework/scene/animation.h"

namespace scene {

struct Resources; //
struct MaterialRef;

/* -------------------------------------------------------------------------- */

//
//  SubMeshes might be kept as shared_ptr with a list of all of them on the
//  Renderer / Resources instance.
//
//  For now a 'Mesh' handle shared transform info (world matrix, skeleton),
//  but technically the rendering are done per materials, so per submeshes.
//

struct Mesh : Geometry {
 public:
  struct SubMesh {
    Mesh const* parent{};
    MaterialRef* material_ref{}; //
    DrawDescriptor draw_descriptor{};
  };

  struct DeviceBufferInfo {
    uint64_t vertex_offset{}; // (should probably be an array of binding count size)
    uint64_t index_offset{};
    uint64_t vertex_size{};
    uint64_t index_size{};
  };

 public:
  Mesh() = default;

  /* Bind mesh attributes to pipeline attributes location. */
  void initialize_submesh_descriptors(AttributeLocationMap const& attribute_to_location);

  void set_device_buffer_info(DeviceBufferInfo const& device_buffer_info) {
    device_buffer_info_ = {
      .vertex_offset = device_buffer_info.vertex_offset,
      .index_offset = device_buffer_info.index_offset,
      .vertex_size = get_vertices().size(),
      .index_size = get_indices().size(),
    };
  }

  void set_resources_ptr(Resources const* R);

  mat4 const& world_matrix() const;

 public:
  PipelineVertexBufferDescriptors vk_pipeline_vertex_buffer_descriptors() const;

  VkIndexType vk_index_type() const;

  VkPrimitiveTopology vk_primitive_topology() const;

 private:
  VkFormat vk_format(AttributeType const attrib_type) const;

  VertexInputDescriptor create_vertex_input_descriptors(
    AttributeOffsetMap const& attribute_to_offset,
    AttributeLocationMap const& attribute_to_location
  ) const;

 public:
  // ------------------------
  uint32_t transform_index{};
  std::vector<SubMesh> submeshes{};
  Resources const* resources_ptr{};
  // mat4f world_matrix{linalg::identity};
  // Skeleton* skeleton{};
  // ------------------------

 protected:
  /* Offset from the mesh device buffers */
  DeviceBufferInfo device_buffer_info_{};
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // VKFRAMEWORK_SCENE_MESH_H
