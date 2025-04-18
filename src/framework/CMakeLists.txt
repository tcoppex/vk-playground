# -----------------------------------------------------------------------------
# 
# 
# -----------------------------------------------------------------------------

set(target vkframework)

set(FRAMEWORK_ROOT_PATH             ${CMAKE_CURRENT_SOURCE_DIR})

# -----------------------------------------------------------------------------

compile_shaders(
  ${FRAMEWORK_SHADERS_DIR}
  ${FRAMEWORK_COMPILED_SHADERS_DIR}
  shaders_bin
  shaders_src
  ${FRAMEWORK_SHADERS_DIR}
)

# -----------------------------------------------------------------------------

file(GLOB_RECURSE
  Sources
  # LIST_DIRECTORIES FALSE
  ${FRAMEWORK_ROOT_PATH}/*.cc
)

add_library(
  ${target}
  STATIC # SHARED
  ${Sources}
  ${shaders_src}
  ${shaders_bin}
)

# -----------------------------------------------------------------------------

target_link_libraries(${target} PRIVATE
  ${SharedLibs}
  VulkanMemoryAllocator
)

target_include_directories(${target} PRIVATE
  ${SharedIncludeDirs}
)

target_compile_definitions(${target} PRIVATE
  FRAMEWORK_COMPILED_SHADERS_DIR="${FRAMEWORK_COMPILED_SHADERS_DIR}/"

  # Define VK_NO_PROTOTYPES to avoid including Vulkan prototypes
  # This is necessary because we are using volk to load Vulkan functions
  VK_NO_PROTOTYPES
  _USE_MATH_DEFINES
)

target_sources(${target} PRIVATE ${IMGUI_SOURCES})
target_compile_definitions(${target} PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)

target_compile_options(${target} PRIVATE
  "${CXX_FLAGS}"
  "$<$<CONFIG:Debug>:${CXX_FLAGS_DEBUG}>"
  "$<$<CONFIG:Release>:${CXX_FLAGS_RELEASE}>"
  "$<$<CONFIG:DebugWithRelInfo>:${CXX_FLAGS_RELWITHDEBINFO}>"
)

set_target_properties(${target}
  PROPERTIES
    LINK_FLAGS "${CXX_LINK_FLAGS}"
)

set_target_output_directory(${target} ${PROJECT_BINARY_DIR})

# -----------------------------------------------------------------------------

set(FRAMEWORK_LIBRARIES ${target} CACHE INTERNAL "")

# -----------------------------------------------------------------------------
