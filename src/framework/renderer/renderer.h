#ifndef HELLOVK_FRAMEWORK_RENDERER_RENDERER_H
#define HELLOVK_FRAMEWORK_RENDERER_RENDERER_H

/* -------------------------------------------------------------------------- */

#include "framework/backend/context.h"
#include "framework/backend/swapchain.h"
#include "framework/backend/command_encoder.h"
#include "framework/renderer/pipeline.h"
#include "framework/scene/resources.h" //

#include "framework/renderer/_experimental/framebuffer.h" // (for Framebuffer::Descriptor_t)
class RenderTarget;


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

  void init(Context const& context, std::shared_ptr<ResourceAllocator> allocator, VkSurfaceKHR const surface);

  void deinit();

  CommandEncoder begin_frame();

  void end_frame();

  Swapchain const& get_swapchain() const {
    return swapchain_;
  }

 public:
  /* ----- Factory ----- */

  // --- Render Target (Dynamic Rendering) ---

  std::shared_ptr<RenderTarget> create_render_target() const;

  // --- Framebuffer (Legacy Rendering) ---

  std::shared_ptr<Framebuffer> create_framebuffer() const;
  std::shared_ptr<Framebuffer> create_framebuffer(Framebuffer::Descriptor_t const& desc) const;

  // --- Pipeline Layout ---

  VkPipelineLayout create_pipeline_layout(PipelineLayoutDescriptor_t const& params) const;

  void destroy_pipeline_layout(VkPipelineLayout layout) const;

  // --- Pipeline ---

  Pipeline create_graphics_pipeline(VkPipelineLayout pipeline_layout, GraphicsPipelineDescriptor_t const& desc) const;

  // Specialized version that create the layout internally.
  Pipeline create_graphics_pipeline(PipelineLayoutDescriptor_t const& layout_desc, GraphicsPipelineDescriptor_t const& desc) const;

  Pipeline create_graphics_pipeline(GraphicsPipelineDescriptor_t const& desc) const;

  void create_compute_pipelines(VkPipelineLayout pipeline_layout, std::vector<backend::ShaderModule> const& modules, Pipeline *pipelines) const;

  Pipeline create_compute_pipeline(VkPipelineLayout pipeline_layout, backend::ShaderModule const& module) const;

  void destroy_pipeline(Pipeline const& pipeline) const;

  // --- Descriptor Set Layout ---

  VkDescriptorSetLayout create_descriptor_set_layout(std::vector<DescriptorSetLayoutParams> const& params) const;

  void destroy_descriptor_set_layout(VkDescriptorSetLayout& layout) const;

  // --- Descriptor Set ---

  std::vector<VkDescriptorSet> create_descriptor_sets(std::vector<VkDescriptorSetLayout> const& layouts) const;

  VkDescriptorSet create_descriptor_set(VkDescriptorSetLayout const layout) const;

  VkDescriptorSet create_descriptor_set(VkDescriptorSetLayout const layout, std::vector<DescriptorSetWriteEntry> const& entries) const;

  void update_descriptor_set(VkDescriptorSet const& descriptor_set, std::vector<DescriptorSetWriteEntry> const& entries) const;

  // --- Texture ---

  bool load_image_2d(CommandEncoder const& cmd, std::string_view const& filename, backend::Image &image) const;

  bool load_image_2d(std::string_view const& filename, backend::Image &image) const;

  // --- Sampler ---

  VkSampler get_default_sampler() const {
    return linear_sampler_;
  }

  // --- Resources gltf objects ---

  std::shared_ptr<scene::Resources> load_and_upload(std::string_view gltf_filename, scene::Mesh::AttributeLocationMap const& attribute_to_location);

 public:
  /* ----- RTInterface Overrides ----- */

  VkExtent2D get_surface_size() const final {
    return swapchain_.get_surface_size();
  }

  uint32_t get_color_attachment_count() const final {
    return 1u;
  }

  std::vector<backend::Image> const& get_color_attachments() const final {
    // (special behavior, just used here, updating it elsewhere is non practical)
    const_cast<Renderer*>(this)->proxy_swap_attachment_ = { get_color_attachment() };
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

  void set_color_clear_value(VkClearColorValue clear_color, uint32_t i = 0u) final {
    color_clear_value_.color = clear_color;
  }

  void set_depth_stencil_clear_value(VkClearDepthStencilValue clear_depth_stencil) final {
    depth_stencil_clear_value_.depthStencil = clear_depth_stencil;
  }

 private:
  inline VkFormat get_valid_depth_format() const {
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
  std::shared_ptr<ResourceAllocator> allocator_{};
  Context::TargetQueue target_queue_{};

  /* Swapchain. */
  Swapchain swapchain_{};
  std::vector<backend::Image> proxy_swap_attachment_{};

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

  // Reference to the current CommandEncoder returned by 'begin_frame'
  CommandEncoder cmd_{}; //

  VkSampler linear_sampler_{}; //
};

/* -------------------------------------------------------------------------- */

#endif
