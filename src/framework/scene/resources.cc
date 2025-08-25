#include "framework/scene/resources.h"

#include "framework/backend/context.h"
#include "framework/backend/command_encoder.h"
#include "framework/renderer/renderer.h" //
#include "framework/renderer/sampler_pool.h"
#include "framework/utils/utils.h"

#include "framework/fx/_experimental/material_fx.h"

#include "framework/scene/private/gltf_loader.h"

/* -------------------------------------------------------------------------- */

namespace scene {

void Resources::release() {
  LOG_CHECK(allocator != nullptr);

  for (auto& img : device_images) {
    allocator->destroy_image(&img);
  }
  allocator->destroy_buffer(index_buffer);
  allocator->destroy_buffer(vertex_buffer);

  material_fx_registry_->release();
  delete material_fx_registry_;

  *this = {};
}

// ----------------------------------------------------------------------------

bool Resources::load_from_file(std::string_view const& filename, SamplerPool& sampler_pool, bool bRestructureAttribs) {
  LOG_CHECK( material_fx_registry_ != nullptr );

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
    using namespace internal::gltf_loader;

#if 0

    /* --- (alternative) Serialized version --- */

    auto samplers_lut       = ExtractSamplers(data, sampler_pool);
    auto skeletons_indices  = ExtractSkeletons(data, skeletons);
    auto images_indices     = ExtractImages(data, host_images);
    auto textures_indices   = ExtractTextures(data, images_indices, samplers_lut, textures);
    auto materials_indices  = ExtractMaterials(data, textures_indices, textures, material_refs);
    ExtractMeshes(data, materials_indices, material_refs, skeletons_indices, skeletons, meshes, bRestructureAttribs);

#else

    /* --- Async tasks version --- */

    auto run_task = utils::RunTaskGeneric<void>;
    auto run_task_ret = utils::RunTaskGeneric<PointerToIndexMap_t>;
    auto run_task_sampler = utils::RunTaskGeneric<PointerToSamplerMap_t>;

    auto taskSamplers = run_task_sampler([data, &sampler_pool] {
      return ExtractSamplers(data, sampler_pool);
    });

    auto taskSkeletons = run_task_ret([data, &skeletons = this->skeletons] {
      return ExtractSkeletons(data, skeletons);
    });

    // [real bottleneck, internally images are loaded asynchronously and must be waited for at the end]
    auto taskImageData = run_task_ret([data, &host_images = this->host_images] {
      return ExtractImages(data, host_images);
    });

    auto taskTextures = run_task_ret([&taskImageData, &taskSamplers, data, &textures = this->textures] {
      auto images_indices = taskImageData.get();
      auto samplers_lut = taskSamplers.get();
      return ExtractTextures(data, images_indices, samplers_lut, textures);
    });

    auto taskMaterials = run_task_ret([
      &taskTextures,
      data,
      &textures = this->textures,
      &material_refs = this->material_refs,
      &material_fx_registry = *this->material_fx_registry_
    ] {
      auto textures_indices = taskTextures.get();
      return ExtractMaterials(data, textures_indices, textures, material_refs, material_fx_registry);
    });

    auto skeletons_indices = taskSkeletons.get();

    auto taskAnimations = run_task([data, &skeletons_indices, &skeletons = this->skeletons] {
      // ExtractAnimations(data, basename, skeletons_indices, skeletons, animations_map);
    });

    auto taskMeshes = run_task([
      &taskMaterials,
      data,
      bRestructureAttribs,
      &skeletons_indices,
      &material_refs = this->material_refs,
      &skeletons = this->skeletons,
      &meshes = this->meshes
    ] {
      auto materials_indices = taskMaterials.get();
      ExtractMeshes(data, materials_indices, material_refs, skeletons_indices, skeletons, meshes, bRestructureAttribs);
    });

    taskAnimations.get();
    taskMeshes.get();

#endif

    /* Wait for the host images to finish loading before using them. */
    for (auto const& host_image : host_images) {
      host_image->getLoadAsyncResult();
    }

    reset_internal_device_resource_info();
  }

  /* Be sure to have finished loading all images before freeing gltf data */
  cgltf_free(data);

#ifndef NDEBUG
    std::cout << "Images count : " << host_images.size() << std::endl;
    std::cout << "Texture count : " << textures.size() << std::endl;
    std::cout << "Material count : " << material_refs.size() << std::endl;
    std::cout << "Skeleton count : " << skeletons.size() << std::endl;
    std::cout << "Animation count : " << animations_map.size() << std::endl;
    std::cout << "Mesh count : " << meshes.size() << std::endl;
    std::cerr << " ----------------- gltf loaded ----------------- " << std::endl;
#endif

  return true;
}

// ----------------------------------------------------------------------------

void Resources::initialize_submesh_descriptors(Mesh::AttributeLocationMap const& attribute_to_location) {
  for (auto& mesh : meshes) {
    mesh->initialize_submesh_descriptors(attribute_to_location);
  }

  // --------------------
  // [~] When we expect Tangent we force recalculate them.
  //     Resulting indices might be incorrect.
  if (attribute_to_location.contains(Geometry::AttributeType::Tangent)) {
    for (auto& mesh : meshes) { mesh->recalculate_tangents(); } //
  }
  // --------------------
}

// ----------------------------------------------------------------------------

