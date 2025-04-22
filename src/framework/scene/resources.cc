#include "framework/scene/resources.h"
#include "framework/scene/private/gltf_loader.h"

#include "framework/backend/context.h"
#include "framework/renderer/sampler_pool.h"
#include "framework/utils/utils.h"

/* -------------------------------------------------------------------------- */

namespace scene {

void Resources::release() {
  assert(allocator != nullptr);
  for (auto& img : device_images) {
    allocator->destroy_image(&img);
  }
  allocator->destroy_buffer(index_buffer);
  allocator->destroy_buffer(vertex_buffer);
  *this = {};
}

// ----------------------------------------------------------------------------

bool Resources::load_from_file(std::string_view const& filename, SamplerPool& sampler_pool, bool bRestructureAttribs) {
  std::string const basename{ utils::ExtractBasename(filename) };

  cgltf_options options{};
  cgltf_result result{};
  cgltf_data* data{};

  utils::FileReader file{};
  if (!file.read(filename)) {
    return false;
  }

  if (result = cgltf_parse(&options, file.buffer.data(), file.buffer.size(), &data); cgltf_result_success != result) {
    LOGE("GLTF: failed to parse file \"%s\" %d.\n", basename.c_str(), result);
    return false;
  }

  if (result = cgltf_load_buffers(&options, data, filename.data()); cgltf_result_success != result) {
    LOGE("GLTF: failed to load buffers in \"%s\" %d.\n", basename.c_str(), result);
    cgltf_free(data);
    return false;
  }

  /* Extract data */
  {
    PointerToStringMap_t texture_names{};
    PointerToStringMap_t material_names{};
    PointerToSamplerMap_t samplers_lut{};

    // (hack) prepass to give proper name to texture when they've got none.
    // ExtractMaterials(basename, texture_names, material_names, data, *this);

    // ExtractImages(data, device_images);
    ExtractSamplers(data, sampler_pool, samplers_lut);

    ExtractTextures(data, basename, texture_names, textures_map/*, samplers_lut*/);
    ExtractMaterials(data, basename, texture_names, material_names, textures_map, materials_map);
    ExtractMeshes(data, basename, bRestructureAttribs, material_names, textures_map, materials_map, skeletons_map, meshes);
    ExtractAnimations(data, basename, skeletons_map, animations_map);

    reset_internal_device_resource_info();
  }
  cgltf_free(data);

#ifndef NDEBUG
    // std::cout << "Texture count : " << textures_map.size() << std::endl;
    // std::cout << "Material count : " << materials_map.size() << std::endl;
    // std::cout << "Skeleton count : " << skeletons.size() << std::endl;
    // std::cout << "Animation count : " << animations.size() << std::endl;
    // std::cout << "Mesh count : " << meshes.size() << std::endl;
    // std::cerr << " ----------------- gltf loaded ----------------- " << std::endl;
#endif

  return true;
}

// ----------------------------------------------------------------------------

void Resources::initialize_submesh_descriptors(Mesh::AttributeLocationMap const& attribute_to_location) {
  /* Bind mesh attributes to pipeline locations. */
  for (auto& mesh : meshes) {
    mesh->initialize_submesh_descriptors(attribute_to_location);
  }
}

// ----------------------------------------------------------------------------

void Resources::upload_to_device(Context const& context, bool const bReleaseHostDataOnUpload) {
  if (!allocator) {
    allocator = context.get_resource_allocator();
  }

  /* Transfer Textures */
  if (total_image_size > 0) {
    upload_images(context);
  }

  /* Transfer Buffers */
  if (vertex_buffer_size > 0) {
    upload_buffers(context);
  }

  /* clear host data once uploaded */
  if (bReleaseHostDataOnUpload) {
    for (auto const& mesh : meshes) {
      mesh->clear_indices_and_vertices(); //
    }
  }
}

// ----------------------------------------------------------------------------

void Resources::reset_internal_device_resource_info() {
  /* Calculate the offsets to indivual mesh data inside the shared vertices and indices buffers. */
  vertex_buffer_size = 0u;
  index_buffer_size = 0u;

  for (auto& mesh : meshes) {
    uint64_t const vertex_size = mesh->get_vertices().size();
    uint64_t const index_size = mesh->get_indices().size();
    mesh->set_device_buffer_info({
      .vertex_offset = vertex_buffer_size,
      .index_offset = index_buffer_size,
      .vertex_size = vertex_size,
      .index_size = index_size,
    });
    vertex_buffer_size += vertex_size;
    index_buffer_size += index_size;
  }

  for (auto [_, texture] : textures_map) {
    total_image_size += scene::Texture::kDefaultNumChannels * texture->width * texture->height; //
  }

#ifndef NDEBUG
  // uint32_t const kMegabyte{ 1024u * 1024u };
  // LOGI("> vertex buffer size %f Mb", vertex_buffer_size / static_cast<float>(kMegabyte));
  // LOGI("> index buffer size %f Mb ", index_buffer_size / static_cast<float>(kMegabyte));
  // LOGI("> total image size %f Mb ", total_image_size / static_cast<float>(kMegabyte));
#endif
}

// ----------------------------------------------------------------------------

void Resources::upload_images(Context const& context) {
  assert(total_image_size > 0);
  assert(allocator != nullptr);

  /* Create a staging buffer. */
  backend::Buffer staging_buffer{
    allocator->create_staging_buffer( total_image_size ) //
  };

  // ----------------
  // xxx devices_images is max the size of texture_map but could be less xxx
  device_images.reserve(textures_map.size()); //
  // ----------------

  std::vector<VkBufferImageCopy> copies;
  copies.reserve(textures_map.size());

  uint32_t channel_index = 0u;
  uint64_t staging_offset = 0u;
  for (auto& [_, texture] : textures_map) {
    // ----------
    // [should correspond to the correct image in device_images instead]
    texture->texture_index = channel_index++; //
    // ----------

    VkExtent3D const extent{
      .width = static_cast<uint32_t>(texture->width),
      .height = static_cast<uint32_t>(texture->height),
      .depth = 1u,
    };
    device_images.push_back(context.create_image_2d(
      extent.width, extent.height, 1u, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT
    ));

    /* Upload image to staging buffer */
    uint32_t const img_bytesize{
      static_cast<uint32_t>(4u * extent.width * extent.height) //
    };
    allocator->write_buffer(
      staging_buffer, staging_offset, texture->pixels.get(), 0u, img_bytesize
    );
    copies.push_back({
      .bufferOffset = staging_offset,
      .imageSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .layerCount = 1u,
      },
      .imageExtent = extent,
    });
    staging_offset += img_bytesize;
  }

