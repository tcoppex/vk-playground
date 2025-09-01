#ifndef VKFRAMEWORK_RENDERER_RAYTRACER_H_
#define VKFRAMEWORK_RENDERER_RAYTRACER_H_

#include "framework/backend/context.h"
#include "framework/backend/accel_struct.h"
#include "framework/scene/host_resources.h" // for scene::ResourceBuffer
#include "framework/scene/mesh.h"

/* -------------------------------------------------------------------------- */

class Raytracer {
 public:
  void init(Context const& ctx);

  void build(
    scene::ResourceBuffer<scene::Mesh> const& meshes,
    backend::Buffer const& vertex_buffer,
    backend::Buffer const& index_buffer
  );

  void build_blas(scene::Mesh::SubMesh const& submesh);

  void build_tlas(bool allow_update = false);

  void release();

 private:
  Context const* context_ptr_{};
  VkDeviceAddress vertex_address_{};
  VkDeviceAddress index_address_{};

  std::vector<backend::BLAS> blas_{}; // one per submesh
  backend::TLAS tlas_{};

  backend::Buffer transient_buffer_{};
};

/* -------------------------------------------------------------------------- */

#endif