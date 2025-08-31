#include "framework/scene/resources.h"

#include "framework/core/camera.h"
#include "framework/renderer/renderer.h"
#include "framework/renderer/fx/material/material_fx.h"

#include "framework/shaders/material/interop.h" //
#include "framework/scene/private/gltf_loader.h" //

static constexpr bool kUseAsyncLoad{true};

/* -------------------------------------------------------------------------- */

namespace scene {

Resources::Resources(Renderer const& renderer)
  : renderer_ptr_(&renderer)
  , context_ptr_(&renderer.context())
{}

// ----------------------------------------------------------------------------

Resources::~Resources() {
  if (allocator_ptr_ != nullptr) {
    for (auto& img : device_images) {
      allocator_ptr_->destroy_image(&img);
    }
    allocator_ptr_->destroy_buffer(transforms_ssbo_);
    allocator_ptr_->destroy_buffer(frame_ubo_);
    allocator_ptr_->destroy_buffer(index_buffer);
    allocator_ptr_->destroy_buffer(vertex_buffer);
  }
  if (material_fx_registry_) {
    material_fx_registry_->release();
  }
}

// ----------------------------------------------------------------------------

void Resources::setup() {
  HostResources::setup();

  material_fx_registry_ = std::make_unique<MaterialFxRegistry>();
  material_fx_registry_->init(*renderer_ptr_);
}

// ----------------------------------------------------------------------------

bool Resources::load_file(std::string_view filename) {
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

    // Reserve data.
    samplers.reserve(data->samplers_count + samplers.size());
    host_images.reserve(data->images_count + host_images.size());
    textures.reserve(data->textures_count + textures.size());
    material_refs.reserve(data->materials_count + material_refs.size());
    skeletons.reserve(data->skins_count + skeletons.size());
    meshes.reserve(data->meshes_count + meshes.size());
    transforms.reserve(data->meshes_count + transforms.size());
    animations_map.reserve(data->animations_count);

    // (device specific, to separate from this)
    //--------------------
    LOG_CHECK( material_fx_registry_ != nullptr );
    PreprocessMaterials(data, *material_fx_registry_);
    material_fx_registry_->setup();
    //--------------------

    if constexpr (kUseAsyncLoad)
    {
      /* --- Async tasks version --- */

      auto run_task = utils::RunTaskGeneric<void>;
      auto run_task_ret = utils::RunTaskGeneric<PointerToIndexMap_t>;
      auto run_task_sampler = utils::RunTaskGeneric<PointerToSamplerMap_t>;

      auto taskSamplers = run_task_sampler([data, &samplers = this->samplers] {
        return ExtractSamplers(data, samplers);
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
        &material_refs = this->material_refs,
        &material_fx_registry = *(this->material_fx_registry_),
        &default_bindings = this->default_bindings_
      ] {
        auto textures_indices = taskTextures.get();
        return ExtractMaterials(
          data, textures_indices, material_refs, material_fx_registry, default_bindings
        );
      });

      auto skeletons_indices = taskSkeletons.get();

      auto taskAnimations = run_task([data, &skeletons_indices, &skeletons = this->skeletons] {
        // ExtractAnimations(data, basename, skeletons_indices, skeletons, animations_map);
      });

      auto taskMeshes = run_task([
        &taskMaterials,
        data,
        &skeletons_indices,
        &material_refs = this->material_refs,
        &skeletons = this->skeletons,
        &meshes = this->meshes,
        &transforms = this->transforms
      ] {
        auto materials_indices = taskMaterials.get();
        ExtractMeshes(
          data, materials_indices, material_refs, skeletons_indices,
          skeletons, meshes, transforms, kRestructureAttribs
        );
      });

      taskAnimations.get();
      taskMeshes.get();
    }
    else
    {
      /* --- Serialized version --- */

      auto samplers_lut       = ExtractSamplers(data, samplers);
      auto skeletons_indices  = ExtractSkeletons(data, skeletons);
      auto images_indices     = ExtractImages(data, host_images);
      auto textures_indices   = ExtractTextures(
        data, images_indices, samplers_lut, textures
      );
      auto materials_indices  = ExtractMaterials(
        data, textures_indices, material_refs, *material_fx_registry_, default_bindings_
      );
      ExtractMeshes(
        data, materials_indices, material_refs, skeletons_indices,
        skeletons, meshes, transforms, kRestructureAttribs
      );
    }

    /* Wait for the host images to finish loading before using them. */
    for (auto & host_image : host_images) {
      host_image.getLoadAsyncResult();
    }
  }

