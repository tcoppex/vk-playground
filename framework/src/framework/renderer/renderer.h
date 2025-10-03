#ifndef VKFRAMEWORK_RENDERER_RENDERER_H_
#define VKFRAMEWORK_RENDERER_RENDERER_H_

/* -------------------------------------------------------------------------- */

#include "framework/core/common.h"

#include "framework/backend/swapchain.h"
// #include "framework/core/platform/openxr/xr_vulkan_interface.h" //
#include "framework/core/platform/openxr/openxr_context.h" //

#include "framework/renderer/render_context.h"
#include "framework/backend/command_encoder.h"

#include "framework/renderer/fx/skybox.h"
#include "framework/renderer/gpu_resources.h" // (for GLTFScene)

/* -------------------------------------------------------------------------- */

/**
 * Main entry point to render image to the swapchain and issue commands to the
 * graphic / present queue.
 *
 * -> Use 'begin_frame' / 'end_frame' when ready to submit command via the
 *    returned CommandEncoder object.
 *
 * -> 'create_render_target' can be used to create custom dynamic render target,
 *    'create_framebuffer' to create legacy rendering target.
 *
 *  Note: As a RTInterface, Renderer always returns 1 color_attachment in the
 *        form of the current swapchain image.
 *
 **/
class Renderer : public backend::RTInterface {
 public:
  static constexpr VkClearValue kDefaultColorClearValue{{
    .float32 = {1.0f, 0.25f, 0.75f, 1.0f}
  }};

 public:
  Renderer() = default;
  ~Renderer() = default;

  void init(
    RenderContext& context,
    SwapchainInterface* swapchain_ptr
  );

  void deinit();

  [[nodiscard]]
  CommandEncoder begin_frame();

  void end_frame();

  [[nodiscard]]
  RenderContext const& context() const noexcept { return *ctx_ptr_; }

  [[nodiscard]]
  Skybox const& skybox() const noexcept { return skybox_; }

  [[nodiscard]]
  Skybox& skybox() noexcept { return skybox_; }

  // --- Render Target (Dynamic Rendering) ---

  [[nodiscard]]
  std::unique_ptr<RenderTarget> create_default_render_target(
    uint32_t num_color_outputs = 1u
  ) const;

  // --- Graphics Pipelines ---

  [[nodiscard]]
  VkGraphicsPipelineCreateInfo create_graphics_pipeline_create_info(
    GraphicsPipelineCreateInfoData_t &data,
    VkPipelineLayout pipeline_layout,
    GraphicsPipelineDescriptor_t const& desc
  ) const;

  // Batch create graphics pipelines from a common layout.
  void create_graphics_pipelines(
    VkPipelineLayout pipeline_layout,
    std::vector<GraphicsPipelineDescriptor_t> const& descs,
    std::vector<Pipeline> *out_pipelines
  ) const;

  // Create a graphics pipeline with a pre-defined layout.
  [[nodiscard]]
  Pipeline create_graphics_pipeline(
    VkPipelineLayout pipeline_layout,
    GraphicsPipelineDescriptor_t const& desc
  ) const;

  // Create a graphics pipeline and a layout based on description.
  [[nodiscard]]
  Pipeline create_graphics_pipeline(
    PipelineLayoutDescriptor_t const& layout_desc,
    GraphicsPipelineDescriptor_t const& desc
  ) const;

  // Create a graphics pipeline with a default empty layout.
  [[nodiscard]]
  Pipeline create_graphics_pipeline(
    GraphicsPipelineDescriptor_t const& desc
  ) const;

  // --- GPUResources gltf objects ---

  [[nodiscard]]
  GLTFScene load_gltf(
    std::string_view gltf_filename,
    scene::Mesh::AttributeLocationMap const& attribute_to_location
  );

  [[nodiscard]]
  GLTFScene load_gltf(std::string_view gltf_filename);

  [[nodiscard]]
  std::future<GLTFScene> async_load_gltf(std::string const& filename) {
    return utils::RunTaskGeneric<GLTFScene>([this, filename] {
      return load_gltf(filename);
    });
  }

