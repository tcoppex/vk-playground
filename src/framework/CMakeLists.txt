# -----------------------------------------------------------------------------
# 
# 
# -----------------------------------------------------------------------------

set(target vkframework)

set(FRAMEWORK_ROOT_PATH          ${CMAKE_CURRENT_SOURCE_DIR})

# -----------------------------------------------------------------------------

file(GLOB_RECURSE
  Sources
  # LIST_DIRECTORIES FALSE
  ${FRAMEWORK_ROOT_PATH}/*.cc
)

add_library(${target} STATIC ${Sources})
# add_library(${target} SHARED ${Sources})

# -----------------------------------------------------------------------------

target_link_libraries(${target} PRIVATE
  ${Vulkan_LIBRARY}
  ${GLFW_LIBRARY}
  volk
  VulkanMemoryAllocator
)

target_include_directories(${target} PRIVATE
  ${PROJECT_SOURCE_DIR}

  ${Vulkan_INCLUDE_DIR}
  ${GLFW_INCLUDE_DIR}
)

target_compile_definitions(${target} PRIVATE
  # Define VK_NO_PROTOTYPES to avoid including Vulkan prototypes
  # This is necessary because we are using volk to load Vulkan functions
  VK_NO_PROTOTYPES
)

# -----------------------------------------------------------------------------

set(FRAMEWORK_LIBRARIES ${target} CACHE INTERNAL "")

# -----------------------------------------------------------------------------
