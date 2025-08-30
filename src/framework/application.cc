#include "framework/application.h"

#include "framework/platform/events.h"
#include "framework/platform/window.h"

/* -------------------------------------------------------------------------- */

int Application::run() {
  if (!presetup() || !setup()) {
    return EXIT_FAILURE;
  }
  context_.get_resource_allocator()->clear_staging_buffers();

  auto &events{ Events::Get() };
  auto const nextFrame{[this, &events]() {
    events.prepareNextFrame();
    return wm_->poll();
  }};

  while (nextFrame()) {
    /* Clock */
    float tick = elapsed_time();
    last_frame_time_ = frame_time_;
    frame_time_ = tick;

    /* User Interface */
    ui_->beginFrame();
    {
      // [todo] OnViewportSizeChange
      {
        float xscale, yscale;
        glfwGetWindowContentScale(reinterpret_cast<GLFWwindow*>(wm_->get_handle()), &xscale, &yscale);
        ImGui::GetIO().FontGlobalScale = xscale;
      }
      build_ui();
      ImGui::Render();
    }
    ui_->endFrame();

    /* User frame */
    frame();
  }

  shutdown();

  return EXIT_SUCCESS;
}

float Application::elapsed_time() const {
  auto now{ std::chrono::high_resolution_clock::now() };
  return std::chrono::duration<float>(now - chrono_).count();
}

bool Application::presetup() {
  /* Singletons. */
  {
    Events::Initialize();
  }

  /* Create the main window surface. */
  if (wm_ = std::make_shared<Window>(); !wm_ || !wm_->init()) {
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

  /* Initialize the default renderer. */
  renderer_.init(context_, context_.get_resource_allocator(), surface_);

  /* Initialize User Interface. */
  if (ui_ = std::make_shared<UIController>(); !ui_ || !ui_->init(context_, renderer_, wm_)) {
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

void Application::shutdown() {
  CHECK_VK(vkDeviceWaitIdle(context_.get_device()));

  // User defined clean up.
  release();

  ui_->release(context_);

  renderer_.deinit();
  vkDestroySurfaceKHR(context_.get_instance(), surface_, nullptr);

  glfwTerminate();
  context_.deinit();

  Events::Deinitialize();
}

/* -------------------------------------------------------------------------- */
