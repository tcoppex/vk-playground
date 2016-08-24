cmake_minimum_required(VERSION 2.8.12)

project(vk_playground)


# -------------------------------------------------------
# Paths
# -------------------------------------------------------

# cmake extensions path
# Note : CMAKE_MODULE_PATH does not seems to work for includes, 
#        only for find_package.
set(MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${MODULE_PATH}")

# project paths
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")


# -------------------------------------------------------
# CMake includes
# -------------------------------------------------------

# Check compiler validity
include(${MODULE_PATH}/CheckCompiler.cmake)

# Vulkan SDK helpers
include(${MODULE_PATH}/VulkanSDK.cmake)


# -------------------------------------------------------
# Dependencies
# -------------------------------------------------------

find_package(Vulkan REQUIRED)
find_package(GLFW 3 REQUIRED)


# -------------------------------------------------------
# General Compiler settings
# -------------------------------------------------------

if(CMAKE_HOST_UNIX)
  find_gcc(4.8)

  set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++11")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
endif(CMAKE_HOST_UNIX)

# Vulkan definitions
add_definitions(
  -DEXTERNAL_SPV
  -DVK_PROTOTYPES
)


# -------------------------------------------------------
# Files
# -------------------------------------------------------

#add_subdirectory($(SRC_DIR))

# sources to compile
file(GLOB_RECURSE Sources ${SRC_DIR}/*.cc)
# others files for IDE integration
file(GLOB_RECURSE Others ${SRC_DIR}/*.h)


# -------------------------------------------------------
# Build target
# -------------------------------------------------------

set(TARGET_NAME ${CMAKE_PROJECT_NAME})
add_executable(${TARGET_NAME} ${Sources} ${Others})

include_directories(
  ${SRC_DIR}
  ${VULKAN_INCLUDE_DIRS}
  ${GLFW_INCLUDE_DIR}
)

target_link_libraries(${TARGET_NAME}
  ${VULKAN_LIBRARY}
  ${GLFW_LIBRARY}
)


# -------------------------------------------------------
# Installation
# -------------------------------------------------------

# TODO