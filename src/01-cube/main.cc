#include <cstdlib>
#include "glm/gtc/matrix_transform.hpp"

#include "engine/window.h"
#include "engine/device/context.h"
#include "engine/device/render_handler.h"
#include "engine/device/buffer_manager.h"
#include "engine/device/graphics_pipeline.h"

#include "cube_data.h"

// ----------------------------------------------------------------------------

#define APP_NAME  ":: Vulkan cube demo ::"

// ----------------------------------------------------------------------------

struct Scene_t {
  // data
  glm::mat4 viewProjClip;
  float theta;

  // uniform
  uint32_t ubuf;
  DescriptorSet_t uniform_desc;

  // geometry
  uint32_t vb;
  VertexBindingAttrib_t vba;
  DrawParameter_t cube;

  // pipeline
  GraphicsPipeline graphics_pipeline;
} g_scene;

// ----------------------------------------------------------------------------

void update_model(DeviceContext &ctx) {
  /* calculate the new matrix */
  glm::mat4 model = glm::rotate(glm::mat4(1.0f), g_scene.theta, glm::vec3(-1.0, 1.0f, 0.0f));
  glm::mat4 mvpc = g_scene.viewProjClip * model;

  /* upload to device buffer */
  ctx.BufferManager().upload(g_scene.ubuf, 0, sizeof(mvpc), (void const*)&mvpc);

  /* update animation value */
  g_scene.theta += 0.001f;
}

// ----------------------------------------------------------------------------

void frame(DeviceContext &ctx, RenderHandler &rh) {
  /* update uniform data */
  update_model(ctx);

  /* renderiiiiing */
  rh.begin();
  {
    CommandBuffer& cmd = rh.graphics_command_buffer();

    /* Bind the graphics pipeline to the command buffer with descriptors. */
    cmd.bind_graphics_pipeline(&g_scene.graphics_pipeline, &g_scene.uniform_desc);

    /* Set the viewport/scissor, both defined as dynamic states. */
    cmd.set_viewport_scissor(0, 0, rh.width(), rh.height());

    /* Draw the cube. */
    cmd.draw(g_scene.cube);
  }
  rh.end();
}

// ----------------------------------------------------------------------------

void demo(DeviceContext &ctx, Window &window) {
  RenderHandler *rh = ctx.create_render_handle(window.width(), window.height());
  DeviceBufferManager &dbm = ctx.BufferManager();

  /* Uniform Data ----------------------------------- */

  /* setup a camera matrix */
  const float ratio = static_cast<float>(window.width()) / window.height();
  const glm::mat4 projection = glm::perspective(glm::radians(60.0f), ratio, 0.1f, 500.0f);
  const glm::mat4 view = glm::lookAt(
    glm::vec3(-5.0, 3.0, -5.0),
    glm::vec3(0.0, 0.0, 0.0),
    glm::vec3(0.0, -1.0, 0.0)
  );
  g_scene.viewProjClip = ctx.clip_matrix() * projection * view;
  g_scene.theta = 0.0f;

  /* Create an uniform buffer for shader data. */
  dbm.create(&g_scene.ubuf);
  dbm.allocate(g_scene.ubuf,
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               sizeof(g_scene.viewProjClip),
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  /* Create a descriptor set layout binding. */
  DSLBinding_t dsl;
  dsl.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

  /* Associate uniform buffer to the desc layout at id 0 */
  dsl.set_desc_buffer_info(0u, dbm.descriptor(g_scene.ubuf)); //


  /* Geometry ------------------------------------ */

  /* Create a data buffer for geometry vertices. */
  dbm.create(&g_scene.vb);
  dbm.allocate(g_scene.vb,
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               sizeof(g_vb_solid_face_colors_Data),
               g_vb_solid_face_colors_Data,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  /* Draw parameter for the cube */
  g_scene.cube.desc = dbm.descriptor(g_scene.vb);
  g_scene.cube.num_vertices = sizeof(g_vb_solid_face_colors_Data) / sizeof(Vertex);

  /* Specify the vertex attributes layout. */
  VertexBindingAttrib_t &vba = g_scene.vba;

  /* Set Vertex Input binding parameters. */
  vba.add_binding(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);

  /* Set Vertex Input attributes parameters. */
  // Position
  vba.add_attrib(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, Vertex::posX));
  // Color
  vba.add_attrib(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, Vertex::r));


  /* Graphics Pipeline -------------------------- */

  GraphicsPipeline &gp = g_scene.graphics_pipeline;

  /* Specify Vertex & Fragment shader stage */
  gp.set_shader_stage(VK_SHADER_STAGE_VERTEX_BIT,
                      CreateShaderModule(ctx.device(), COMPILED_SHADERS_DIR "vs_simple.glsl.spv"));
  gp.set_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT,
                      CreateShaderModule(ctx.device(), COMPILED_SHADERS_DIR "fs_simple.glsl.spv"));

  /* Specify vertices input attributes */
  gp.set_vertex_binding_attrib(g_scene.vba);

  /* Descriptor layout define how to access pipeline's uniform data */
  gp.create_descriptor_set_layout(ctx.device(), dsl.bindings.data(), dsl.bindings.size());

  /* Set Dynamic states Viewport and Scissor.
   * (ie. their value can be changed after the pipeline is bind). */
  gp.enable_dynamic(VK_DYNAMIC_STATE_VIEWPORT);
  gp.enable_dynamic(VK_DYNAMIC_STATE_SCISSOR);

  /* Set the render pass to be used (ie. where to render.) */
  gp.set_render_pass(&(rh->render_pass()));

  /* Finalize the pipeline before using it. */
  CHECK_VK( gp.complete(&ctx.device()) );


  /* Descriptor Set -------------------------- */

  DescriptorSet_t &udesc = g_scene.uniform_desc;

  /* Create a descriptor set from binding and the pipeline description layout */
  udesc.create_from_layout( ctx.device(),
                            dsl.bindings.data(), dsl.bindings.size(),
                            gp.desc_layouts(), gp.num_desc_layout());

  /* Fill the set with previously defined info */
  udesc.update_set(ctx.device(), dsl);


  /* Miscs ---------------------------------- */

  rh->clear_colors(0.15f, 0.10f, 0.10f, 1.0f);


  /* Mainloop */
  while (window.is_open()) {
    /* Manage events */
    window.events();

    /* Detects change in resolution */
    if (window.updated_resolution()) {
      rh->resize(window.width(), window.height());
    }

    /* Update and render one frame */
    frame(ctx, *rh);

    /* Display the rendered image and swap buffers */
    rh->display_and_swap();
  }

  /* Clean Everythings ! */
  udesc.destroy(ctx.device());
  gp.destroy();
  dbm.destroy(g_scene.ubuf);
  dbm.destroy(g_scene.vb);
  ctx.destroy_render_handle(&rh);
}

// ----------------------------------------------------------------------------

int main() {
  Window window;
  DeviceContext device_context;

  /* Initialize the Window Manager */
  window.init();

  /* Create the window */
  window.create(APP_NAME);

  /* Initialize the device context (Vulkan) */
  device_context.init(APP_NAME, &window);

  /* demo specific code */
  demo(device_context, window);

  /* Clean exit */
  device_context.deinit();
  window.deinit();

  return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------------
