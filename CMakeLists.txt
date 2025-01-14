cmake_minimum_required(VERSION 3.21)

# -----------------------------------------------------------------------------
# CMake policies.
# -----------------------------------------------------------------------------

# Enable the use of "VERSION" for project setting in older cmake version.
if(POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif()

# Use <PackageName>_ROOT variables.
if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

# Honor CMAKE_REQUIRED_LIBRARIES in the include file check macros.
if(POLICY CMP0075)
  cmake_policy(SET CMP0075 NEW)
endif()

# -----------------------------------------------------------------------------
# Project.
# -----------------------------------------------------------------------------

set(PROJECT_NAME hello_vk)
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES CXX C)

# -----------------------------------------------------------------------------
# Paths.
# -----------------------------------------------------------------------------

set(PROJECT_ROOT_PATH               ${CMAKE_CURRENT_SOURCE_DIR})

set(PROJECT_THIRD_PARTY_DIR         ${PROJECT_ROOT_PATH}/thirdparty)
set(PROJECT_SOURCE_DIR              ${PROJECT_ROOT_PATH}/src)
set(PROJECT_BINARY_DIR              ${PROJECT_ROOT_PATH}/bin)

set(FRAMEWORK_PATH                  ${PROJECT_SOURCE_DIR}/framework)
set(SAMPLES_PATH                    ${PROJECT_SOURCE_DIR}/samples)

# -----------------------------------------------------------------------------
# CMake settings.
# -----------------------------------------------------------------------------

# CMake module path.
# list(APPEND CMAKE_MODULE_PATH       ${PROJECT_TOOLS_DIR}/cmake)

# Default CMake behavior.
set(CMAKE_VERBOSE_MAKEFILE          OFF)
set(CMAKE_COLOR_MAKEFILE            ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD   ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS   ON)
set(CMAKE_DISABLE_SOURCE_CHANGES    OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default standards with no extensions.
set(CMAKE_C_STANDARD                11)
set(CMAKE_CXX_STANDARD              20)
set(CMAKE_CXX_STANDARD_REQUIRED     ON)
set(CMAKE_CXX_EXTENSIONS            OFF)

# -----------------------------------------------------------------------------
# Custom functions
# -----------------------------------------------------------------------------

function(set_global_output_directory OUTPUT_DIR)
  # Default output directory.
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR} CACHE PATH "")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR} CACHE PATH "")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR} CACHE PATH "")
endfunction()

# Force output directory destination, especially for MSVC (@so7747857).
function(set_target_output_directory target ouput_dir)
  foreach(type RUNTIME LIBRARY ARCHIVE)
    set_target_properties(${target} PROPERTIES
      ${type}_OUTPUT_DIRECTORY         ${output_dir}
      ${type}_OUTPUT_DIRECTORY_DEBUG   ${output_dir}
      ${type}_OUTPUT_DIRECTORY_RELEASE ${output_dir}
    )
  endforeach()
endfunction()

set_global_output_directory(${PROJECT_BINARY_DIR})

# -----------------------------------------------------------------------------
# CMake includes
# -----------------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Vulkan SDK helpers
include(VulkanSDK)

# Download CPM.cmake
set(CPM_DOWNLOAD_VERSION 0.40.3)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
  message(STATUS "Downloading CPM.cmake...")
  file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif()

include(${CPM_DOWNLOAD_LOCATION})

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

# Include Volk
CPMAddPackage(
  NAME volk
  GITHUB_REPOSITORY zeux/volk
  GIT_TAG 1.4.304
)

# Include VMA
CPMAddPackage(
  NAME VulkanMemoryAllocator
  GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG v3.2.0
)

find_package(Vulkan REQUIRED)
find_package(GLFW 3.3 REQUIRED)

# -----------------------------------------------------------------------------

add_subdirectory(${FRAMEWORK_PATH})
add_subdirectory(${SAMPLES_PATH})

# -----------------------------------------------------------------------------
