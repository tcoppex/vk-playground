/* -------------------------------------------------------------------------- */

#include "skybox.h"

#include "framework/backend/context.h"
#include "framework/renderer/renderer.h"
#include "framework/scene/camera.h"

/* -------------------------------------------------------------------------- */

void Skybox::init(Context const& context, Renderer const& renderer) {
  /* Create Buffers & Image(s). */
  {
    Geometry::MakeCube(cube_);

    cube_.initialize_submesh_descriptors({
      { Geometry::AttributeType::Position, shader_interop::skybox::kAttribLocation_Position },
    });

    auto cmd = context.create_transient_command_encoder();

    vertex_buffer_ = cmd.create_buffer_and_upload(
      cube_.get_vertices(),
      VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT
    );
    index_buffer_ = cmd.create_buffer_and_upload(
      cube_.get_indices(),
      VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT
    );

    context.finish_transient_command_encoder(cmd);
    cube_.clear_indices_and_vertices();
  }

  /* Create the HDR envmaps & the BRDF LUT. */
  {
    std::shared_ptr<ResourceAllocator> allocator{ context.get_resource_allocator() };

    VkImageCreateInfo image_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, //
      .imageType = VK_IMAGE_TYPE_2D,
      .mipLevels = 1u, //
      .arrayLayers = 6u,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_STORAGE_BIT
             | VK_IMAGE_USAGE_SAMPLED_BIT
             ,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImageViewCreateInfo view_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
      .components = {
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A,
      },
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0u,
        .levelCount = image_info.mipLevels,
        .baseArrayLayer = 0u,
        .layerCount = image_info.arrayLayers,
      },
    };

    image_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    view_info.format = image_info.format;

    image_info.extent = { kDiffuseEnvmapResolution, kDiffuseEnvmapResolution, 1u };
    allocator->create_image_with_view(image_info, view_info, &diffuse_envmap_);

    image_info.extent = { kIrradianceEnvmapResolution, kIrradianceEnvmapResolution, 1u };
    allocator->create_image_with_view(image_info, view_info, &irradiance_envmap_);

    image_info.extent = { kSpecularEnvmapResolution, kSpecularEnvmapResolution, 1u };
    image_info.mipLevels = kSpecularEnvmapLevelCount;
    view_info.subresourceRange.levelCount = image_info.mipLevels;
    // view_info.subresourceRange.baseMipLevel = 2u;
    allocator->create_image_with_view(image_info, view_info, &specular_envmap_);

    /* The BRDF LUT is a simple 2D texture of RG16F */
    image_info.flags = 0;
    image_info.format = VK_FORMAT_R16G16_SFLOAT;
    image_info.extent = { kBRDFLutResolution, kBRDFLutResolution, 1u };
    image_info.arrayLayers = 1u;
    image_info.mipLevels = 1u;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = image_info.format;
    view_info.subresourceRange.levelCount = image_info.mipLevels;
    view_info.subresourceRange.layerCount = image_info.arrayLayers;
    allocator->create_image_with_view(image_info, view_info, &brdf_lut_);
  }

  /* Shared descriptor sets */
  {
    VkDescriptorBindingFlags const kDefaultDescBindingFlags{
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
      | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
      | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    };

    descriptor_set_layout_ = renderer.create_descriptor_set_layout({
      {
        .binding = shader_interop::skybox::kDescriptorSetBinding_Sampler,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = kDefaultDescBindingFlags,
      },
      {
        .binding = shader_interop::skybox::kDescriptorSetBinding_StorageImage,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .bindingFlags = kDefaultDescBindingFlags,
      },
      {
        .binding = shader_interop::skybox::kDescriptorSetBinding_StorageImageArray,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = kSpecularEnvmapLevelCount,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .bindingFlags = kDefaultDescBindingFlags,
      },
    });

    descriptor_set_ = renderer.create_descriptor_set(descriptor_set_layout_, {
      {
        .binding = shader_interop::skybox::kDescriptorSetBinding_StorageImage,
        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .images = {
          {
            .sampler = VK_NULL_HANDLE,
            .imageView = diffuse_envmap_.view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
          }
        }
      },
    });
  }

  pipeline_layout_ = renderer.create_pipeline_layout({
    .setLayouts = { descriptor_set_layout_ },
    .pushConstantRanges = {
      {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                    | VK_SHADER_STAGE_FRAGMENT_BIT
                    | VK_SHADER_STAGE_COMPUTE_BIT
                    ,
        .offset = 0u,
        .size = sizeof(push_constant_),
      }
    },
  });

  /* Create the compute pipelines. */
  {
    auto shaders{context.create_shader_modules(COMPILED_SHADERS_DIR "skybox/", {
      "spherical_to_cubemap.comp.glsl",
      "integrate_brdf.comp.glsl",
      "irradiance_convolution.comp.glsl",
      "specular_convolution.comp.glsl",
    })};
    renderer.create_compute_pipelines(pipeline_layout_, shaders, compute_pipelines_.data());
    context.release_shader_modules(shaders);
  }

  /* Create the render pipeline */
  {
    auto shaders{context.create_shader_modules(COMPILED_SHADERS_DIR "skybox/", {
      "skybox.vert.glsl",
      "skybox.frag.glsl",
    })};

    /* Setup the graphics pipeline. */
    {
      graphics_pipeline_ = renderer.create_graphics_pipeline(pipeline_layout_, {
        .vertex = {
          .module = shaders[0u].module,
          .buffers = cube_.get_vk_pipeline_vertex_buffer_descriptors(),
        },
        .fragment = {
          .module = shaders[1u].module,
          .targets = {
            {
              .writeMask = VK_COLOR_COMPONENT_R_BIT
                         | VK_COLOR_COMPONENT_G_BIT
                         | VK_COLOR_COMPONENT_B_BIT
                         | VK_COLOR_COMPONENT_A_BIT
                         ,
            }
          },
        },
        .depthStencil = {
          .depthTestEnable = VK_TRUE,
          // .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        },
        .primitive = {
          .topology = cube_.get_vk_primitive_topology(),
          .cullMode = VK_CULL_MODE_NONE,
        }
      });
    }

    context.release_shader_modules(shaders);
  }

  compute_brdf_lut();
}

