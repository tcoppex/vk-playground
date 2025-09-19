#include "framework/application.h"
#include "framework/core/platform/events.h"

#include "framework/core/platform/desktop/window/window.h" //

#if defined(_WIN32)
  #include <windows.h> // for SetConsoleOutputCP
#endif

/* -------------------------------------------------------------------------- */

int Application::run() {
  if (!presetup() || !setup()) {
    return EXIT_FAILURE;
  }
  context_.allocator().clear_staging_buffers();

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
    build_ui();
    ui_->endFrame();

    /* User frame */
    frame();
  }

  shutdown();

  return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------------

float Application::elapsed_time() const {
  auto now{ std::chrono::high_resolution_clock::now() };
  return std::chrono::duration<float>(now - chrono_).count();
}

// ----------------------------------------------------------------------------

bool Application::presetup() {
#if defined(_WIN32)
    // Switch Windows console to UTF-8
    SetConsoleOutputCP(CP_UTF8);
#endif

  /* Singletons. */
  {
    Events::Initialize();
  }

  /* Create the main window surface. */
  if (wm_ = std::make_unique<Window>(); !wm_ || !wm_->init()) {
    return false;
  }

  /* Initialize the Vulkan context. */
  if (!context_.init(wm_->getVulkanInstanceExtensions())) {
    return false;
  }

  /* Create the context surface. */
  if (auto res = wm_->createWindowSurface(context_.instance(), &surface_); CHECK_VK(res)) {
    return false;
  }

  /* Initialize the default renderer. */
  renderer_.init(context_, context_.allocator_ptr(), surface_);

  /* Initialize User Interface. */
  if (ui_ = std::make_unique<UIController>(); !ui_ || !ui_->init(renderer_, *wm_)) {
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

// ----------------------------------------------------------------------------

void Application::shutdown() {
  CHECK_VK(vkDeviceWaitIdle(context_.device()));

  // User defined clean up.
  release();

  ui_->release(context_);

  renderer_.deinit();
  vkDestroySurfaceKHR(context_.instance(), surface_, nullptr);

  glfwTerminate();
  context_.deinit();

  Events::Deinitialize();
}

/* -------------------------------------------------------------------------- */
