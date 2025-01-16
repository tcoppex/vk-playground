/* -------------------------------------------------------------------------- */
//
//    02 - Hello Push Constants
//
//  Demonstrates how to uses PushConstant for quick update of uniform values.
//    - Add push constant range to pipeline layout.
//    - Push constant before binding a pipeline.
//    - Push constant after binding a pipeline.
//    - Set viewport-scissor with/without an offset.
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"
#include "framework/renderer/graphics_pipeline.h"

/* Constants & data structures shared between the device and host. */
namespace shader_interop {
#include "shaders/interop.h"
}

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
 public:
  static bool constexpr kFlipScreenVertically{ false };

  struct Vertex_t {
    float Position[4];
    float Color[4];
  };

  enum AttributeLocation {
    Position = 0,
    Color    = 1,
    kAttributeLocationCount
  };

  std::vector<Vertex_t> const kVertices{
    {.Position = { -0.7f, -0.4f, 0.0f, 1.0f}, .Color = {0.25f, 0.1f, 1.0f, 1.0f}},
    {.Position = {  0.7f, -0.4f, 0.0f, 1.0f}, .Color = {1.0f, 0.25f, 0.1f, 1.0f}},
    {.Position = {  0.0f,  0.8f, 0.0f, 1.0f}, .Color = {0.1f, 1.0f, 0.25f, 1.0f}},
  };

 public:
  SampleApp() = default;
  ~SampleApp() {}

 private:
  bool setup() final {
    glfwSetWindowTitle(window_, "02 - танцующий треугольник");

    renderer_.set_color_clear_value({.float32 = {0.60f, 0.65f, 0.55f, 1.0f}});

    {
      auto cmd = context_.create_transient_command_encoder();

      vertex_buffer_ = cmd.create_buffer_and_upload(std::span<Vertex_t const>(kVertices),
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
      );

      context_.finish_transient_command_encoder(cmd);
    }

    auto shaders{context_.create_shader_modules(COMPILED_SHADERS_DIR, {
      "vs_simple.glsl",
      "fs_simple.glsl",
    })};

    /* Setup the graphics pipeline. */
    {
      auto& gp = graphics_pipeline_;

      gp.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, shaders[0u]);
      gp.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, shaders[1u]);

      /* Add a push constant range to access. */
      gp.add_push_constant_range({
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .offset = 0u,
        .size = sizeof(shader_interop::PushConstant),
      });

      gp.set_vertex_binding_attribute({
        .bindings = {
          {
            .binding = 0u,
            .stride = sizeof(Vertex_t),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
          },
        },
        .attributes = {
          {
            .location = AttributeLocation::Position,
            .binding = 0u,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex_t, Vertex_t::Position),
          },
          {
            .location = AttributeLocation::Color,
            .binding = 0u,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex_t, Vertex_t::Color),
          },
        },
      });

      gp.complete(context_.get_device(), renderer_);
    }

    context_.release_shader_modules(shaders);

    return true;
  }

  void release() final {
    auto allocator = context_.get_resource_allocator();
    graphics_pipeline_.release(context_.get_device());
    allocator->destroy_buffer(vertex_buffer_);
  }

  void frame() final {
    /* Retrieve current frame time. */
    float const tick{ get_frame_time() };

    /* Prepare values for left/right viewport-scissor, showing the two ways to do it. */
    VkExtent2D const left_screen{
      viewport_size_.width/2u,
      viewport_size_.height
    };
    VkRect2D const right_screen{
      .offset = {.x = static_cast<int>(left_screen.width)},
      .extent = left_screen,
    };

    auto cmd = renderer_.begin_frame();
    {
      /* By specifying the pipeline layout to target, we can push constants
       * before the actual pipeline is bound. */
      cmd.push_constant(tick * 1.4f, graphics_pipeline_.get_layout());

      auto pass = cmd.begin_rendering();
      {
        pass.set_pipeline(graphics_pipeline_);
        pass.set_vertex_buffer(vertex_buffer_);

        // Left-side.
        {
          /* Set viewport-scissor using a VkExtent2D (with no offset) */
          pass.set_viewport_scissor(left_screen, kFlipScreenVertically);
          pass.draw(kVertices.size());
        }

        // Right-side.
        {
          /* Set viewport-scissor using a VkRect2D (with offset) */
          pass.set_viewport_scissor(right_screen, !kFlipScreenVertically);

          /**
           * If no pipeline layout is specified, the pass encoder will take the
           * one from the currently bound pipeline when available.
           * This is only available using a RenderPassEncoder.
           **/
          pass.push_constant(tick * 4.0f);
          pass.draw(kVertices.size());
        }
      }
      cmd.end_rendering();
    }
    renderer_.end_frame();
  }

 private:
  GraphicsPipeline graphics_pipeline_;
  Buffer_t vertex_buffer_;
};

// ----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  return SampleApp().run();
}

/* -------------------------------------------------------------------------- */
