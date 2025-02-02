#include "framework/application.h"
#include "framework/wm/events.h"
#include "framework/wm/window.h"

/* -------------------------------------------------------------------------- */

#define WINDOW_SCALE          0.5f
#define MONITOR_SCALE         1.25f
#define WINDOW_SIZE_FACTOR    (WINDOW_SCALE / MONITOR_SCALE)

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
    frame_time_ = get_elapsed_time();
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

  renderer_.deinit();
  vkDestroySurfaceKHR(context_.get_instance(), surface_, nullptr);
  glfwTerminate();
  context_.deinit();

  Events::Deinitialize();
}

/* -------------------------------------------------------------------------- */
