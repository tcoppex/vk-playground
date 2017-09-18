cmake_minimum_required(VERSION 2.8.12)

project(vk-playground)


# -------------------------------------------------------
# Paths
# -------------------------------------------------------

# cmake extensions path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# project paths
set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin)

# -------------------------------------------------------
# CMake includes
# -------------------------------------------------------

# Custom Cmake helpers
include(Helpers)

# Vulkan SDK helpers
include(VulkanSDK)

# -------------------------------------------------------
# Generic properties
# -------------------------------------------------------

set_global_output_directory(${OUTPUT_DIR})

# -------------------------------------------------------
# Generic dependencies
# -------------------------------------------------------

find_package(Vulkan REQUIRED)
find_package(GLFW 3.2 REQUIRED)

# -------------------------------------------------------
# General Compiler settings
# -------------------------------------------------------

# Generic definitions
add_definitions(
  -DVK_PROTOTYPES
)

if(CMAKE_HOST_UNIX)
  find_gcc(4.8)

  set(COMMON_COMPILE_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
  set(COMMON_COMPILE_FLAGS "${COMMON_COMPILE_FLAGS} -fvisibility=hidden -fno-strict-aliasing -fno-builtin-memcmp")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_COMPILE_FLAGS} -std=c99")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_COMPILE_FLAGS} -std=c++11")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")

  add_definitions(
    -DVK_USE_PLATFORM_XCB_KHR
  )
endif(CMAKE_HOST_UNIX)

include_directories(
  ${SOURCE_DIR}
  ${VULKAN_INCLUDE_DIR}
  ${GLFW_INCLUDE_DIR}
  ${GLM_INCLUDE_DIR}
)

# -------------------------------------------------------
# Shader Paths
# -------------------------------------------------------

# relative and global shaders path
set(SHADERS_DIR shaders)
set(INCLUDE_SHADERS_DIR ${CMAKE_SOURCE_DIR}/${SHADERS_DIR})
# Spirv directory
set(COMPILED_SHADERS_DIR ${CMAKE_SOURCE_DIR}/spirv)


# -------------------------------------------------------
# Files
# -------------------------------------------------------

add_subdirectory(${SOURCE_DIR})

# -------------------------------------------------------
# Installation
# -------------------------------------------------------

# TODO
