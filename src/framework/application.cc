#include "framework/application.h"
#include "framework/wm/events.h"
#include "framework/wm/window.h"

/* -------------------------------------------------------------------------- */

int Application::run() {
  if (!presetup() || !setup()) {
    return EXIT_FAILURE;
  }

  auto &events{ Events::Get() };
  auto const nextFrame{[this, &events]() {
    events.prepareNextFrame();
    return wm_->poll();
  }};

  while (nextFrame()) {
    float tick = get_elapsed_time();
    last_frame_time_ = frame_time_;
    frame_time_ = tick;

    frame();
  }

  shutdown();

  return EXIT_SUCCESS;
}

float Application::get_elapsed_time() const {
  auto now{ std::chrono::high_resolution_clock::now() };
  return std::chrono::duration<float>(now - chrono_).count();
}

bool Application::presetup() {
  /* Singletons. */
  {
    Events::Initialize();
  }

  /* Create the main window surface. */
  if (wm_ = std::make_unique<Window>(); !wm_->init()) {
    return false;
  }

  /* Initialize the Vulkan context. */
  if (!context_.init(wm_->getVulkanInstanceExtensions())) {
    return false;
  }

  /* Create the context surface. */
  if (auto res = wm_->createWindowSurface(context_.get_instance(), &surface_); CHECK_VK(res)) {
    return false;
  }

  /* Init the default renderer. */
  renderer_.init(context_, context_.get_resource_allocator(), surface_);

  /* Initialize ImGui. */
  if (!presetup_ui()) {
    return false;
  }

  /* Miscs */
  {
    Events::Get().registerCallbacks(this);

    viewport_size_ = {
      .width = wm_->get_surface_width(),
      .height = wm_->get_surface_height(),
    };

    // Init time tracker.
    chrono_ = std::chrono::high_resolution_clock::now();
  }

  return true;
}

bool Application::presetup_ui() {
  IMGUI_CHECKVERSION();

  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  if (!ImGui_ImplGlfw_InitForVulkan(reinterpret_cast<GLFWwindow*>(wm_->get_handle()), true)) {
    return false;
  }

  VkDescriptorPoolSize const pool_sizes[]{
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512u },
  };
  VkDescriptorPoolCreateInfo const desc_pool_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
           | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
           ,
    .maxSets = 1024u, //
    .poolSizeCount = 1u,
    .pPoolSizes = pool_sizes,
  };
  CHECK_VK(vkCreateDescriptorPool(context_.get_device(), &desc_pool_info, nullptr, &imgui_descriptor_pool_));

  VkFormat const formats[]{
    renderer_.get_color_attachment().format
  };
  VkFormat const depth_stencil_format{
    renderer_.get_depth_stencil_attachment().format
  };
  auto const main_queue{ context_.get_queue() };
  ImGui_ImplVulkan_InitInfo info{
    .Instance = context_.get_instance(),
    .PhysicalDevice = context_.get_gpu(),
    .Device = context_.get_device(),
    .QueueFamily = main_queue.family_index,
    .Queue = main_queue.queue,
    .DescriptorPool = imgui_descriptor_pool_,
    .MinImageCount = 2,
    .ImageCount = renderer_.get_swapchain().get_image_count(),
    .UseDynamicRendering = true,
    .PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1u,
      .pColorAttachmentFormats = formats,
      .depthAttachmentFormat = depth_stencil_format,
      .stencilAttachmentFormat = depth_stencil_format,
    },
  };
  if (!ImGui_ImplVulkan_Init(&info)) {
    return false;
  }

  ImGui::GetIO().ConfigFlags = ImGuiConfigFlags_DockingEnable;

  return true;
}

void Application::shutdown() {
  CHECK_VK(vkDeviceWaitIdle(context_.get_device()));

  // User defined clean up.
  release();

  // ImGui.
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  vkDestroyDescriptorPool(context_.get_device(), imgui_descriptor_pool_, nullptr);

  renderer_.deinit();
  vkDestroySurfaceKHR(context_.get_instance(), surface_, nullptr);

  glfwTerminate();
  context_.deinit();

  Events::Deinitialize();
}

/* -------------------------------------------------------------------------- */
