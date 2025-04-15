/* -------------------------------------------------------------------------- */

#define IMGUI_WRAPPER_IMPL
#include "framework/wm/ui/ui_controller.h"

#include "framework/backend/context.h"
#include "framework/renderer/renderer.h"

/* -------------------------------------------------------------------------- */

bool UIController::init(Context const& context, Renderer const& renderer, std::shared_ptr<WMInterface> wm) {
  IMGUI_CHECKVERSION();

  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  if (!ImGui_ImplGlfw_InitForVulkan(reinterpret_cast<GLFWwindow*>(wm->get_handle()), true)) {
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
  CHECK_VK(vkCreateDescriptorPool(context.get_device(), &desc_pool_info, nullptr, &imgui_descriptor_pool_));

  VkFormat const formats[]{
    renderer.get_color_attachment().format
  };
  VkFormat const depth_stencil_format{
    renderer.get_depth_stencil_attachment().format
  };
  auto const main_queue{ context.get_queue() };
  ImGui_ImplVulkan_InitInfo info{
    .Instance = context.get_instance(),
    .PhysicalDevice = context.get_gpu(),
    .Device = context.get_device(),
    .QueueFamily = main_queue.family_index,
    .Queue = main_queue.queue,
    .DescriptorPool = imgui_descriptor_pool_,
    .MinImageCount = 2,
    .ImageCount = renderer.get_swapchain().get_image_count(),
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

// ----------------------------------------------------------------------------

void UIController::release(Context const& context) {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  vkDestroyDescriptorPool(context.get_device(), imgui_descriptor_pool_, nullptr);
}

// ----------------------------------------------------------------------------

void UIController::beginFrame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  setupStyles();
}

// ----------------------------------------------------------------------------

void UIController::endFrame() {
  if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
  ImGui::EndFrame();
}

// ----------------------------------------------------------------------------

void UIController::setupStyles() {
  typedef ImVec4 (*srgbFunction)(float, float, float, float);
  srgbFunction   passthrough = [](float r, float g, float b, float a) -> ImVec4 { return ImVec4(r, g, b, a); };
  srgbFunction   toLinear    = [](float r, float g, float b, float a) -> ImVec4 {
    auto toLinearScalar = [](float u) -> float {
      return u <= 0.04045 ? 25 * u / 323.f : powf((200 * u + 11) / 211.f, 2.4f);
    };
    return ImVec4(toLinearScalar(r), toLinearScalar(g), toLinearScalar(b), a);
  };
  srgbFunction srgb = true ? toLinear : passthrough;

  ImGui::StyleColorsDark();

  ImGuiStyle& style                  = ImGui::GetStyle();
  style.WindowRounding               = 0.0f;
  style.WindowBorderSize             = 0.0f;
  style.ColorButtonPosition          = ImGuiDir_Right;
  style.FrameRounding                = 2.0f;
  style.FrameBorderSize              = 1.0f;
  style.GrabRounding                 = 4.0f;
  style.IndentSpacing                = 12.0f;
  style.Colors[ImGuiCol_WindowBg]    = srgb(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_MenuBarBg]   = srgb(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_ScrollbarBg] = srgb(0.2f, 0.2f, 0.2f, 1.0f);
  style.Colors[ImGuiCol_PopupBg]     = srgb(0.14f, 0.14f, 0.14f, 1.0f);
  style.Colors[ImGuiCol_Border]      = srgb(0.4f, 0.4f, 0.4f, 0.5f);
  style.Colors[ImGuiCol_FrameBg]     = srgb(0.05f, 0.05f, 0.05f, 0.5f);

  // Normal
  ImVec4 normal_color = srgb(0.47, 0.47, 0.53f, 1.0f);
  std::vector<ImGuiCol> to_change_nrm;
  to_change_nrm.push_back(ImGuiCol_Header);
  to_change_nrm.push_back(ImGuiCol_SliderGrab);
  to_change_nrm.push_back(ImGuiCol_Button);
  to_change_nrm.push_back(ImGuiCol_CheckMark);
  to_change_nrm.push_back(ImGuiCol_ResizeGrip);
  to_change_nrm.push_back(ImGuiCol_TextSelectedBg);
  to_change_nrm.push_back(ImGuiCol_Separator);
  to_change_nrm.push_back(ImGuiCol_FrameBgActive);
  for(auto c : to_change_nrm)
  {
    style.Colors[c] = normal_color;
  }

  // Active
  ImVec4 active_color = srgb(0.365f, 0.365f, 0.425f, 1.0f);
  std::vector<ImGuiCol> to_change_act;
  to_change_act.push_back(ImGuiCol_HeaderActive);
  to_change_act.push_back(ImGuiCol_SliderGrabActive);
  to_change_act.push_back(ImGuiCol_ButtonActive);
  to_change_act.push_back(ImGuiCol_ResizeGripActive);
  to_change_act.push_back(ImGuiCol_SeparatorActive);
  for(auto c : to_change_act)
  {
    style.Colors[c] = active_color;
  }

  // Hovered
  ImVec4 hovered_color = srgb(0.565f, 0.565f, 0.625f, 1.0f);
  std::vector<ImGuiCol> to_change_hover;
  to_change_hover.push_back(ImGuiCol_HeaderHovered);
  to_change_hover.push_back(ImGuiCol_ButtonHovered);
  to_change_hover.push_back(ImGuiCol_FrameBgHovered);
  to_change_hover.push_back(ImGuiCol_ResizeGripHovered);
  to_change_hover.push_back(ImGuiCol_SeparatorHovered);
  for(auto c : to_change_hover)
  {
    style.Colors[c] = hovered_color;
  }

  style.Colors[ImGuiCol_TitleBgActive]    = srgb(0.465f, 0.465f, 0.465f, 1.0f);
  style.Colors[ImGuiCol_TitleBg]          = srgb(0.125f, 0.125f, 0.125f, 1.0f);
  style.Colors[ImGuiCol_Tab]              = srgb(0.05f, 0.05f, 0.05f, 0.5f);
  style.Colors[ImGuiCol_TabHovered]       = srgb(0.465f, 0.495f, 0.525f, 1.0f);
  style.Colors[ImGuiCol_TabActive]        = srgb(0.282f, 0.290f, 0.302f, 1.0f);
  style.Colors[ImGuiCol_ModalWindowDimBg] = srgb(0.465f, 0.465f, 0.465f, 0.350f);

  ImGui::SetColorEditOptions(ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel);
}

/* -------------------------------------------------------------------------- */