// ----------------------------------------------------------------------------

void Skybox::release() {
  for (auto pipeline : compute_pipelines_) {
    // renderer_.destroy_pipeline(pipeline);
  }
  // renderer_.destroy_pipeline(graphics_pipeline_);
  // renderer_.destroy_pipeline_layout(pipeline_layout_);
  // renderer_.destroy_descriptor_set_layout(descriptor_set_layout_);
}

// ----------------------------------------------------------------------------

bool Skybox::setup(Context const& context, Renderer const& renderer, std::string_view hdr_filename) {
  backend::Image spherical_envmap{};

  if (!renderer.load_image_2d(hdr_filename, spherical_envmap)) {
    LOGE("Fail to load spherical map \"%s\".", hdr_filename.data());
    return false;
  }

  renderer.update_descriptor_set(descriptor_set_, {
    {
      .binding = shader_interop::skybox::kDescriptorSetBinding_Sampler,
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .images = {
        {
          .sampler = renderer.get_default_sampler(), //
          .imageView = spherical_envmap.view,
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }
      }
    }
  });

  //------------------

  auto cmd = context.create_transient_command_encoder();
  {

    {
      VkImageMemoryBarrier2 const textureMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,

        .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        .srcAccessMask = 0,

        .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,

        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, //
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = diffuse_envmap_.image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
      };

      VkDependencyInfo const dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1u,
        .pImageMemoryBarriers = &textureMemoryBarrier,
      };

      vkCmdPipelineBarrier2(cmd.get_handle(), &dependency);
    }

    //----------

    cmd.bind_descriptor_set(descriptor_set_, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT);

    push_constant_.mapResolution = kDiffuseEnvmapResolution; //
    cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);

    cmd.bind_pipeline(compute_pipelines_.at(Compute_TransformSpherical));

    cmd.dispatch<
      shader_interop::skybox::kCompute_SphericalTransform_kernelSize_x,
      shader_interop::skybox::kCompute_SphericalTransform_kernelSize_y
    >(push_constant_.mapResolution, push_constant_.mapResolution, 6u);

    //----------

    {
      VkImageMemoryBarrier2 const textureMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,

        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,

        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,

        .oldLayout = VK_IMAGE_LAYOUT_GENERAL, //
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = diffuse_envmap_.image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
      };

      VkDependencyInfo const dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1u,
        .pImageMemoryBarriers = &textureMemoryBarrier,
      };

      vkCmdPipelineBarrier2(cmd.get_handle(), &dependency);
    }
  }
  context.finish_transient_command_encoder(cmd);


  //------------------

  renderer.update_descriptor_set(descriptor_set_, {
    {
      .binding = shader_interop::skybox::kDescriptorSetBinding_Sampler,
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .images = {
        {
          .sampler = renderer.get_default_sampler(), //
          .imageView = diffuse_envmap_.view,
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }
      }
    }
  });

  compute_irradiance(context, renderer); //

  compute_specular(context, renderer); //

  // // [debug]
  renderer.update_descriptor_set(descriptor_set_, {
    {
      .binding = shader_interop::skybox::kDescriptorSetBinding_Sampler,
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .images = {
        {
          .sampler = renderer.get_default_sampler(), //
          .imageView =
              // irradiance_envmap_.view,
              specular_envmap_.view,
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        }
      }
    }
  });

  return true;
}