  auto cmd{ context.create_transient_command_encoder(Context::TargetQueue::Transfer) };
  {
    VkImageLayout const transfer_layout{ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL };

    cmd.transition_images_layout(device_images, VK_IMAGE_LAYOUT_UNDEFINED, transfer_layout);
    for (uint32_t tex_id = 0u; tex_id < device_images.size(); ++tex_id) {
      vkCmdCopyBufferToImage(cmd.get_handle(), staging_buffer.buffer, device_images[tex_id].image, transfer_layout, 1u, &copies[tex_id]);
    }
    cmd.transition_images_layout(device_images, transfer_layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
  context.finish_transient_command_encoder(cmd);
}

// ----------------------------------------------------------------------------

void Resources::upload_buffers(Context const& context) {
  assert(vertex_buffer_size > 0);
  assert(allocator != nullptr);

  vertex_buffer = allocator->create_buffer(
    vertex_buffer_size,
    VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
    VMA_MEMORY_USAGE_GPU_ONLY
  );

  if (index_buffer_size > 0) {
    index_buffer = allocator->create_buffer(
      index_buffer_size,
      VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
      VMA_MEMORY_USAGE_GPU_ONLY
    );
  }

  /* Copy host mesh data to staging buffer */
  backend::Buffer mesh_staging_buffer{
    allocator->create_staging_buffer(vertex_buffer_size + index_buffer_size)
  };
  {
    size_t vertex_offset{0u};
    size_t index_offset{vertex_buffer_size};

    for (auto const& mesh : meshes) {
      auto const& vertices = mesh->get_vertices();
      allocator->write_buffer(mesh_staging_buffer, vertex_offset, vertices.data(), 0u, vertices.size());
      vertex_offset += vertices.size();


      if (index_buffer_size > 0) {
        auto const& indices = mesh->get_indices();
        allocator->write_buffer(mesh_staging_buffer, index_offset, indices.data(), 0u, indices.size());
        index_offset += indices.size();
      }
    }
  }

  /* Copy device data from staging buffers to their respective buffers. */
  auto cmd = context.create_transient_command_encoder(Context::TargetQueue::Transfer);
  {
    cmd.copy_buffer(mesh_staging_buffer, 0u, vertex_buffer, 0u, vertex_buffer_size);
    if (index_buffer_size > 0) {
      cmd.copy_buffer(mesh_staging_buffer, vertex_buffer_size, index_buffer, 0u, index_buffer_size);
    }
  }
  context.finish_transient_command_encoder(cmd);
}

}  // namespace scene

/* -------------------------------------------------------------------------- */
