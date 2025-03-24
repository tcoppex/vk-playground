/* -------------------------------------------------------------------------- */

#include "framework/scene/skybox.h"

#include "framework/backend/context.h"
#include "framework/renderer/renderer.h"
#include "framework/scene/camera.h"

/* -------------------------------------------------------------------------- */

void Skybox::init(Context const& context, Renderer const& renderer) {
  envmap_.init(context, renderer);

  /* Precalculate the BRDF LUT. */
  compute_brdf_lut(); //

  /* internal sampler */
  {
    VkSamplerCreateInfo const sampler_create_info{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      // .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, //
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, //
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .anisotropyEnable = VK_FALSE,
    };
    CHECK_VK( vkCreateSampler(context.get_device(), &sampler_create_info, nullptr, &sampler_) );
  }

  /* Create the skybox geometry on the device. */
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

  /* Shared descriptor sets */
  {
    descriptor_set_layout_ = renderer.create_descriptor_set_layout({
      {
        .binding = shader_interop::skybox::kDescriptorSetBinding_Sampler,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1u,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
    });

    descriptor_set_ = renderer.create_descriptor_set(descriptor_set_layout_, {
      {
        .binding = shader_interop::skybox::kDescriptorSetBinding_Sampler,
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .images = {
          {
            .sampler = sampler_, //
            .imageView = envmap_.get_image(Envmap::ImageType::Diffuse).view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          }
        }
      }
    });
  }

  pipeline_layout_ = renderer.create_pipeline_layout({
    .setLayouts = { descriptor_set_layout_ },
    .pushConstantRanges = {
      {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                    | VK_SHADER_STAGE_FRAGMENT_BIT
                    ,
        .size = sizeof(push_constant_),
      }
    },
  });

  /* Create the render pipeline */
  {
    auto shaders{context.create_shader_modules(FRAMEWORK_COMPILED_SHADERS_DIR "skybox/", {
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
}

// ----------------------------------------------------------------------------

void Skybox::release(Context const& context, Renderer const& renderer) {
  renderer.destroy_pipeline(graphics_pipeline_);
  renderer.destroy_pipeline_layout(pipeline_layout_);
  renderer.destroy_descriptor_set_layout(descriptor_set_layout_);

  std::shared_ptr<ResourceAllocator> allocator = context.get_resource_allocator();
  allocator->destroy_buffer(index_buffer_);
  allocator->destroy_buffer(vertex_buffer_);

  vkDestroySampler(context.get_device(), sampler_, nullptr); //

  envmap_.release();
}

// ----------------------------------------------------------------------------

bool Skybox::setup(std::string_view hdr_filename) {
  return envmap_.setup(hdr_filename);
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
    pass.push_constant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    pass.bind_vertex_buffer(vertex_buffer_);
    pass.bind_index_buffer(index_buffer_, cube_.get_vk_index_type());
    pass.draw_indexed(cube_.get_index_count());
  }
}

// ----------------------------------------------------------------------------

void Skybox::compute_brdf_lut() {
#if 0
  /* The BRDF LUT is a simple 2D texture of RG16F */
  VkImageCreateInfo const image_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = 0,
    .imageType = VK_IMAGE_TYPE_2D,
    .arrayLayers = 1u,
    .format = VK_FORMAT_R16G16_SFLOAT,
    .extent = { kBRDFLutResolution, kBRDFLutResolution, 1u },
    .mipLevels = 1u,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_STORAGE_BIT
           | VK_IMAGE_USAGE_SAMPLED_BIT
           ,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  VkImageViewCreateInfo const view_info{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = image_info.format,
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

  allocator->create_image_with_view(image_info, view_info, &brdf_lut_);
#endif

  // TODO
}

/* -------------------------------------------------------------------------- */