# -----------------------------------------------------------------------------
# 
# Framework
# 
# -----------------------------------------------------------------------------

set(target vkframework)

# -----------------------------------------------------------------------------
# Source dependencies.
# -----------------------------------------------------------------------------

file(GLOB_RECURSE Sources CONFIGURE_DEPENDS
  LIST_DIRECTORIES FALSE
  ${FRAMEWORK_SOURCE_DIR}/*.cc
  ${FRAMEWORK_SOURCE_DIR}/*.h
)

# -----------------------------------------------------------------------------
# Platform specific Source dependencies.
# -----------------------------------------------------------------------------

set(FRAMEWORK_PLATFORM_SRC_DIR ${FRAMEWORK_SOURCE_DIR}/framework/core/platform)

# Exclude all platform specific codes..
list(FILTER Sources EXCLUDE REGEX "${FRAMEWORK_PLATFORM_SRC_DIR}/.*")
# ..but those at the root.
file(GLOB PlatformCommonSources CONFIGURE_DEPENDS
  LIST_DIRECTORIES FALSE
  ${FRAMEWORK_PLATFORM_SRC_DIR}/*.cc
  ${FRAMEWORK_PLATFORM_SRC_DIR}/*.h
)

# Add back target platform specifics sources.
if(ANDROID)
  set(FRAMEWORK_PLATFORM_SRC_DIR ${FRAMEWORK_PLATFORM_SRC_DIR}/android)
else()
  set(FRAMEWORK_PLATFORM_SRC_DIR ${FRAMEWORK_PLATFORM_SRC_DIR}/desktop)
endif()
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
set(FRAMEWORK_LIBRARIES ${target} CACHE INTERNAL "")

# -----------------------------------------------------------------------------
# Third Parties Dependencies.
# -----------------------------------------------------------------------------

if(OS_LINUX)
  # Required to build imgui_glfw_impl
  find_package(X11 REQUIRED)
endif()

# Vulkan
# Headers + Utility Libraries + Volk (dynamic extension loader)
set(VULKAN_SDK_TAG vulkan-sdk-1.4.321.0)
CPMAddPackage(
  NAME VulkanHeaders
  GITHUB_REPOSITORY KhronosGroup/Vulkan-Headers
  GIT_TAG ${VULKAN_SDK_TAG}
  DOWNLOAD_ONLY YES
)
CPMAddPackage(
  NAME VulkanUtilityLibraries
  GITHUB_REPOSITORY KhronosGroup/Vulkan-Utility-Libraries
  GIT_TAG ${VULKAN_SDK_TAG}
  DOWNLOAD_ONLY YES
)
CPMAddPackage(
  NAME volk
  GITHUB_REPOSITORY zeux/volk
  GIT_TAG ${VULKAN_SDK_TAG}
  OPTIONS
    "VULKAN_HEADERS_INSTALL_DIR ${VulkanHeaders_SOURCE_DIR}/include"
)
list(APPEND VULKAN_INCLUDE_DIR
  ${volk_SOURCE_DIR}
  ${VulkanHeaders_SOURCE_DIR}/include
  ${VulkanUtilityLibraries_SOURCE_DIR}/include
)

# Vulkan Memory Allocator (VMA)
CPMAddPackage(
  NAME VulkanMemoryAllocator
  GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG v3.2.0
)
set(VMA_INCLUDE_DIR  ${VulkanMemoryAllocator_SOURCE_DIR}/include)

# CGLTF
CPMAddPackage(
  NAME cgltf
  GITHUB_REPOSITORY jkuhlmann/cgltf
  GIT_TAG v1.14
)
set(CGLTF_INCLUDE_DIR ${cgltf_SOURCE_DIR})

# [wip] Draco (not currently supported)
# see https://github.com/google/draco/blob/1.5.7/BUILDING.md#cmake-build-configuration
# option(FRAMEWORK_HAS_DRACO OFF)
if(FRAMEWORK_HAS_DRACO)
  # CPMAddPackage(
  #   NAME Draco
  #   GITHUB_REPOSITORY google/draco
  #   GIT_TAG 1.5.7
  #   OPTIONS
  #     "DRACO_TRANSCODER_SUPPORTED OFF"
  # )
  # set(DRACO_INCLUDE_DIR  ${Draco_SOURCE_DIR})

  # list(APPEND SharedIncludeDirs ${DRACO_INCLUDE_DIR})
  # list(APPEND SharedLibs draco)
endif()

# MikkTSpace (build as Static Lib).
CPMAddPackage(
  NAME mikktspace
  GITHUB_REPOSITORY mmikk/MikkTSpace
  GIT_TAG 3e895b49d05ea07e4c2133156cfa94369e19e409
  DOWNLOAD_ONLY YES
)
add_library(mikktspace STATIC
  ${mikktspace_SOURCE_DIR}/mikktspace.c
  ${mikktspace_SOURCE_DIR}/mikktspace.h
)
set(MIKKTSPACE_INCLUDE_DIR ${mikktspace_SOURCE_DIR})

# libfmt, formatting library emulating std20.
CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 12.0.0
)
set(FMT_INCLUDE_DIR ${fmt_SOURCE_DIR}/include)

# linalg (single header linear algebra library).
CPMAddPackage(
  NAME linalg
  GITHUB_REPOSITORY sgorsten/linalg
  GIT_TAG v2.2
  DOWNLOAD_ONLY YES
)
set(LINALG_INCLUDE_DIR ${linalg_SOURCE_DIR})

# LibM.
findLibM()

# ImGui Docking.
if(TRUE)
  CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG docking
  )

  if(ANDROID)
    set(IMGUI_WM_BACKEND android)
  else()
    set(IMGUI_WM_BACKEND glfw)
  endif()

  list(APPEND IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui.h
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.h
    ${imgui_SOURCE_DIR}/backends/imgui_impl_${IMGUI_WM_BACKEND}.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_${IMGUI_WM_BACKEND}.h
  )
  source_group("Imgui" FILES ${IMGUI_SOURCES})
  set_source_files_properties(${IMGUI_SOURCES}
    PROPERTIES
      COMPILE_DEFINITIONS
        # Tell Imgui to use volk's vulkan header.
        IMGUI_IMPL_VULKAN_USE_VOLK
      COMPILE_FLAGS
        "-Wno-null-dereference"
  )

  target_sources(${target} PRIVATE ${IMGUI_SOURCES})
  set(IMGUI_INCLUDE_DIR ${imgui_SOURCE_DIR})
endif()

# -----------------------------------------------------------------------------
# Target properties.
# -----------------------------------------------------------------------------

target_link_libraries(${target}
  PUBLIC
    ${PlatformLibs}
    volk
    fmt
    ${LIBM_LIBRARIES}
  PRIVATE
    VulkanMemoryAllocator
    mikktspace
    ${X11_LIBRARIES}
)

target_include_directories(${target}
  PUBLIC
    ${FRAMEWORK_INCLUDE_DIR}
    ${PROJECT_THIRD_PARTY_DIR}
    ${PlatformIncludeDirs}
    ${VULKAN_INCLUDE_DIR}
    ${VMA_INCLUDE_DIR} #!
    ${FMT_INCLUDE_DIR}
    ${LINALG_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
  PRIVATE
    ${FRAMEWORK_SHADERS_DIR} #!
    ${CGLTF_INCLUDE_DIR}
    ${MIKKTSPACE_INCLUDE_DIR}
)

target_compile_definitions(${target}
  PUBLIC
    # For Windows build.
    _USE_MATH_DEFINES=1
    # Define VK_NO_PROTOTYPES to avoid including Vulkan prototypes
    # This is necessary because we are using volk to load Vulkan functions
    VK_NO_PROTOTYPES=1
  PRIVATE
    FRAMEWORK_HAS_DRACO=0
)

if(ANDROID)
  target_compile_definitions(${target}
    PUBLIC
      ASSETS_DIR=""
      FRAMEWORK_COMPILED_SHADERS_DIR="${FRAMEWORK_SPIRV_ASSETS_SUBDIR}/"
      VK_USE_PLATFORM_ANDROID_KHR=1
      ANDROID_NDK=1
  )
else()
  target_compile_definitions(${target}
    PUBLIC
      ASSETS_DIR="${PROJECT_ASSETS_DIR}/"
      FRAMEWORK_COMPILED_SHADERS_DIR="${FRAMEWORK_COMPILED_SHADERS_DIR}/" #!
  )
endif()

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

# should probably be inside the Sample App instead..
if(ANDROID)
  helpers_addNativeAppGlue(${target})
endif()

# -----------------------------------------------------------------------------
# Compile Framework shaders.
# -----------------------------------------------------------------------------

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
add_dependencies(${target} BuildFrameworkShaders)

# -----------------------------------------------------------------------------
