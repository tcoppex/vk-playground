#include "framework/scene/resources.h"

#include "framework/backend/context.h"
#include "framework/backend/command_encoder.h"
#include "framework/renderer/renderer.h" //
#include "framework/renderer/sampler_pool.h"
#include "framework/utils/utils.h"
#include "framework/renderer/fx/material/material_fx.h" //

#include "framework/scene/private/gltf_loader.h"
#include "framework/shaders/scene/interop.h"

/* -------------------------------------------------------------------------- */

namespace scene {

void Resources::release() {
  LOG_CHECK(allocator_ != nullptr);

  for (auto& img : device_images) {
    allocator_->destroy_image(&img);
  }
  allocator_->destroy_buffer(frame_ubo_);
  allocator_->destroy_buffer(index_buffer);
  allocator_->destroy_buffer(vertex_buffer);
  allocator_->destroy_buffer(transforms_ssbo_);

  material_fx_registry_->release();
  delete material_fx_registry_;

  *this = {};
}

// ----------------------------------------------------------------------------

void Resources::prepare_material_fx(Context const& context, Renderer const& renderer) {
  material_fx_registry_ = new MaterialFxRegistry();
  material_fx_registry_->init(context, renderer);

  // Create default 1x1 textures for optionnal bindings.
  //  -> should it be left to each MaterialFx?
  {
    auto const& sampler = renderer.get_default_sampler();

    auto push_default_texture{
      [&textures = this->textures, &host_images = this->host_images, &sampler]
      (std::array<uint8_t, 4> const& c) -> uint32_t {
        uint32_t texture_id = textures.size();
        textures.push_back( std::make_shared<Texture>(host_images.size(), sampler) );
        host_images.push_back( std::make_shared<ImageData>(c[0], c[1], c[2], c[3]) );
        return texture_id;
      }
    };

    auto &bindings = optionnal_texture_binding_;
    bindings.basecolor          = push_default_texture({255, 255, 255, 255});
    bindings.normal             = push_default_texture({128, 128, 255, 255});
    bindings.roughness_metallic = push_default_texture({  0, 255,   0, 255});
    bindings.occlusion          = push_default_texture({255, 255, 255, 255});
    bindings.emissive           = push_default_texture({  0,   0,   0, 255});
  }
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
    LOGE("GLTF: failed to read the file.");
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

    PreprocessMaterials(data, *material_fx_registry_);
    material_fx_registry_->setup();

#if 0

    /* --- Serialized version --- */

    auto samplers_lut       = ExtractSamplers(data, sampler_pool);
    auto skeletons_indices  = ExtractSkeletons(data, skeletons);
    auto images_indices     = ExtractImages(data, host_images);
    auto textures_indices   = ExtractTextures(
      data, images_indices, samplers_lut, textures
    );
    auto materials_indices  = ExtractMaterials(
      data, textures_indices, textures, material_refs, *material_fx_registry_, optionnal_texture_binding_
    );
    ExtractMeshes(
      data, materials_indices, material_refs,
      skeletons_indices, skeletons, meshes, bRestructureAttribs
    );

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

    auto taskTextures = run_task_ret(
      [&taskImageData, &taskSamplers, data, &textures = this->textures] {
      auto images_indices = taskImageData.get();
      auto samplers_lut = taskSamplers.get();
      return ExtractTextures(data, images_indices, samplers_lut, textures);
    });

    auto taskMaterials = run_task_ret([
      &taskTextures,
      data,
      &textures = this->textures,
      &material_refs = this->material_refs,
      &material_fx_registry = *this->material_fx_registry_,
      &default_bindings = this->optionnal_texture_binding_
    ] {
      auto textures_indices = taskTextures.get();
      return ExtractMaterials(
        data, textures_indices, textures, material_refs, material_fx_registry, default_bindings
      );
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
    // This will also display the extra data procedurally created.
    std::cout << basename << " loaded." << std::endl;
    std::cout << "┌────────────┬─────────── " << std::endl;
    std::cout << "│ Images     │ " << host_images.size() << std::endl;
    std::cout << "│ Textures   │ " << textures.size() << std::endl;
    std::cout << "│ Materials  │ " << material_refs.size() << std::endl;
    std::cout << "│ Skeletons  │ " << skeletons.size() << std::endl;
    std::cout << "│ Animations │ " << animations_map.size() << std::endl;
    std::cout << "│ Meshes     │ " << meshes.size() << std::endl;
    std::cerr << "└────────────┴───────────" << std::endl;
#endif

  return true;
}

// ----------------------------------------------------------------------------

void Resources::initialize_submesh_descriptors(Mesh::AttributeLocationMap const& attribute_to_location) {
  for (auto mesh : meshes) {
    mesh->initialize_submesh_descriptors(attribute_to_location);
  }

  // --------------------
  // [~] When we expect Tangent we force recalculate them.
  //     Resulting indices might be incorrect.

  if (attribute_to_location.contains(Geometry::AttributeType::Tangent)) {
    // for (auto& mesh : meshes) { mesh->recalculate_tangents(); } //
  }
  // --------------------
}

// ----------------------------------------------------------------------------

void Resources::upload_to_device(Context const& context, bool const bReleaseHostDataOnUpload) {
  context_ptr_ = &context;
  if (!allocator_) {
    allocator_ = context.get_resource_allocator();
  }

  /* Create the shared Frame UBO */
  frame_ubo_ = allocator_->create_buffer(
    sizeof(FrameData),
      VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT
    | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_AUTO,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    | VMA_ALLOCATION_CREATE_MAPPED_BIT
  );
  material_fx_registry_->update_frame_ubo(frame_ubo_);

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
    auto const& img = device_images.at(texture->channel_index());
    texture_atlas_entry.images.push_back({
      .sampler = texture->sampler,
      .imageView = img.view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    });
  }

  return texture_atlas_entry;
}

// ----------------------------------------------------------------------------

void Resources::update(Camera const& camera, VkExtent2D const& surfaceSize, float elapsedTime) {
  // Update the shared Frame UBO.
  FrameData const frame_data{
    .projectionMatrix = camera.proj(),
    .viewMatrix = camera.view(),
    .viewProjMatrix = camera.viewproj(),
    .cameraPos_Time = vec4(camera.position(), elapsedTime),
    .resolution = vec2(surfaceSize.width, surfaceSize.height),
  };

  context_ptr_->transfer_host_to_device(
    &frame_data, sizeof(frame_data), frame_ubo_
  );

  ///
  /// DevNote
  /// We could improve the overall sorting by using a generic 64bits sorting
  /// key (pipeline << 32) | (material << 16) | depthBits), using a single buffer.
  ///

  // Retrieve submeshes associated to each MaterialFx.
  if constexpr (true) {
    lookups_ = {};
    for (auto const& mesh : meshes) {
      for (auto const& submesh : mesh->submeshes) {
        if (auto matref = submesh.material_ref; matref) {
          auto const alpha_mode = matref->states.alpha_mode;
          auto fx = material_fx_registry_->material_fx(*matref);
          lookups_[alpha_mode][fx].push_back(&submesh);
        }
      }
    }
    //reset_scene_lookups = false;
  }

  /// Preprocess each buffer of submeshes.

  using SortKey = std::pair<float, size_t>; // (depthProxy, index)
  std::vector<SortKey> sortkeys{};
  vec3 const camera_dir{camera.direction()};
  SubMeshBuffer swap_buffer{};

  // Update the submeshes buffer to be sorted.
  auto sort_submeshes = [&](SubMeshBuffer &submeshes, auto comp) {
    sortkeys = {};
    sortkeys.reserve(submeshes.size());
    for (size_t i = 0; i < submeshes.size(); ++i) {
      vec3 const pos = lina::to_vec3(submeshes[i]->parent->world_matrix.w);
      vec3 const v = camera.position() - pos;
      float const dp = linalg::dot(camera_dir, v);
      sortkeys.emplace_back(dp, i);
    }
    std::ranges::sort(sortkeys, comp, &SortKey::first);

    // final-sort on submeshes by swapping with new buffer.
    swap_buffer.resize(submeshes.size());
    for (size_t i = 0; i < submeshes.size(); ++i) {
      auto [_, submesh_index] = sortkeys[i];
      swap_buffer[i] = std::move(submeshes[submesh_index]);
    }
    submeshes.swap(swap_buffer);
  };

  // Sort front to back for depth testing.
  for (auto& [_, submeshes] : lookups_[MaterialStates::AlphaMode::Opaque]) {
    sort_submeshes(submeshes, std::less{});
  }
  for (auto& [fx, submeshes] : lookups_[MaterialStates::AlphaMode::Mask]) {
    sort_submeshes(submeshes, std::less{});
  }
  // Sort back to front for alpha blending.
  for (auto& [fx, submeshes] : lookups_[MaterialStates::AlphaMode::Blend]) {
    sort_submeshes(submeshes, std::greater{});
  }
}

// ----------------------------------------------------------------------------

void Resources::render(RenderPassEncoder const& pass) {
  LOG_CHECK( material_fx_registry_ != nullptr );
  // Render each Fx.
  uint32_t instance_index = 0u;
  for (auto& lookup : lookups_) {
    for (auto& [fx, submeshes] : lookup) {
      // Bind pipeline & descriptor set.
      auto const& states = submeshes[0]->material_ref->states;
      fx->prepareDrawState(pass, states);

      // Draw submeshes.
      for (auto submesh : submeshes) {
        auto mesh = submesh->parent;

        // Submesh's pushConstants.
        fx->setTransformIndex(mesh->transform_index);
        fx->setMaterialIndex(submesh->material_ref->material_index);
        fx->setInstanceIndex(instance_index++); //
        fx->pushConstant(pass);

        pass.set_primitive_topology(mesh->get_vk_primitive_topology());
        pass.draw(submesh->draw_descriptor, vertex_buffer, index_buffer); //
      }
    }
  }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void Resources::reset_internal_device_resource_info() {
  /* Calculate the offsets to indivual mesh data inside the shared vertices
   * and indices buffers. */
  vertex_buffer_size = 0u;
  index_buffer_size = 0u;

  uint32_t transform_index = 0u;
  for (auto const& mesh : meshes) {
    uint64_t const mesh_vertices_size = mesh->get_vertices().size();
    uint64_t const mesh_indices_size = mesh->get_indices().size();
    mesh->set_device_buffer_info({
      .vertex_offset = vertex_buffer_size,
      .index_offset = index_buffer_size,
      .vertex_size = mesh_vertices_size,
      .index_size = mesh_indices_size,
    });
    vertex_buffer_size += mesh_vertices_size;
    index_buffer_size += mesh_indices_size;

    mesh->transform_index = transform_index++; //
  }

  for (auto const& host_image : host_images) {
    total_image_size += host_image->getBytesize();
  }

#ifndef NDEBUG
  uint32_t const kMegabyte{ 1024u * 1024u };
  LOGI("> vertex buffer size %f Mb", vertex_buffer_size / static_cast<float>(kMegabyte));
  LOGI("> index buffer size %f Mb ", index_buffer_size / static_cast<float>(kMegabyte));
  LOGI("> total image size %f Mb ", total_image_size / static_cast<float>(kMegabyte));
#endif
}

// ----------------------------------------------------------------------------

void Resources::upload_images(Context const& context) {
  assert(total_image_size > 0);
  assert(allocator_ != nullptr);

  /* Create a staging buffer. */
  backend::Buffer staging_buffer{
    allocator_->create_staging_buffer( total_image_size ) //
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
    allocator_->write_buffer(
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
  LOG_CHECK(vertex_buffer_size > 0);
  LOG_CHECK(allocator_ != nullptr);

  /* Allocate device buffers for meshes & their transforms. */
  vertex_buffer = allocator_->create_buffer(
    vertex_buffer_size,
    VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
    VMA_MEMORY_USAGE_GPU_ONLY
  );

  if (index_buffer_size > 0) {
    index_buffer = allocator_->create_buffer(
      index_buffer_size,
      VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
      VMA_MEMORY_USAGE_GPU_ONLY
    );
  }

  // Meshes transforms buffer.
  size_t const transforms_buffer_size{ meshes.size() * sizeof(mat4) };
  {
    // We assume most meshes would be static, so with unfrequent updates.
    transforms_ssbo_ = allocator_->create_buffer(
      transforms_buffer_size,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY
    );
    // Update the transform SSBO DescriptorSet entry for all MaterialFx.
    material_fx_registry_->update_transforms_ssbo(transforms_ssbo_);
  }

  /* Copy host mesh data to the staging buffer. */
  backend::Buffer staging_buffer{
    allocator_->create_staging_buffer(vertex_buffer_size + index_buffer_size + transforms_buffer_size)
  };
    {
    size_t vertex_offset{0u};
    size_t index_offset{vertex_buffer_size};
    size_t transform_offset{vertex_buffer_size + index_buffer_size};

    std::byte* device_data{};
    allocator_->map_memory(staging_buffer, (void**)&device_data);
    for (auto const& mesh : meshes) {
      auto const& vertices = mesh->get_vertices();
      memcpy(device_data + vertex_offset, vertices.data(), vertices.size());
      vertex_offset += vertices.size();

      if (index_buffer_size > 0) {
        auto const& indices = mesh->get_indices();
        memcpy(device_data + index_offset, indices.data(), indices.size());
        index_offset += indices.size();
      }

      memcpy(device_data + transform_offset, lina::ptr(mesh->world_matrix), sizeof(mat4));
      transform_offset += sizeof(mat4);
    }
    allocator_->unmap_memory(staging_buffer);
  }

  /* Copy device data from staging buffers to their respective buffers. */
  auto cmd = context.create_transient_command_encoder(Context::TargetQueue::Transfer);
  {
    size_t src_offset{0};

    src_offset = cmd.copy_buffer(staging_buffer, src_offset, vertex_buffer, 0u, vertex_buffer_size);

    if (index_buffer_size > 0) {
      src_offset = cmd.copy_buffer(staging_buffer, src_offset, index_buffer, 0u, index_buffer_size);
    }

    src_offset = cmd.copy_buffer(staging_buffer, src_offset, transforms_ssbo_, 0u, transforms_buffer_size);

    std::vector<VkBufferMemoryBarrier2> barriers{
      {
        .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        .buffer = vertex_buffer.buffer,
        .size = vertex_buffer_size,
      },
      {
        .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, //
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .buffer = transforms_ssbo_.buffer,
        .size = transforms_buffer_size,
      },
    };
    if (index_buffer_size > 0) {
      barriers.emplace_back(VkBufferMemoryBarrier2{
        .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        .buffer = index_buffer.buffer,
        .size = index_buffer_size,
      });
    }
    cmd.pipeline_buffer_barriers(barriers);
  }

  context.finish_transient_command_encoder(cmd);
}

}  // namespace scene

/* -------------------------------------------------------------------------- */