  /* Be sure to have finished loading all images before freeing gltf data */
  cgltf_free(data);

  reset_internal_descriptors();

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

  // uint32_t const kMegabyte{ 1024u * 1024u };
  // LOGI("> vertex buffer size %f Mb", vertex_buffer_size / static_cast<float>(kMegabyte));
  // LOGI("> index buffer size %f Mb ", index_buffer_size / static_cast<float>(kMegabyte));
  // LOGI("> total image size %f Mb ", total_image_size / static_cast<float>(kMegabyte));
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
    // for (auto& mesh : meshes) { mesh->recalculate_tangents(); } //
  }
  // --------------------
}

// ----------------------------------------------------------------------------

void Resources::upload_to_device(bool const bReleaseHostDataOnUpload) {
  if (!allocator_ptr_) {
    allocator_ptr_ = context_ptr_->allocator_ptr();
  }

  /* Create the shared Frame UBO */
  frame_ubo_ = allocator_ptr_->create_buffer(
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
    upload_images(*context_ptr_);

    material_fx_registry_->update_texture_atlas([this](uint32_t binding) {
      return this->descriptor_set_texture_atlas_entry(binding);
    });
  }

  /* Transfer Buffers */
  if (vertex_buffer_size > 0) {
    upload_buffers(*context_ptr_);
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

  auto const& sampler_pool = renderer_ptr_->sampler_pool();

  for (auto const& texture : textures) {
    auto const& img = device_images.at(texture.channel_index());
    texture_atlas_entry.images.push_back({
      .sampler = sampler_pool.convert(texture.sampler),
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

  /// ---------------------------------------
  /// DevNote
  /// We could improve the overall sorting by using a generic 64bits sorting
  /// key (pipeline << 32) | (material << 16) | depthBits), using a single buffer.
  /// ---------------------------------------

  // Retrieve submeshes associated to each MaterialFx.
  if constexpr (true) {
    lookups_ = {};
    for (auto const& mesh : meshes) {
      for (auto const& submesh : mesh->submeshes) {
        if (auto matref = submesh.material_ref; matref) {
          auto const alpha_mode = matref->states.alpha_mode;
          auto fx = material_fx_registry_->material_fx(*matref);
          auto hashpair = std::make_pair(fx, matref->states);
          lookups_[alpha_mode][hashpair].emplace_back(&submesh);
        }
      }
    }
    //reset_scene_lookups = false;
  }

  // Sort each buffer of submeshes.

  using SortKey = std::pair<float, size_t>; // (depthProxy, index)
  std::vector<SortKey> sortkeys{};
  vec3 const camera_dir{camera.direction()};
  SubMeshBuffer swap_buffer{};

  auto sort_submeshes = [&](SubMeshBuffer &submeshes, auto comp) {
    sortkeys = {};
    sortkeys.reserve(submeshes.size());
    for (size_t i = 0; i < submeshes.size(); ++i) {
      mat4 const& world = submeshes[i]->parent->world_matrix();
      vec3 const pos = lina::to_vec3(world.w);
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
  for (auto& [_, submeshes] : lookups_[MaterialStates::AlphaMode::Mask]) {
    sort_submeshes(submeshes, std::less{});
  }
  // Sort back to front for alpha blending.
  for (auto& [_, submeshes] : lookups_[MaterialStates::AlphaMode::Blend]) {
    sort_submeshes(submeshes, std::greater{});
  }
}

// ----------------------------------------------------------------------------

void Resources::render(RenderPassEncoder const& pass) {
  LOG_CHECK( material_fx_registry_ != nullptr );
  // Render each Fx.
  uint32_t instance_index = 0u;
  for (auto& lookup : lookups_) {
    for (auto& [ hashpair, submeshes] : lookup) {
      auto [fx, states] = hashpair;

      // Bind pipeline & descriptor set.
      // auto const& states = submeshes[0]->material_ref->states;
      fx->prepareDrawState(pass, states);

      // Draw submeshes.
      for (auto submesh : submeshes) {
        auto mesh = submesh->parent;

        // Submesh's pushConstants.
        fx->setTransformIndex(mesh->transform_index);
        fx->setMaterialIndex(submesh->material_ref->material_index);
        fx->setInstanceIndex(instance_index++); //
        fx->pushConstant(pass);

        pass.set_primitive_topology(mesh->vk_primitive_topology());
        pass.draw(submesh->draw_descriptor, vertex_buffer, index_buffer); //
      }
    }
  }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void Resources::upload_images(Context const& context) {
  assert(total_image_size > 0);
  assert(allocator_ptr_ != nullptr);

  /* Create a staging buffer. */
  backend::Buffer staging_buffer{
    allocator_ptr_->create_staging_buffer( total_image_size ) //
  };

  device_images.reserve(host_images.size()); //

  std::vector<VkBufferImageCopy> copies{};
  copies.reserve(host_images.size());

  // uint32_t channel_index = 0u;
  uint64_t staging_offset = 0u;
  for (auto const& host_image : host_images) {
    VkExtent3D const extent{
      .width = static_cast<uint32_t>(host_image.width),
      .height = static_cast<uint32_t>(host_image.height),
      .depth = 1u,
    };
    device_images.push_back(context.create_image_2d(
      extent.width,
      extent.height,
      VK_FORMAT_R8G8B8A8_UNORM, //
      VK_IMAGE_USAGE_TRANSFER_DST_BIT
    ));

    /* Upload image to staging buffer */
    uint32_t const img_bytesize{ host_image.getBytesize() };
    allocator_ptr_->write_buffer(
      staging_buffer, staging_offset, host_image.getPixels(), 0u, img_bytesize
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
  LOG_CHECK(allocator_ptr_ != nullptr);

  /* Allocate device buffers for meshes & their transforms. */
  vertex_buffer = allocator_ptr_->create_buffer(
    vertex_buffer_size,
    VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
    VMA_MEMORY_USAGE_GPU_ONLY
  );

  if (index_buffer_size > 0) {
    index_buffer = allocator_ptr_->create_buffer(
      index_buffer_size,
      VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR,
      VMA_MEMORY_USAGE_GPU_ONLY
    );
  }

  // Meshes transforms buffer.
  size_t const transforms_buffer_size{ transforms.size() * sizeof(transforms[0]) };
  {
    // We assume most meshes would be static, so with unfrequent updates.
    transforms_ssbo_ = allocator_ptr_->create_buffer(
      transforms_buffer_size,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY
    );
    // Update the transform SSBO DescriptorSet entry for all MaterialFx.
    material_fx_registry_->update_transforms_ssbo(transforms_ssbo_);
  }

  /* Copy host mesh data to the staging buffer. */
  backend::Buffer staging_buffer{
    allocator_ptr_->create_staging_buffer(vertex_buffer_size + index_buffer_size + transforms_buffer_size)
  };
  {
    size_t vertex_offset{0u};
    size_t index_offset{vertex_buffer_size};

    // Transfer the attributes & indices by ranges.
    std::byte* device_data{};
    allocator_ptr_->map_memory(staging_buffer, (void**)&device_data);
    for (auto const& mesh : meshes) {
      auto const& vertices = mesh->get_vertices();
      memcpy(device_data + vertex_offset, vertices.data(), vertices.size());
      vertex_offset += vertices.size();

      if (index_buffer_size > 0) {
        auto const& indices = mesh->get_indices();
        memcpy(device_data + index_offset, indices.data(), indices.size());
        index_offset += indices.size();
      }
    }

    // Transfer the transforms buffer in one go.
    memcpy(
      device_data + vertex_buffer_size + index_buffer_size,
      transforms.data(),
      transforms_buffer_size
    );

    allocator_ptr_->unmap_memory(staging_buffer);
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