 public:
  /* ----- RTInterface Overrides ----- */

  [[nodiscard]]
  VkExtent2D surface_size() const final {
    LOG_CHECK(swapchain_ptr_ != nullptr);
    return swapchain_ptr_->surfaceSize();
  }

  [[nodiscard]]
  uint32_t color_attachment_count() const final {
    return 1u;
  }

  [[nodiscard]]
  std::vector<backend::Image> color_attachments() const final {
    return { color_attachment() };
  }

  [[nodiscard]]
  backend::Image color_attachment(uint32_t index = 0u) const final {
    LOG_CHECK(swapchain_ptr_ != nullptr);
    return swapchain_ptr_->currentImage();
  }

  // VkFormat color_attachment_format(uint32_t index = 0u) const {
  //   return color_attachment(index).format;
  // }

  [[nodiscard]]
  backend::Image depth_stencil_attachment() const final {
    return depth_stencil_;
  }

  [[nodiscard]]
  VkClearValue color_clear_value(uint32_t index = 0u) const final {
    return color_clear_value_;
  }

  [[nodiscard]]
  VkClearValue depth_stencil_clear_value() const final {
    return depth_stencil_clear_value_;
  }

  [[nodiscard]]
  VkAttachmentLoadOp color_load_op(uint32_t index = 0u) const final {
    return color_load_op_;
  }

  [[nodiscard]]
  backend::RenderingViewInfo rendering_view_info() const noexcept final {
    LOG_CHECK(swapchain_ptr_ != nullptr);
    return swapchain_ptr_->renderingViewInfo();
  }

  void set_color_clear_value(VkClearColorValue clear_color, uint32_t index = 0u) final {
    color_clear_value_.color = clear_color;
  }

  void set_depth_stencil_clear_value(VkClearDepthStencilValue clear_depth_stencil) final {
    depth_stencil_clear_value_.depthStencil = clear_depth_stencil;
  }

  void set_color_load_op(VkAttachmentLoadOp load_op, uint32_t index = 0u) final {
    color_load_op_ = load_op;
  }

  bool resize(uint32_t w, uint32_t h) final;

  // -----------

  void set_color_clear_value(vec4 clear_color, uint32_t index = 0u) {
    set_color_clear_value(
      {.float32 = { clear_color.x, clear_color.y, clear_color.z, clear_color.w }},
      index
    );
  }

  [[nodiscard]]
  uint32_t swap_image_count() const noexcept {
    LOG_CHECK(swapchain_ptr_ != nullptr);
    return swapchain_ptr_->imageCount();
  }

 private:
  void init_view_resources();
  void deinit_view_resources();

  // ------------------------------------------
  [[nodiscard]]
  VkFormat valid_depth_format() const noexcept {
    return VK_FORMAT_D24_UNORM_S8_UINT //
           ;
  }
  // ------------------------------------------

 private:
  struct FrameResources {
    VkCommandPool command_pool{};
    VkCommandBuffer command_buffer{};
  };

  /* References for quick access */
  RenderContext* ctx_ptr_{};
  ResourceAllocator* allocator_ptr_{};
  VkDevice device_{};

  SwapchainInterface* swapchain_ptr_{};

  /* Default depth-stencil buffer. */
  backend::Image depth_stencil_{}; // xxx

  /* Timeline frame resources */
  std::vector<FrameResources> frames_{};
  uint32_t frame_index_{};

  /* Miscs resources */
  VkClearValue color_clear_value_{kDefaultColorClearValue};
  VkClearValue depth_stencil_clear_value_{{{1.0f, 0u}}};
  VkAttachmentLoadOp color_load_op_{VK_ATTACHMENT_LOAD_OP_CLEAR};

  /* Reference to the current CommandEncoder returned by 'begin_frame' */
  CommandEncoder cmd_{}; //

  // ----------

  Skybox skybox_{}; //
};

/* -------------------------------------------------------------------------- */

#endif // VKFRAMEWORK_RENDERER_RENDERER_H_
