#ifndef VKFRAMEWORK_RENDERER_RENDERER_H_
#define VKFRAMEWORK_RENDERER_RENDERER_H_

/* -------------------------------------------------------------------------- */

#include "framework/core/common.h"

#include "framework/backend/swapchain.h"
#include "framework/backend/command_encoder.h"
#include "framework/renderer/targets/framebuffer.h"
#include "framework/renderer/targets/render_target.h"
#include "framework/renderer/pipeline.h"
#include "framework/renderer/sampler_pool.h"
#include "framework/renderer/descriptor_set_registry.h" //
#include "framework/renderer/gpu_resources.h" // (for GLTFScene)
#include "framework/renderer/fx/skybox.h"

class Context;

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
  static constexpr uint32_t kMaxDescriptorPoolSets{ 256u };

 public:
  Renderer() = default;
  ~Renderer() {}

  void init(
    Context const& context,
    Swapchain& swapchain,
    ResourceAllocator& allocator
  );

  void deinit();

  [[nodiscard]]
  CommandEncoder begin_frame();

  void end_frame();

  [[nodiscard]]
  Context const& context() const noexcept {
    return *ctx_ptr_;
  }

  [[nodiscard]]
  Skybox const& skybox() const noexcept {
    return skybox_;
  }

  [[nodiscard]]
  Skybox& skybox() noexcept {
    return skybox_;
  }

 public:
  /* ----- Factory ----- */

  // --- Render Target (Dynamic Rendering) ---

  [[nodiscard]]
  std::unique_ptr<RenderTarget> create_render_target() const;

  [[nodiscard]]
  std::unique_ptr<RenderTarget> create_render_target(
    RenderTarget::Descriptor_t const& desc
  ) const;

  [[nodiscard]]
  std::unique_ptr<RenderTarget> create_default_render_target(
    uint32_t num_color_outputs = 1u
  ) const;

  // --- Framebuffer (Legacy Rendering) ---

  [[nodiscard]]
  std::unique_ptr<Framebuffer> create_framebuffer() const;

  [[nodiscard]]
  std::unique_ptr<Framebuffer> create_framebuffer(
    Framebuffer::Descriptor_t const& desc
  ) const;

  // --- Pipeline Layout ---

  [[nodiscard]]
  VkPipelineLayout create_pipeline_layout(
    PipelineLayoutDescriptor_t const& params
  ) const;

  void destroy_pipeline_layout(
    VkPipelineLayout layout
  ) const;

  // --- Pipelines ---

  void destroy_pipeline(
    Pipeline const& pipeline
  ) const;

  // --- Graphics Pipelines ---

  [[nodiscard]]
  VkGraphicsPipelineCreateInfo get_graphics_pipeline_create_info(
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

  // --- Compute Pipelines ---

  void create_compute_pipelines(
    VkPipelineLayout pipeline_layout,
    std::vector<backend::ShaderModule> const& modules,
    Pipeline *pipelines
  ) const;

  [[nodiscard]]
  Pipeline create_compute_pipeline(
    VkPipelineLayout pipeline_layout,
    backend::ShaderModule const& module
  ) const;

  // --- Ray Tracing Pipelines ---

  [[nodiscard]]
  Pipeline create_raytracing_pipeline(
    VkPipelineLayout pipeline_layout,
    RayTracingPipelineDescriptor_t const& desc
  ) const;

  // --- Descriptor Set Registry ---

  [[nodiscard]]
  DescriptorSetRegistry const& descriptor_set_registry() const noexcept {
    return descriptor_set_registry_;
  }

  [[nodiscard]]
  VkDescriptorSetLayout create_descriptor_set_layout(
    DescriptorSetLayoutParamsBuffer const& params,
    VkDescriptorSetLayoutCreateFlags flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT
  ) const;

  void destroy_descriptor_set_layout(VkDescriptorSetLayout& layout) const;

  [[nodiscard]]
  VkDescriptorSet create_descriptor_set(VkDescriptorSetLayout const layout) const;

  [[nodiscard]]
  VkDescriptorSet create_descriptor_set(
    VkDescriptorSetLayout const layout,
    std::vector<DescriptorSetWriteEntry> const& entries
  ) const;

  // --- Texture ---

  bool load_image_2d(
    CommandEncoder const& cmd,
    std::string_view filename,
    backend::Image& image
  ) const;

  bool load_image_2d(
    std::string_view filename,
    backend::Image& image
  ) const;

  // --- Sampler ---

  [[nodiscard]]
  VkSampler default_sampler() const noexcept {
    return sampler_pool_.default_sampler();
  }

  [[nodiscard]]
  SamplerPool& sampler_pool() noexcept {
    return sampler_pool_;
  }

  [[nodiscard]]
  SamplerPool const& sampler_pool() const noexcept {
    return sampler_pool_;
  }

  // --- GPUResources gltf objects ---

  [[nodiscard]]
  GLTFScene load_and_upload(
    std::string_view gltf_filename,
    scene::Mesh::AttributeLocationMap const& attribute_to_location
  );

  [[nodiscard]]
  GLTFScene load_and_upload(std::string_view gltf_filename);

  [[nodiscard]]
  std::future<GLTFScene> async_load_to_device(std::string const& filename) {
    // [might be incorrect if async spawn a thread, as there is GPU transfer here]
    return utils::RunTaskGeneric<GLTFScene>([this, filename] {
      return load_and_upload(filename);
    });
  }

 public:
  /* ----- RTInterface Overrides ----- */

  VkExtent2D get_surface_size() const final {
    return swapchain_ptr_->get_surface_size();
  }

  uint32_t get_color_attachment_count() const final {
    return 1u;
  }

  std::vector<backend::Image> const& get_color_attachments() const final {
    proxy_swap_attachment_ = { get_color_attachment() };
    return proxy_swap_attachment_;
  }

  backend::Image const& get_color_attachment(uint32_t index = 0u) const final {
    return swapchain_ptr_->get_current_swap_image();
  }

  backend::Image const& get_depth_stencil_attachment() const final {
    return depth_stencil_;
  }

  VkClearValue get_color_clear_value(uint32_t index = 0u) const final {
    return color_clear_value_;
  }

  VkClearValue get_depth_stencil_clear_value() const final {
    return depth_stencil_clear_value_;
  }

  VkAttachmentLoadOp get_color_load_op(uint32_t index = 0u) const final {
    return color_load_op_;
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

  uint32_t get_swap_image_count() const noexcept {
    return swapchain_ptr_->get_image_count();
  }

 private:
  VkFormat get_valid_depth_format() const noexcept {
    return VK_FORMAT_D24_UNORM_S8_UINT
           // VK_FORMAT_D16_UNORM  //
           ;
  }

 private:
  struct TimelineFrame_t {
    uint64_t signal_index{};
    VkCommandPool command_pool{};
    VkCommandBuffer command_buffer{};
  };

  struct Timeline_t {
    std::vector<TimelineFrame_t> frames{};
    uint32_t frame_index{};
    VkSemaphore semaphore{};

    TimelineFrame_t& current_frame() noexcept {
      return frames[frame_index];
    }
  };

 private:
  /* References for quick access */
  Context const* ctx_ptr_{};
  Swapchain* swapchain_ptr_{};
  ResourceAllocator* allocator_ptr_{};
  VkDevice device_{};

  /* Default depth-stencil buffer */
  backend::Image depth_stencil_{};

  /* Timeline frame resources */
  Timeline_t timeline_{};

  /* Pipeline Cache */
  VkPipelineCache pipeline_cache_{};

  /* Descriptor registry and allocator */
  DescriptorSetRegistry descriptor_set_registry_{};

  // ----------------

  /* Utils */
  SamplerPool sampler_pool_{};
  Skybox skybox_{}; // (higher level..)

  // ----------------

  /* Miscs resources */

  // Proxy to const ref return the swapbuffer..
  mutable std::vector<backend::Image> proxy_swap_attachment_{}; //

  VkClearValue color_clear_value_{kDefaultColorClearValue};
  VkClearValue depth_stencil_clear_value_{{{1.0f, 0u}}};
  VkAttachmentLoadOp color_load_op_{VK_ATTACHMENT_LOAD_OP_CLEAR};

  // Reference to the current CommandEncoder returned by 'begin_frame'
  CommandEncoder cmd_{}; //
};

/* -------------------------------------------------------------------------- */

#endif
