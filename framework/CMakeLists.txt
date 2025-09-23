# -----------------------------------------------------------------------------
# 
# Framework
# 
# -----------------------------------------------------------------------------

set(target vkframework)

set(FRAMEWORK_ROOT_PATH          ${CMAKE_CURRENT_SOURCE_DIR})
set(FRAMEWORK_SOURCE_DIR         ${FRAMEWORK_ROOT_PATH}/src)

set(FRAMEWORK_PLATFORM_SRC_DIR   ${FRAMEWORK_SOURCE_DIR}/framework/core/platform)

# -----------------------------------------------------------------------------
# Generic Dependencies.
# -----------------------------------------------------------------------------

# Libraries.
list(APPEND FrameworkLibs
  ${SharedLibs}
  ${MikkTSpace}
  VulkanMemoryAllocator
)

# Sources.
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS
  LIST_DIRECTORIES FALSE
  ${FRAMEWORK_SOURCE_DIR}/*.cc
  ${FRAMEWORK_SOURCE_DIR}/*.h
)

# SPIR-V Shaders.
compile_shaders(
  ${FRAMEWORK_SHADERS_DIR}
  ${FRAMEWORK_COMPILED_SHADERS_DIR}
  shaders_bin
  shaders_src
  ${FRAMEWORK_SHADERS_DIR}
)

add_custom_target(
  BuildFrameworkShaders ALL
  DEPENDS
    ${shaders_bin}
    ${shaders_src}
)

# -----------------------------------------------------------------------------
# Platform specific dependencies.
# -----------------------------------------------------------------------------

# Exclude all platform specific codes.
list(FILTER Sources EXCLUDE REGEX "${FRAMEWORK_PLATFORM_SRC_DIR}/.*")

# But keep those at the root..
file(GLOB PlatformCommonSources CONFIGURE_DEPENDS
  LIST_DIRECTORIES FALSE
  ${FRAMEWORK_PLATFORM_SRC_DIR}/*.cc
  ${FRAMEWORK_PLATFORM_SRC_DIR}/*.h
)

if (OS_ANDROID)
  set(FRAMEWORK_PLATFORM_SRC_DIR   ${FRAMEWORK_PLATFORM_SRC_DIR}/android)
else()
  set(FRAMEWORK_PLATFORM_SRC_DIR   ${FRAMEWORK_PLATFORM_SRC_DIR}/desktop)

  if (OS_LINUX)
    # Required to build imgui_glfw_impl
    find_package(X11 REQUIRED)
    list(APPEND FrameworkLibs ${X11_LIBRARIES})
  endif()
endif()

# Add Platform specifics sources.
file(GLOB_RECURSE PlatformSources CONFIGURE_DEPENDS
  LIST_DIRECTORIES FALSE
  ${FRAMEWORK_PLATFORM_SRC_DIR}/*.cc
  ${FRAMEWORK_PLATFORM_SRC_DIR}/*.h
)

list(APPEND Sources
  ${PlatformCommonSources}
  ${PlatformSources}
)

# -----------------------------------------------------------------------------
# Target library compilation.
# -----------------------------------------------------------------------------

add_library(${target} STATIC ${Sources})
add_dependencies(${target} BuildFrameworkShaders)

# -----------------------------------------------------------------------------
# Target properties.
# -----------------------------------------------------------------------------

target_link_libraries(${target} PRIVATE ${FrameworkLibs})

target_include_directories(${target} PRIVATE
  ${SharedIncludeDirs}
  ${MikkTSpace_SOURCE_DIR}
)

target_compile_definitions(${target} PRIVATE
  ${CustomDefinitions}
)

if(FRAMEWORK_HAS_DRACO)
  target_compile_definitions(${target} PRIVATE FRAMEWORK_HAS_DRACO=1)
else()
  target_compile_definitions(${target} PRIVATE FRAMEWORK_HAS_DRACO=0)
endif()

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

if (OS_ANDROID)
  helpers_addNativeAppGlue(${target})
endif()

# -----------------------------------------------------------------------------
# Global variables.
# -----------------------------------------------------------------------------

set(FRAMEWORK_LIBRARIES ${target} CACHE INTERNAL "")

# -----------------------------------------------------------------------------
