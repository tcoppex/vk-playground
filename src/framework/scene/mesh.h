#ifndef HELLO_VK_FRAMEWORK_SCENE_MESH_H
#define HELLO_VK_FRAMEWORK_SCENE_MESH_H

#include "framework/common.h"

#include "framework/utils/geometry.h"
#include "framework/backend/types.h"      // for VertexInputDescriptor
#include "framework/renderer/pipeline.h"  // for PipelineVertexBufferDescriptors
#include "framework/scene/animation.h"
#include "framework/scene/material.h" //

namespace scene {

/* -------------------------------------------------------------------------- */

//
//  [WIP]
//
//  SubMeshes might be kept as shared_ptr with a list of all of them on the
//  Renderer / Resources instance.
//
//  For now a 'Mesh' handle shared transform info (world matrix, skeleton),
//  but technically the rendering are done per materials, so per submeshes.
//
//  The submesh material ref should probably be switch to raw value.
//

struct Mesh : Geometry {
 public:
  struct SubMesh {
    Mesh const* parent{};
    std::shared_ptr<MaterialRef> material_ref{}; //
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

  PipelineVertexBufferDescriptors get_vk_pipeline_vertex_buffer_descriptors() const;

  VkIndexType get_vk_index_type() const;

  VkPrimitiveTopology get_vk_primitive_topology() const;

  DrawDescriptor const& get_draw_descriptor(uint32_t const index = 0u) const {
    return submeshes[index].draw_descriptor;
  }

  void set_device_buffer_info(DeviceBufferInfo const& device_buffer_info) {
    device_buffer_info_ = device_buffer_info;
  }

 private:
  VkFormat get_vk_format(AttributeType const attrib_type) const;

  VertexInputDescriptor create_vertex_input_descriptors(
    AttributeOffsetMap const& attribute_to_offset,
    AttributeLocationMap const& attribute_to_location
  ) const;

 public:
  // ------------------------
  uint32_t transform_index{}; // Use to reference the transform SSBO.
  mat4f world_matrix{linalg::identity}; //
  std::shared_ptr<Skeleton> skeleton{};
  std::vector<SubMesh> submeshes{};
  // ------------------------

 protected:
  /* Offset from the mesh device buffers */
  DeviceBufferInfo device_buffer_info_{};
};

/* -------------------------------------------------------------------------- */

}  // namespace scene

#endif // HELLO_VK_FRAMEWORK_SCENE_MESH_H
