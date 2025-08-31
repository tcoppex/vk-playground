#include "framework/scene/host_resources.h"

// #include "framework/scene/private/gltf_loader.h"

/* -------------------------------------------------------------------------- */

namespace scene {

void HostResources::setup() {
  // Create default 1x1 textures for optionnal bindings.
  //  -> should it be left to each MaterialFx?
  {
    constexpr uint32_t kDefaultResourceSize = 32u;
    host_images.reserve(kDefaultResourceSize);
    textures.reserve(kDefaultResourceSize);

    auto push_default_texture{
      [&textures = this->textures, &host_images = this->host_images]
      (std::array<uint8_t, 4> const& c) -> uint32_t {
        uint32_t const texture_id = textures.size();
        textures.emplace_back( host_images.size() );
        host_images.emplace_back( c[0], c[1], c[2], c[3] );
        return texture_id;
      }
    };

    auto &bindings = default_bindings_;
    bindings.basecolor          = push_default_texture({255, 255, 255, 255});
    bindings.normal             = push_default_texture({128, 128, 255, 255});
    bindings.roughness_metallic = push_default_texture({  0, 255,   0, 255});
    bindings.occlusion          = push_default_texture({255, 255, 255, 255});
    bindings.emissive           = push_default_texture({  0,   0,   0, 255});
  }
}

bool HostResources::load_file(std::string_view filename) {
  return true;
}

// ----------------------------------------------------------------------------

void HostResources::reset_internal_descriptors() {
  /* Calculate the offsets to indivual mesh data inside the shared vertices
   * and indices buffers. */
  uint32_t transform_index = 0u;
  vertex_buffer_size = 0u;
  index_buffer_size = 0u;

  for (auto const& mesh : meshes) {
    // ---------
    mesh->transform_index = transform_index++; //
    mesh->set_resources_ptr(this); //
    mesh->set_buffer_info({
      .vertex_offset = vertex_buffer_size,
      .index_offset = index_buffer_size,
    });
    // ---------
    vertex_buffer_size += mesh->get_vertices().size();
    index_buffer_size += mesh->get_indices().size();
  }

  for (auto const& host_image : host_images) {
    total_image_size += host_image.getBytesize();
  }

#ifndef NDEBUG
  uint32_t const kMegabyte{ 1024u * 1024u };
  LOGI("> vertex buffer size %f Mb", vertex_buffer_size / static_cast<float>(kMegabyte));
  LOGI("> index buffer size %f Mb ", index_buffer_size / static_cast<float>(kMegabyte));
  LOGI("> total image size %f Mb ", total_image_size / static_cast<float>(kMegabyte));
#endif
}

}  // namespace scene

/* -------------------------------------------------------------------------- */
