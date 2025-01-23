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

add_library(
  ${target}
  STATIC
  # SHARED
  ${Sources}
)

# -----------------------------------------------------------------------------

target_link_libraries(${target} PRIVATE
  ${Vulkan_LIBRARY}
  # ${GLFW_LIBRARY}
  glfw
  volk
  VulkanMemoryAllocator
)

target_include_directories(${target} PRIVATE
  ${PROJECT_SOURCE_DIR}
  ${Vulkan_INCLUDE_DIR}
  ${GLFW_INCLUDE_DIR}
  ${LINALG_INCLUDE_DIR}
  ${VOLK_INCLUDE_DIR}
  ${VMA_INCLUDE_DIR}
  ${PROJECT_THIRD_PARTY_DIR}
)

target_compile_definitions(${target} PRIVATE
  # Define VK_NO_PROTOTYPES to avoid including Vulkan prototypes
  # This is necessary because we are using volk to load Vulkan functions
  VK_NO_PROTOTYPES
)

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