void Resources::upload_to_device(Context const& context, bool const bReleaseHostDataOnUpload) {
  if (!allocator) {
    allocator = context.get_resource_allocator();
  }

  /* Transfer Materials */
  material_fx_registry_->push_material_storage_buffers();

  /* Transfer Textures */
  if (total_image_size > 0) {
    upload_images(context);

    material_fx_registry_->update_texture_atlas([this](uint32_t binding) {
      return this->descriptor_set_texture_atlas_entry(binding);
    });
  }

  /* Transfer Buffers */
  if (vertex_buffer_size > 0) {
    upload_buffers(context);
  }

  /* Clear host data once uploaded */
  if (bReleaseHostDataOnUpload) {
    host_images.clear();
    host_images.shrink_to_fit();
    for (auto const& mesh : meshes) {
      mesh->clear_indices_and_vertices(); //
    }
  }
}

// ----------------------------------------------------------------------------

DescriptorSetWriteEntry Resources::descriptor_set_texture_atlas_entry(uint32_t const binding) const {
  DescriptorSetWriteEntry texture_atlas_entry{
    .binding = binding,
    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
  };

  if (textures.empty()) {
    return texture_atlas_entry;
  }
  LOG_CHECK( !device_images.empty() );

  for (auto const& texture : textures) {
    auto const& img = device_images.at(texture->getChannelIndex());
    texture_atlas_entry.images.push_back({
      .sampler = texture->sampler,
      .imageView = img.view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    });
  }

  return texture_atlas_entry;
}

// ----------------------------------------------------------------------------

void Resources::prepare_material_fx(Context const& context, Renderer const& renderer) {
  material_fx_registry_ = new MaterialFxRegistry();
  material_fx_registry_->setup(context, renderer);
}

// ----------------------------------------------------------------------------

void Resources::render(RenderPassEncoder const& pass, Camera const& camera) {
  LOG_CHECK( material_fx_registry_ != nullptr );

  using FxToSubmeshesMap = std::map< MaterialFx*, std::vector<Mesh::SubMesh const*> >;

  // 1) Retrieve submeshes associated to each Fx.
  //    { Submeshes share Materials share Fx }
  FxToSubmeshesMap fx_to_submeshes{};
  for (auto const& mesh : meshes) {
    for (auto const& submesh : mesh->submeshes) {
      if (auto matref = submesh.material_ref; matref) {
        if (auto fx = material_fx_registry_->material_fx(*matref); fx) {
          fx_to_submeshes[fx].push_back(&submesh);
        }
      }
    }
  }
  LOG_CHECK( !fx_to_submeshes.empty() );

  // 2) (optionnal) Preprocess each buffer of submeshes
  //  -> sort per material instance
  //  -> sort wrt camera
  // ..

  // 3) (optionnal) Create shared material buffer for each Fx.
  // ..

  // 4) Render each Fx.
  uint32_t instance_index = 0u;
  for (auto& [fx, submeshes] : fx_to_submeshes) {
    // Bind pipeline & descriptor set.
    fx->prepareDrawState(pass); //

    // Update host-side camera (pushConstant, uniforms).
    fx->updateCameraData(camera);

    // Transfer app uniforms to device (internal to fx for now).
    fx->pushUniforms();

    // Draw submeshes.
    for (auto* submesh : submeshes) {
      auto mesh = submesh->parent;

      // (tmp) should go to separate fx wide SSBO
      // ---------------------
      fx->setWorldMatrix(mesh->world_matrix); //
      // ---------------------

      // Submesh's pushConstants.
      fx->setMaterialIndex(submesh->material_ref->material_index);
      fx->setInstanceIndex(instance_index++);
      fx->pushConstant(pass);

      pass.set_primitive_topology(mesh->get_vk_primitive_topology());
      pass.draw(submesh->draw_descriptor, vertex_buffer, index_buffer); //
    }
  }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void Resources::reset_internal_device_resource_info() {
  /* Calculate the offsets to indivual mesh data inside the shared vertices and indices buffers. */
  vertex_buffer_size = 0u;
  index_buffer_size = 0u;

  for (auto const& mesh : meshes) {
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

  for (auto const& host_image : host_images) {
    total_image_size += host_image->getBytesize();
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

  device_images.reserve(host_images.size()); //

  std::vector<VkBufferImageCopy> copies{};
  copies.reserve(host_images.size());

  // uint32_t channel_index = 0u;
  uint64_t staging_offset = 0u;
  for (auto const& host_image : host_images) {
    VkExtent3D const extent{
      .width = static_cast<uint32_t>(host_image->width),
      .height = static_cast<uint32_t>(host_image->height),
      .depth = 1u,
    };
    device_images.push_back(context.create_image_2d(
      extent.width,
      extent.height,
      VK_FORMAT_R8G8B8A8_UNORM, //
      VK_IMAGE_USAGE_TRANSFER_DST_BIT
    ));

    /* Upload image to staging buffer */
    uint32_t const img_bytesize{ host_image->getBytesize() };
    allocator->write_buffer(
      staging_buffer, staging_offset, host_image->getPixels(), 0u, img_bytesize
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
    for (uint32_t i = 0u; i < device_images.size(); ++i) {
      vkCmdCopyBufferToImage(cmd.get_handle(), staging_buffer.buffer, device_images[i].image, transfer_layout, 1u, &copies[i]);
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
