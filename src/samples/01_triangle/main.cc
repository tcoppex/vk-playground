/* -------------------------------------------------------------------------- */
//
//    01 - Hello Triangle
//
//    Demonstrates how to render a triangle, using:
//        - Graphics Pipeline,
//        - Vertex buffer,
//        - Transient command buffer,
//        - RenderPassEncoder commands:
//            * Dynamic Viewport / Scissor states,
//            * set_pipeline / set_vertex_buffer / draw.
//
//
/* -------------------------------------------------------------------------- */

#include "framework/application.h"
#include "framework/renderer/graphics_pipeline.h"

/* -------------------------------------------------------------------------- */

class SampleApp final : public Application {
 public:
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
    {.Position = { -0.5f, -0.5f, 0.0f, 1.0f}, .Color = {0.5f, 0.2f, 1.0f, 1.0f}},
    {.Position = { +0.5f, -0.5f, 0.0f, 1.0f}, .Color = {1.0f, 0.5f, 0.2f, 1.0f}},
    {.Position = {  0.0f, +0.5f, 0.0f, 1.0f}, .Color = {0.2f, 1.0f, 0.5f, 1.0f}},
  };

 public:
  SampleApp() = default;
  ~SampleApp() {}

 private:
  bool setup() final {
    glfwSetWindowTitle(window_, "01 - さんかくのセレナーデ");

    renderer_.set_color_clear_value({.float32 = {0.25f, 0.25f, 0.25f, 1.0f}});

    /* Create a device storage buffer, then upload vertices host data to it.
     *
     * We use a (temporary) transient command buffer to create the device vertex
     * buffer.
     **/
    {
      auto cmd = context_.create_transient_command_encoder();

      vertex_buffer_ = cmd.create_buffer_and_upload(kVertices,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
      );

      /* Submit the command to the graphics queue. */
      context_.finish_transient_command_encoder(cmd);
    }

    /* Load the precompiled shader modules (the '.spv' prefix is omitted). */
    auto const shaders{context_.create_shader_modules(COMPILED_SHADERS_DIR, {
      "vs_simple.glsl",
      "fs_simple.glsl",
    })};

    /* Setup the graphics pipeline. */
    {
      /**
       * The GraphicsPipeline object is presetup with a default layout for
       * rendering. Before using it we need to specify at least a vertex and
       * fragment shader and the binding of its vertex attributes, if any.
       **/
      auto& gp = graphics_pipeline_;

      gp.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, shaders[0u]);
      gp.add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, shaders[1u]);

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

      /**
       * We call 'complete' to finalize the GraphicsPipeline setup for a given output.
       *
       * Both RTInterface (dynamic_rendering) and RPInterface (legacy rendering)
       * are accepted. Here we use the main renderer as we output directly to
       * the swapchain via dynamic_rendering.
       **/
      gp.complete(context_.get_device(), renderer_);
    }

    /* Release the shader modules. */
    context_.release_shader_modules(shaders);

    return true;
  }

  void release() final {
    auto allocator = context_.get_resource_allocator();

    graphics_pipeline_.release(context_.get_device()); //
    allocator->destroy_buffer(vertex_buffer_);
  }

  void frame() final {
    auto cmd = renderer_.begin_frame();

    /**
     * 'begin_rendering' (dynamic_rendering) or 'begin_render_pass' (legacy rendering)
     * returns a RenderPassEncoder, which is a specialized CommandEncoder to specify
     * rendering operations to a specific output (here the swapchain directly).
     **/
    auto pass = cmd.begin_rendering();
    {
      /**
       * Set the viewport and scissor with the flag 'flip_y' to true to flip
       * the Y-axis upward, like traditional APIs. It is set to false by default.
       *
       * As GraphicPipeline uses dynamic Viewport and Scissor states by default,
       * we need to specify them when using a graphic pipeline.
       *
       * As dynamic states are not bound to a pipeline they can be set whenever
       * during rendering before the draw command.
       **/
      pass.set_viewport_scissor(viewport_size_, true);

      pass.set_pipeline(graphics_pipeline_);
      pass.set_vertex_buffer(vertex_buffer_);
      pass.draw(kVertices.size());
    }
    cmd.end_rendering();

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