// ----------------------------------------------------------------------------

void Skybox::render(RenderPassEncoder & pass, Camera const& camera) {
  mat4 view{ camera.view() };
  view[3] = vec4(vec3(0.0f), view[3].w);
  push_constant_.viewProjectionMatrix = linalg::mul(camera.proj(), view);
  push_constant_.hdrIntensity = 1.0;

  pass.bind_pipeline(graphics_pipeline_);
  {
    pass.bind_descriptor_set(descriptor_set_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    pass.push_constant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);

    pass.bind_vertex_buffer(vertex_buffer_);
    pass.bind_index_buffer(index_buffer_, cube_.get_vk_index_type());
    pass.draw_indexed(cube_.get_index_count());
  }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void Skybox::compute_irradiance(Context const& context, Renderer const& renderer) {

  renderer.update_descriptor_set(descriptor_set_, {
    {
      .binding = shader_interop::skybox::kDescriptorSetBinding_StorageImage,
      .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .images = {
        {
          .imageView = irradiance_envmap_.view,
          .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        }
      }
    }
  });

  auto cmd = context.create_transient_command_encoder();
  {
    {
      VkImageMemoryBarrier2 const textureMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,

        .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        .srcAccessMask = 0,

        .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,

        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, //
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = irradiance_envmap_.image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
      };

      VkDependencyInfo const dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1u,
        .pImageMemoryBarriers = &textureMemoryBarrier,
      };

      vkCmdPipelineBarrier2(cmd.get_handle(), &dependency);
    }

    //----------

    cmd.bind_descriptor_set(descriptor_set_, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT);

    push_constant_.mapResolution = kIrradianceEnvmapResolution; //
    push_constant_.numSamples = kIrradianceEnvmapSampleCount; //
    cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);

    cmd.bind_pipeline(compute_pipelines_.at(Compute_Irradiance));

    cmd.dispatch<
      shader_interop::skybox::kCompute_Irradiance_kernelSize_x,
      shader_interop::skybox::kCompute_Irradiance_kernelSize_y
    >(push_constant_.mapResolution, push_constant_.mapResolution, 6u);

    //----------

    {
      VkImageMemoryBarrier2 const textureMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,

        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,

        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,

        .oldLayout = VK_IMAGE_LAYOUT_GENERAL, //
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = irradiance_envmap_.image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }
      };

      VkDependencyInfo const dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1u,
        .pImageMemoryBarriers = &textureMemoryBarrier,
      };

      vkCmdPipelineBarrier2(cmd.get_handle(), &dependency);
    }
  }
  context.finish_transient_command_encoder(cmd);
}

// ----------------------------------------------------------------------------

