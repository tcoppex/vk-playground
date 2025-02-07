/* -------------------------------------------------------------------------- */
//
//    04 - Hello Texture
//
//    Where we put some interpolated crabs on.
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"
#include "framework/utils/geometry.h"

namespace shader_interop {
#include "shaders/interop.h"
}

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
 public:
  using HostData_t = shader_interop::UniformData;

 public:
  SampleApp() = default;
  ~SampleApp() {}

 private:
  bool setup() final {
    wm_->setTitle("04 - خوراي ، كىشىلەر ماڭا دىققەت قىلىۋاتىدۇ");

    renderer_.set_color_clear_value({{ 0.94f, 0.93f, 0.94f, 1.0f }});

    allocator_ = context_.get_resource_allocator();

    /* Initialize the scene data. */
    host_data_.scene.camera = {
      .viewMatrix = linalg::lookat_matrix(
        vec3f(1.0f, 2.0f, 3.0f),
        vec3f(0.0f, 0.0f, 0.0f),
        vec3f(0.0f, 1.0f, 0.0f)
      ),
      .projectionMatrix = linalg::perspective_matrix(
        lina::radians(60.0f),
        static_cast<float>(viewport_size_.width) / static_cast<float>(viewport_size_.height),
        0.01f,
        500.0f,
        linalg::neg_z,
        linalg::zero_to_one
      ),
    };

    /* Create a cube mesh procedurally on the host.
     * We have no need to keep it on memory after initialization so we just
     * save the attribute we need. */
    Geometry cube_geo;
    {
      Geometry::MakeCube(cube_geo);
      index_type_ = cube_geo.get_vk_index_type();
      index_count_ = cube_geo.get_index_count();
    }

    /* Create Buffers & Image(s). */
    {
      auto cmd = context_.create_transient_command_encoder();

      uniform_buffer_ = cmd.create_buffer_and_upload(
        &host_data_, sizeof(host_data_),
        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT
      );

      /* Transfer the cube geometry (vertices attributes & indices) to the device. */
      vertex_buffer_ = cmd.create_buffer_and_upload(
        cube_geo.get_vertices(),
        VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT
      );
      index_buffer_ = cmd.create_buffer_and_upload(
        cube_geo.get_indices(),
        VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT
      );

      /* Load a texture using the current transient command encoder. */
      if (std::string fn{ASSETS_DIR "textures/whynot.png"}; !renderer_.load_texture_2d(cmd, fn, image_)) {
        fprintf(stderr, "The texture image '%s' could not be found.\n", fn.c_str());
      }

      context_.finish_transient_command_encoder(cmd);
    }

    /* Alternatively, the texture could have been loaded directly using an
     * internal transient command encoder. */
    // renderer_.load_texture_2d(path_to_texture, image_);

    /* Descriptor set. */
    {
      descriptor_set_layout_ = renderer_.create_descriptor_set_layout({
        {
          .binding = shader_interop::kDescriptorSetBinding_UniformBuffer,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1u,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                        | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                        | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
        },
        {
          .binding = shader_interop::kDescriptorSetBinding_Sampler,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1u,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        },
      });

      descriptor_set_ = renderer_.create_descriptor_set(descriptor_set_layout_, {
        {
          .binding = shader_interop::kDescriptorSetBinding_UniformBuffer,
          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .resource = { .buffer = { uniform_buffer_.buffer } }
        },
        {
          .binding = shader_interop::kDescriptorSetBinding_Sampler,
          .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .resource = {
            .image = {
              .sampler = renderer_.get_default_sampler(),
              .imageView = image_.view,
              .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            }
          }
        }
      });
    }

    auto shaders{context_.create_shader_modules(COMPILED_SHADERS_DIR, {
      "vs_simple.glsl",
      "fs_simple.glsl",
    })};

    /* Setup the graphics pipeline. */
    {
      VkPipelineLayout const pipeline_layout = renderer_.create_pipeline_layout({
        .setLayouts = { descriptor_set_layout_ },
        .pushConstantRanges = {
          {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .size = sizeof(shader_interop::PushConstant),
          }
        },
      });

      graphics_pipeline_ = renderer_.create_graphics_pipeline(pipeline_layout, {
        .vertex = {
          .module = shaders[0u].module,
          .buffers = {
            {
              .stride = cube_geo.get_stride(),
              .attributes =  cube_geo.get_vk_binding_attributes(0u, {
                { Geometry::AttributeType::Position, shader_interop::kAttribLocation_Position },
                { Geometry::AttributeType::Texcoord, shader_interop::kAttribLocation_Texcoord },
                { Geometry::AttributeType::Normal, shader_interop::kAttribLocation_Normal },
              }),
            }
          }
        },
        .fragment = {
          .module = shaders[1u].module,
          .targets = {
            {
              .format = renderer_.get_color_attachment().format,
              .writeMask = VK_COLOR_COMPONENT_R_BIT
                         | VK_COLOR_COMPONENT_G_BIT
                         | VK_COLOR_COMPONENT_B_BIT
                         | VK_COLOR_COMPONENT_A_BIT
                         ,
            }
          },
        },
        .depthStencil = {
          .format = renderer_.get_depth_stencil_attachment().format,
          .depthTestEnable = VK_TRUE,
          .depthWriteEnable = VK_TRUE,
          .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        },
        .primitive = {
          .topology = cube_geo.get_vk_primitive_topology(),
          .cullMode = VK_CULL_MODE_BACK_BIT,
        }
      });
    }

    context_.release_shader_modules(shaders);

    return true;
  }

  void release() final {
    renderer_.destroy_descriptor_set_layout(descriptor_set_layout_);
    renderer_.destroy_pipeline_layout(graphics_pipeline_.get_layout());
    renderer_.destroy_pipeline(graphics_pipeline_);

    allocator_->destroy_image(&image_);

    allocator_->destroy_buffer(index_buffer_);
    allocator_->destroy_buffer(vertex_buffer_);
    allocator_->destroy_buffer(uniform_buffer_);
  }

  void frame() final {
    /* Update the world matrix. */
    {
      float const frame_time{ get_frame_time() };

      // rotation_matrix_axis automatically normalize the given axis.
      push_constant_.model.worldMatrix = lina::rotation_matrix_axis(
        vec3(3.0f * frame_time, 0.8f, sinf(frame_time)),
        frame_time * 0.62f
      );
    }

    auto cmd = renderer_.begin_frame();
    {
      auto pass = cmd.begin_rendering();
      {
        pass.set_viewport_scissor(viewport_size_);

        pass.bind_pipeline(graphics_pipeline_);
        {
          pass.push_constant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT);
          pass.bind_descriptor_set(descriptor_set_, VK_SHADER_STAGE_VERTEX_BIT);

          pass.bind_vertex_buffer(vertex_buffer_);
          pass.bind_index_buffer(index_buffer_, index_type_);
          pass.draw_indexed(index_count_);
        }
      }
      cmd.end_rendering();
    }
    renderer_.end_frame();
  }

 private:
  std::shared_ptr<ResourceAllocator> allocator_{};

  HostData_t host_data_{};
  VkIndexType index_type_{};
  uint32_t index_count_{};

  Buffer_t uniform_buffer_{};

  Buffer_t vertex_buffer_{};
  Buffer_t index_buffer_{};

  Image_t image_{};

  VkDescriptorSetLayout descriptor_set_layout_{};
  VkDescriptorSet descriptor_set_{};
  shader_interop::PushConstant push_constant_{};

  Pipeline graphics_pipeline_{};
};

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  return SampleApp().run();
}

/* -------------------------------------------------------------------------- */
