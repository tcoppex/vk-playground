#ifndef VKFRAMEWORK_RENDERER_RENDERER_H_
#define VKFRAMEWORK_RENDERER_RENDERER_H_

/* -------------------------------------------------------------------------- */

#include "framework/core/common.h"

#include "framework/backend/swapchain.h"
#include "framework/backend/command_encoder.h"
#include "framework/renderer/pipeline.h"
#include "framework/renderer/sampler_pool.h"
#include "framework/renderer/targets/framebuffer.h"
#include "framework/renderer/targets/render_target.h"
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
  static constexpr VkClearValue kDefaultColorClearValue{{{1.0f, 0.25f, 0.75f, 1.0f}}};

 public:
  Renderer() = default;
  ~Renderer() {}

  void init(Context const& context, ResourceAllocator* allocator, VkSurfaceKHR const surface);

  void deinit();

  CommandEncoder begin_frame();

  void end_frame();

  Swapchain const& swapchain() const {
    return swapchain_;
  }

  Context const& context() const {
    return *ctx_ptr_;
  }

  Skybox& skybox() {
    return skybox_;
  }

  Skybox const& skybox() const {
    return skybox_;
  }

 public:
  /* ----- Factory ----- */

  // --- Render Target (Dynamic Rendering) ---

  std::shared_ptr<RenderTarget> create_render_target() const;

  std::shared_ptr<RenderTarget> create_render_target(RenderTarget::Descriptor_t const& desc) const;

  std::shared_ptr<RenderTarget> create_default_render_target(uint32_t num_color_outputs = 1u) const;

  // --- Framebuffer (Legacy Rendering) ---

  std::shared_ptr<Framebuffer> create_framebuffer() const;

  std::shared_ptr<Framebuffer> create_framebuffer(Framebuffer::Descriptor_t const& desc) const;

  // --- Pipeline Layout ---

  VkPipelineLayout create_pipeline_layout(PipelineLayoutDescriptor_t const& params) const;

  void destroy_pipeline_layout(VkPipelineLayout layout) const;

  // --- Pipelines ---

  void destroy_pipeline(Pipeline const& pipeline) const;

  // --- Graphics Pipelines ---

  VkGraphicsPipelineCreateInfo get_graphics_pipeline_create_info(
    GraphicsPipelineCreateInfoData_t &data,
    VkPipelineLayout pipeline_layout,
    GraphicsPipelineDescriptor_t const& desc
  ) const;

  void create_graphics_pipelines(
    VkPipelineLayout pipeline_layout,
    std::vector<GraphicsPipelineDescriptor_t> const& descs,
    std::vector<Pipeline> *out_pipelines
  ) const;

  Pipeline create_graphics_pipeline(
    VkPipelineLayout pipeline_layout,
    GraphicsPipelineDescriptor_t const& desc
  ) const;

  Pipeline create_graphics_pipeline(
    PipelineLayoutDescriptor_t const& layout_desc,
    GraphicsPipelineDescriptor_t const& desc
  ) const;

  Pipeline create_graphics_pipeline(
    GraphicsPipelineDescriptor_t const& desc
  ) const;

  // --- Compute Pipelines ---

  void create_compute_pipelines(
    VkPipelineLayout pipeline_layout,
    std::vector<backend::ShaderModule> const& modules,
    Pipeline *pipelines
  ) const;

  Pipeline create_compute_pipeline(
    VkPipelineLayout pipeline_layout,
    backend::ShaderModule const& module
  ) const;

  // --- Ray Tracing Pipelines ---

  Pipeline create_raytracing_pipeline(
    VkPipelineLayout pipeline_layout,
    RayTracingPipelineDescriptor_t const& desc
  ) const;

  // --- Descriptor Set Layout ---

  VkDescriptorSetLayout create_descriptor_set_layout(std::vector<DescriptorSetLayoutParams> const& params) const;

  void destroy_descriptor_set_layout(VkDescriptorSetLayout& layout) const;

  // --- Descriptor Set ---

  std::vector<VkDescriptorSet> create_descriptor_sets(std::vector<VkDescriptorSetLayout> const& layouts) const;

  VkDescriptorSet create_descriptor_set(VkDescriptorSetLayout const layout) const;

  VkDescriptorSet create_descriptor_set(VkDescriptorSetLayout const layout, std::vector<DescriptorSetWriteEntry> const& entries) const;

  // --- Texture ---

  bool load_image_2d(CommandEncoder const& cmd, std::string_view const& filename, backend::Image &image) const;

  bool load_image_2d(std::string_view const& filename, backend::Image &image) const;

  // --- Sampler ---

  VkSampler default_sampler() const {
    return sampler_pool_.default_sampler(); //
  }

  SamplerPool& sampler_pool() {
    return sampler_pool_;
  }

  SamplerPool const& sampler_pool() const {
    return sampler_pool_;
  }

  // --- GPUResources gltf objects ---

  GLTFScene load_and_upload(std::string_view gltf_filename, scene::Mesh::AttributeLocationMap const& attribute_to_location);

  GLTFScene load_and_upload(std::string_view gltf_filename);

  std::future<GLTFScene> async_load_to_device(std::string const& filename) {
    // [might be incorrect if async spawn a thread, as there is GPU transfer here]
    return utils::RunTaskGeneric<GLTFScene>([this, filename] {
      return load_and_upload(filename);
    });
  }

 public:
  /* ----- RTInterface Overrides ----- */

  VkExtent2D get_surface_size() const final {
    return swapchain_.get_surface_size();
  }

  uint32_t get_color_attachment_count() const final {
    return 1u;
  }

  std::vector<backend::Image> const& get_color_attachments() const final {
    proxy_swap_attachment_ = { get_color_attachment() };
    return proxy_swap_attachment_;
  }

  backend::Image const& get_color_attachment(uint32_t i = 0u) const final {
    return swapchain_.get_current_swap_image();
  }

  backend::Image const& get_depth_stencil_attachment() const final {
    return depth_stencil_;
  }

  VkClearValue get_color_clear_value(uint32_t i = 0u) const final {
    return color_clear_value_;
  }

  VkClearValue get_depth_stencil_clear_value() const final {
    return depth_stencil_clear_value_;
  }

  VkAttachmentLoadOp get_color_load_op(uint32_t i = 0u) const {
    return color_load_op_;
  }

  void set_color_clear_value(VkClearColorValue clear_color, uint32_t i = 0u) final {
    color_clear_value_.color = clear_color;
  }

  void set_depth_stencil_clear_value(VkClearDepthStencilValue clear_depth_stencil) final {
    depth_stencil_clear_value_.depthStencil = clear_depth_stencil;
  }

  void set_color_load_op(VkAttachmentLoadOp load_op, uint32_t i = 0u) {
    color_load_op_ = load_op;
  }

 private:
  VkFormat get_valid_depth_format() const {
    return VK_FORMAT_D24_UNORM_S8_UINT; // VK_FORMAT_D16_UNORM; //
  }

  void init_descriptor_pool();

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

    TimelineFrame_t& current_frame() {
      return frames[frame_index];
    }
  };

 private:
  /* Copy references for quick access */
  Context const* ctx_ptr_{};
  VkDevice device_{};
  ResourceAllocator* allocator_ptr_{};

  /* Swapchain. */
  Swapchain swapchain_{};
  mutable std::vector<backend::Image> proxy_swap_attachment_{};

  /* Pipeline Cache */
  VkPipelineCache pipeline_cache_;

  /* Default depth-stencil buffer. */
  backend::Image depth_stencil_{};

  /* Timeline frame resources */
  Timeline_t timeline_;

  /* Main descriptor pool. */
  std::vector<VkDescriptorPoolSize> descriptor_pool_sizes_{};
  VkDescriptorPool descriptor_pool_{};

  /* Miscs resources */
  VkClearValue color_clear_value_{kDefaultColorClearValue};
  VkClearValue depth_stencil_clear_value_{{{1.0f, 0u}}};
  VkAttachmentLoadOp color_load_op_{VK_ATTACHMENT_LOAD_OP_CLEAR};

  // Reference to the current CommandEncoder returned by 'begin_frame'
  CommandEncoder cmd_{}; //

  /* Utils */
  SamplerPool sampler_pool_{};
  Skybox skybox_{};
};

/* -------------------------------------------------------------------------- */

#endif