void Skybox::compute_specular(Context const& context, Renderer const& renderer) {

  /*

  To work properly we might need to create an image view per mip levels, and or
  maybe use the VK_EXT_image_view_min_lod extension

  */


  VkImageViewMinLodCreateInfoEXT min_lod{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT,
    .minLod = 0.0f,
  };

  VkImageViewCreateInfo view_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = &min_lod,
    .image = specular_envmap_.image,
    .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
    .components = {
      VK_COMPONENT_SWIZZLE_R,
      VK_COMPONENT_SWIZZLE_G,
      VK_COMPONENT_SWIZZLE_B,
      VK_COMPONENT_SWIZZLE_A,
    },
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0u,
      .levelCount = 1,
      .baseArrayLayer = 0u,
      .layerCount = 6u,
    },
  };

  std::vector<VkDescriptorImageInfo> desc_image_infos(kSpecularEnvmapLevelCount, {.imageLayout = VK_IMAGE_LAYOUT_GENERAL});
  for (uint32_t level = 0u; level < kSpecularEnvmapLevelCount; ++level) {
    view_info.subresourceRange.baseMipLevel = level;
    min_lod.minLod = static_cast<float>(level);
    CHECK_VK(vkCreateImageView(context.get_device(), &view_info, nullptr, &desc_image_infos[level].imageView));
  }


  // ---------

  renderer.update_descriptor_set(descriptor_set_, {
    {
      .binding = shader_interop::skybox::kDescriptorSetBinding_StorageImageArray,
      .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .images = desc_image_infos,
    }
  });

  auto cmd = context.create_transient_command_encoder();
  {
    float constexpr kInvMaxLevel{
      (kSpecularEnvmapLevelCount <= 1) ? 1.0f : 1.0f / static_cast<float>(kSpecularEnvmapLevelCount - 1)
    };

    cmd.bind_descriptor_set(descriptor_set_, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT);
    cmd.bind_pipeline(compute_pipelines_.at(Compute_Specular));

    for (uint32_t level = 0u; level < kSpecularEnvmapLevelCount; ++level) {
      float const roughness = static_cast<float>(level) * kInvMaxLevel;

      push_constant_.mapResolution = kIrradianceEnvmapResolution >> level;
      push_constant_.numSamples = kSpecularEnvmapSampleCount;
      push_constant_.roughnessSquared = std::pow(roughness, 2.0f);
      push_constant_.mipLevel = level;

      cmd.push_constant(push_constant_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT
                                                        | VK_SHADER_STAGE_FRAGMENT_BIT
                                                        | VK_SHADER_STAGE_COMPUTE_BIT);

      /* Image barrier to the specific level. */
      {
        VkImageMemoryBarrier2 const textureMemoryBarrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,

          .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
          .srcAccessMask = 0,

          .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,

          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_GENERAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = specular_envmap_.image,
          .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 6 }
        };

        VkDependencyInfo const dependency{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1u,
          .pImageMemoryBarriers = &textureMemoryBarrier,
        };

        vkCmdPipelineBarrier2(cmd.get_handle(), &dependency);
      }

      cmd.dispatch<
        shader_interop::skybox::kCompute_Specular_kernelSize_x,
        shader_interop::skybox::kCompute_Specular_kernelSize_y
      >(push_constant_.mapResolution, push_constant_.mapResolution, 6u);
    }

    /* Render the specular envmap accessible to fragment shaders. */
    {
      VkImageMemoryBarrier2 const textureMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,

        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,

        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,

        .oldLayout = VK_IMAGE_LAYOUT_GENERAL, //
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = specular_envmap_.image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, kSpecularEnvmapLevelCount, 0, 6 }
      };

      VkDependencyInfo const dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1u,
        .pImageMemoryBarriers = &textureMemoryBarrier,
      };

      vkCmdPipelineBarrier2(cmd.get_handle(), &dependency);
    }
  }
  context.finish_transient_command_encoder(cmd);


  for (auto const& desc_image_info : desc_image_infos) {
    vkDestroyImageView(context.get_device(), desc_image_info.imageView, nullptr);
  }
}

// ----------------------------------------------------------------------------

void Skybox::compute_brdf_lut() {
  // // Setup the 2D texture.
  // constexpr int32_t kFormat     = GL_RG16F;
  // constexpr int32_t kResolution = 512;
  // constexpr int32_t kNumSamples = 1024;
  // int32_t const kLevels = Texture::GetMaxMipLevel(kResolution);

  // brdf_lut_map_ = TEXTURE_ASSETS.create2d(
  //   "skybox::integrate_brdf",
  //   kLevels, kFormat, kResolution, kResolution
  // );

  // // Setup the compute program.
  // auto pgm_handle = PROGRAM_ASSETS.createCompute(
  //   SHADERS_DIR "/skybox/cs_integrate_brdf.glsl"
  // );
  // auto const pgm = pgm_handle->id;

  // // Run the Kernel.
  // gx::SetUniform( pgm, "uResolution", kResolution);
  // gx::SetUniform( pgm, "uNumSamples", kNumSamples);

  // constexpr int32_t image_unit = 0;
  // glBindImageTexture( image_unit, brdf_lut_map_->id, 0, GL_FALSE, 0, GL_WRITE_ONLY, kFormat); //
  // gx::SetUniform( pgm, "uDstImg", image_unit);

  // gx::UseProgram(pgm);
  //   gx::DispatchCompute<16, 16>(kResolution, kResolution);
  // gx::UseProgram(0);

  // glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT); //

  // if (kLevels > 1) {
  //   brdf_lut_map_->generate_mipmaps();
  // }
}

/* -------------------------------------------------------------------------- */