cmake_minimum_required(VERSION 3.22.1)

# -----------------------------------------------------------------------------
# Project.
# -----------------------------------------------------------------------------

set(PROJECT_NAME vkplayground)

project(${PROJECT_NAME} VERSION 0.2 LANGUAGES CXX C)

# -----------------------------------------------------------------------------
# Paths.
# -----------------------------------------------------------------------------

set(PROJECT_ROOT_PATH               ${CMAKE_CURRENT_SOURCE_DIR})

set(FRAMEWORK_PATH                  ${PROJECT_ROOT_PATH}/framework)
set(FRAMEWORK_SOURCE_DIR            ${FRAMEWORK_PATH}/src) # [wip] !
set(FRAMEWORK_SHADERS_DIR           ${FRAMEWORK_SOURCE_DIR}/framework/shaders) #!

set(ANDROID_BUILD_DIR               ${PROJECT_ROOT_PATH}/android)
set(SAMPLES_PATH                    ${PROJECT_ROOT_PATH}/samples)
set(PROJECT_ASSETS_DIR              ${PROJECT_ROOT_PATH}/assets)
set(PROJECT_THIRD_PARTY_DIR         ${PROJECT_ROOT_PATH}/third_party)

set(PROJECT_BINARY_DIR              ${PROJECT_ROOT_PATH}/bin)
set(FRAMEWORK_COMPILED_SHADERS_DIR  ${PROJECT_BINARY_DIR}/spirv)

# (important)
# Currently the framework spirv directory is also used by the Android gradle build
# to migrate them to samples APK assets directory, so we need to specify a distinct
# subdir for them to be in conflict with others assets (shaders or otherwhise).
# [better alternative: build the framework as an AAR library]
set(FRAMEWORK_SPIRV_ASSETS_SUBDIR   framework_shaders)
set(FRAMEWORK_COMPILED_SHADERS_DIR  ${FRAMEWORK_COMPILED_SHADERS_DIR}/${FRAMEWORK_SPIRV_ASSETS_SUBDIR})

# -----------------------------------------------------------------------------
# CMake settings.
# -----------------------------------------------------------------------------

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
function(set_target_output_directory target output_dir)
  foreach(type RUNTIME LIBRARY ARCHIVE)
    set_target_properties(${target} PROPERTIES
      ${type}_OUTPUT_DIRECTORY         ${output_dir}
      ${type}_OUTPUT_DIRECTORY_DEBUG   ${output_dir}
      ${type}_OUTPUT_DIRECTORY_RELEASE ${output_dir}
    )
  endforeach()
endfunction()

function(helpers_findLibM)
  # Chuck Atkins @ https://cmake.org/pipermail/cmake/2019-March/069168.html
  include(CheckCSourceCompiles)
  set(LIBM_TEST_SOURCE "#include<math.h>\nint main(){float f = sqrtf(4.0f);}")
  check_c_source_compiles("${LIBM_TEST_SOURCE}" HAVE_MATH)
  if(HAVE_MATH)
    set(LIBM_LIBRARIES)
  else()
    set(CMAKE_REQUIRED_LIBRARIES m)
    check_c_source_compiles("${LIBM_TEST_SOURCE}" HAVE_LIBM_MATH)
    unset(CMAKE_REQUIRED_LIBRARIES)
    if(NOT HAVE_LIBM_MATH)
      message(FATAL_ERROR "Unable to use C math library functions.")
    endif()
    set(LIBM_LIBRARIES m)
  endif()
endfunction(helpers_findLibM)

function(download_asset assets_dir url)
  file(MAKE_DIRECTORY ${assets_dir})
  get_filename_component(filename ${url} NAME)
  set(dest "${assets_dir}/${filename}")
  if(NOT EXISTS ${dest})
    message(STATUS "Downloading ${filename}...")
    file(DOWNLOAD
      ${url}
      ${dest}
      SHOW_PROGRESS
      STATUS status
      LOG log
    )
    list(GET status 0 status_code)
    if(NOT status_code EQUAL 0)
      message(FATAL_ERROR "Failed to download ${url}: ${log}")
    endif()
  else()
    message(STATUS "${filename} already exists, skipping download")
  endif()
endfunction()

function(download_model url)
  download_asset(${PROJECT_ASSETS_DIR}/models/ ${url})
endfunction()

function(download_texture url)
  download_asset(${PROJECT_ASSETS_DIR}/textures/ ${url})
endfunction()

# -----------------------------------------------------------------------------
# Custom macros
# -----------------------------------------------------------------------------

## {MACRO} Setup the default parameters for a target.
##   @TARGET        : Unique name referencing the library as dependency.
##   @INCLUDE_DIRS  : List of path for include files.
##   @LIBRARIES     : List of libraries to link against.
##   @DEFINITIONS   : List of definitions.
macro(helpers_setupTarget)
  set(oneValueArgs TARGET)
  set(multiValueArgs INCLUDE_DIRECTORIES LIBRARIES DEFINITIONS)
  cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueArgs}" "${ARGN}")
  target_compile_options(
    ${ARG_TARGET}
    PRIVATE
    "${CXX_FLAGS}"
    "$<$<CONFIG:Debug>:${CXX_FLAGS_DEBUG}>"
    "$<$<CONFIG:Release>:${CXX_FLAGS_RELEASE}>"
    "$<$<CONFIG:DebugWithRelInfo>:${CXX_FLAGS_RELWITHDEBINFO}>"
  )
  target_include_directories(${ARG_TARGET} PRIVATE "${ARG_INCLUDE_DIRECTORIES}")
  target_link_libraries(${ARG_TARGET} PRIVATE "${ARG_LIBRARIES}")
  target_compile_definitions(${ARG_TARGET} PRIVATE "${ARG_DEFINITIONS}")
  set_target_properties(${ARG_TARGET} PROPERTIES LINK_FLAGS "${CXX_LINK_FLAGS}")
endmacro()

# -----------------------------------------------------------------------------
# Internal configuration attributes.
# -----------------------------------------------------------------------------

# Detects if the build is flagged for full debug mode.
if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(DEBUG TRUE)
  message(STATUS "DEBUG build.")
else()
  set(DEBUG FALSE)
endif()

# Detects the build platform.
if(UNIX AND NOT APPLE AND NOT ANDROID AND NOT WEBGL)
  set(OS_NAME "Linux")
  set(OS_LINUX TRUE)
elseif(WIN32)
  set(OS_NAME "Windows")
  set(OS_WIN32 TRUE)
elseif(ANDROID)
  set(OS_NAME "Android")
  set(OS_ANDROID TRUE)
else()
  message(WARNING "The \"${CMAKE_SYSTEM_NAME}\" platform is not supported.")
endif()

message(STATUS "Configured for system \"${OS_NAME}\"")

# Customize based on platform.
if (ANDROID)
  # --------------------------------------------------------
  if (${ANDROID_PLATFORM_LEVEL} LESS 29)
    message(FATAL_ERROR "Android is not supported before API level 29.")
  endif()

  # Capture renamed definitions.
  if(NOT DEFINED ANDROID_ABI)
    set(ANDROID_ABI ${CMAKE_ANDROID_ARCH_ABI})
  endif()
  if(NOT DEFINED ANDROID_NDK)
    set(ANDROID_NDK ${CMAKE_ANDROID_NDK})
  endif()

  # Setup clang tidy.
  if (DEBUG)
    # message(STATUS "clang-tidy enabled.")
    # set(CLANG_TIDY_PATH
    #   "${ANDROID_TOOLCHAIN_ROOT}/bin/clang-tidy${ANDROID_TOOLCHAIN_SUFFIX}"
    # )
    # set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_PATH} "-checks=android-*")
  endif()
  # --------------------------------------------------------
else()
  # On Desktop force all outputs to the binary directory.
  set_global_output_directory(${PROJECT_BINARY_DIR})
endif()

# Detects the current compiler.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(USE_GCC TRUE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?[Cc]lang")
  set(USE_CLANG TRUE)
elseif(MSVC)
  set(USE_MSVC TRUE)
else()
  message(WARNING "The \"${CMAKE_CXX_COMPILER_ID}\" compiler is not officially supported.")
endif()

# -----------------------------------------------------------------------------
# Generic compiler settings.
# -----------------------------------------------------------------------------

if(USE_GCC OR USE_CLANG)
  # This is a mix of warning options not available in all GCC / Clang version.
  list(APPEND CXX_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wmisleading-indentation
    -Wduplicated-cond
    -Wcast-align
    -Wlogical-op
    -Wnull-dereference
    -Wdisabled-optimization
    -Wvector-operation-performance
    -Wformat=2 -Wformat-truncation -Wformat-overflow
    -Wno-unknown-pragmas
    -Wno-unused-function
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-invalid-offsetof
    # -Wdouble-promotion
    # -Wuseless-cast
    # -Wzero-as-null-pointer-constant
    # -Wundef
  )

  # Clang specific warnings.
  if(USE_CLANG)
    list(APPEND CXX_FLAGS
      -Wno-unknown-warning-option
      -Wno-nullability-extension
      -Wno-unused-lambda-capture
      -Wno-missing-braces #!
    )
  endif()

  # Features.
  list(APPEND CXX_FLAGS
    -fdiagnostics-color=always
    # -fno-strict-aliasing
    # -fno-builtin-memcmp
    # -fno-common
    # -fno-exceptions
    # -ffunction-sections
    # -fdata-sections
    # -ftree-loop-distribution
    # -funroll-loops
  )

  list(APPEND CXX_FLAGS_DEBUG           -O0 -g3)
  list(APPEND CXX_FLAGS_RELEASE         -O2 -msse4.1 -s)
  list(APPEND CXX_FLAGS_RELWITHDEBINFO  -O2 -msse4.1 -g1)

  # Linker flags.
  list(APPEND CustomLinkFlags
    -Wl,--build-id
    # -Wl,--gc-sections
  )

  # Definitions.
  if (ANDROID)
    list(APPEND CustomDefinitions
      ANDROID_NDK=1
      VK_USE_PLATFORM_ANDROID_KHR=1
    )
  endif()
elseif(USE_MSVC)
  # list(APPEND CXX_FLAGS
  #   /W4           # Warning level 4 (strict warnings, less noisy than /Wall)
  #   /w14640       # Warn about thread-unsafe static member initialization
  #   /w14928       # Warn about illegal copy-initialization
  #   /w14265       # class has virtual functions but non-virtual destructor
  #   /w14555       # Warn about expressions with no effect/side-effect
  #   /wd4100       # Disable: "unused parameter" warnings
  #   /MP           # build with multiple process.
  #   #/EHsc-       # no exception
  #   #/GR-         # disable RTTI
  #   /permissive-
  # )
  # list(APPEND CXX_FLAGS_DEBUG           /Zi)
  # list(APPEND CXX_FLAGS_RELEASE         /Ox /INCREMENTAL:NO /DEBUG:NONE /DNDEBUG)
  # list(APPEND CXX_FLAGS_RELWITHDEBINFO  ${CXX_FLAGS_RELEASE} /Ox /Zi)

  # list(APPEND CustomDefinitions
  #   NOMINMAX
  #   WIN32
  #   _WINDOWS
  #   _CRT_SECURE_NO_WARNINGS
  #   # _HAS_EXCEPTIONS=0
  #   VC_EXTRALEAN
  #   WIN32_LEAN_AND_MEAN
  # )
endif()

# Transform linker flags from a list to a string.
foreach(flag ${CustomLinkFlags})
  set(CXX_LINK_FLAGS "${CXX_LINK_FLAGS} ${flag}")
endforeach()

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
  file(DOWNLOAD
    https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
    ${CPM_DOWNLOAD_LOCATION}
  )
endif()

# Set a share cache for CPM if none exist.
if(NOT DEFINED CPM_SOURCE_CACHE)
  set(CPM_SOURCE_CACHE "${PROJECT_THIRD_PARTY_DIR}/.cpmlocalcache" CACHE PATH "")
endif()

include(${CPM_DOWNLOAD_LOCATION})

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

if(ANDROID)
  # [SHARED] Android specifics.
  find_library(ANDROID_LIBRARY     NAMES android)
  find_library(ANDROID_LOG_LIBRARY NAMES log)

  list(APPEND SharedLibs
    ${ANDROID_LIBRARY}
    ${ANDROID_LOG_LIBRARY}
  )

  # [OBJECT] Android Native App Glue.
  find_path(
    ANDROID_NATIVE_APP_GLUE_PATH
      android_native_app_glue.h
    PATHS
      "${ANDROID_NDK}/sources/android/native_app_glue"
  )
  add_library(
    android_native_app_glue
    OBJECT
      "${ANDROID_NATIVE_APP_GLUE_PATH}/android_native_app_glue.c"
  )
  target_include_directories(
    android_native_app_glue
    PUBLIC
      "${ANDROID_NATIVE_APP_GLUE_PATH}"
  )
  target_compile_options(
    android_native_app_glue PRIVATE -Wno-unused-parameter
  )

  ## {MACRO} Properly add the Native App Glue dependencies to its consuming target.
  macro(helpers_addNativeAppGlue TargetName)
    target_include_directories(${TargetName} PUBLIC ${ANDROID_NATIVE_APP_GLUE_PATH})
    target_link_libraries(${TargetName} PUBLIC $<TARGET_OBJECTS:android_native_app_glue>)
  endmacro()

  # (Alternatively just uncomment the following line.)
  # set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

else() # DESKTOP

  # GLFW
  CPMAddPackage(
    NAME glfw
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.4
    OPTIONS
      "GLFW_BUILD_DOCS OFF"
      "GLFW_BUILD_EXAMPLES OFF"
      "GLFW_BUILD_TESTS OFF"
      "GLFW_BUILD_WAYLAND OFF"
      "GLFW_INSTALL OFF"
  )
  set(GLFW_INCLUDE_DIR ${glfw_SOURCE_DIR}/include)

  list(APPEND SharedLibs
    glfw
  )

  list(APPEND SharedIncludeDirs
    ${GLFW_INCLUDE_DIR}
  )

endif()

# Vulkan
# Headers + Utility Libraries + Volk (dynamic extension loader)
set(VULKAN_SDK_TAG vulkan-sdk-1.4.321.0)
CPMAddPackage(
  NAME Vulkan-Headers
  GITHUB_REPOSITORY KhronosGroup/Vulkan-Headers
  GIT_TAG ${VULKAN_SDK_TAG}
  DOWNLOAD_ONLY YES
)
CPMAddPackage(
  NAME Vulkan-Utility-Libraries
  GITHUB_REPOSITORY KhronosGroup/Vulkan-Utility-Libraries
  GIT_TAG ${VULKAN_SDK_TAG}
  DOWNLOAD_ONLY YES
)
CPMAddPackage(
  NAME volk
  GITHUB_REPOSITORY zeux/volk
  GIT_TAG ${VULKAN_SDK_TAG}
  OPTIONS
    "VULKAN_HEADERS_INSTALL_DIR ${Vulkan-Headers_SOURCE_DIR}/include"
)
list(APPEND VULKAN_INCLUDE_DIR
  ${volk_SOURCE_DIR}
  ${Vulkan-Headers_SOURCE_DIR}/include
  ${Vulkan-Utility-Libraries_SOURCE_DIR}/include
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
option(FRAMEWORK_HAS_DRACO OFF)
if(FRAMEWORK_HAS_DRACO)
  CPMAddPackage(
    NAME Draco
    GITHUB_REPOSITORY google/draco
    GIT_TAG 1.5.7
    OPTIONS
      "DRACO_TRANSCODER_SUPPORTED OFF"
  )
  set(DRACO_INCLUDE_DIR  ${Draco_SOURCE_DIR})

  list(APPEND SharedIncludeDirs ${DRACO_INCLUDE_DIR})
  list(APPEND SharedLibs draco)
endif()

# linalg (linear algebra single header)
CPMAddPackage(
  NAME linalg
  GITHUB_REPOSITORY sgorsten/linalg
  GIT_TAG v2.2
  DOWNLOAD_ONLY YES
)
set(LINALG_INCLUDE_DIR ${linalg_SOURCE_DIR})

# MikkTSpace (build as Static Lib)
CPMAddPackage(
  NAME MikkTSpace
  GITHUB_REPOSITORY mmikk/MikkTSpace
  GIT_TAG 3e895b49d05ea07e4c2133156cfa94369e19e409
)
set(MikkTSpace mikktspace)
add_library(
  ${MikkTSpace}
  STATIC
  ${MikkTSpace_SOURCE_DIR}/mikktspace.c
  ${MikkTSpace_SOURCE_DIR}/mikktspace.h
)

# ImGui
if (TRUE)
  CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG docking
  )

  if (ANDROID)
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
endif()

CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 12.0.0
)
set(FMT_INCLUDE_DIR  ${fmt_SOURCE_DIR}/include)
message(STATUS ${FMT_INCLUDE_DIR})

# Sometimes we need to link libm too.
helpers_findLibM()

# -----------------------------------------------------------------------------
# Shared dependencies.
# -----------------------------------------------------------------------------

list(APPEND SharedLibs
  volk
  ${LIBM_LIBRARIES}
  fmt
)

list(APPEND SharedIncludeDirs
  ${FRAMEWORK_SOURCE_DIR} #!
  ${FRAMEWORK_SHADERS_DIR} #!
  ${PROJECT_THIRD_PARTY_DIR}
  ${VULKAN_INCLUDE_DIR}
  ${VMA_INCLUDE_DIR}
  ${CGLTF_INCLUDE_DIR}
  ${LINALG_INCLUDE_DIR}
  ${imgui_SOURCE_DIR}
  ${FMT_INCLUDE_DIR}
)

list(APPEND CustomDefinitions
  # Define VK_NO_PROTOTYPES to avoid including Vulkan prototypes
  # This is necessary because we are using volk to load Vulkan functions
  VK_NO_PROTOTYPES=1
  _USE_MATH_DEFINES=1
)

# -----------------------------------------------------------------------------

if (ANDROID)
  list(APPEND CustomDefinitions
    ASSETS_DIR=""
    FRAMEWORK_COMPILED_SHADERS_DIR="${FRAMEWORK_SPIRV_ASSETS_SUBDIR}/"
  )
else()
  list(APPEND CustomDefinitions
    ASSETS_DIR="${PROJECT_ASSETS_DIR}/"
    FRAMEWORK_COMPILED_SHADERS_DIR="${FRAMEWORK_COMPILED_SHADERS_DIR}/" #!
  )
endif()

# -----------------------------------------------------------------------------

add_subdirectory(${FRAMEWORK_PATH})
add_subdirectory(${SAMPLES_PATH})

# -----------------------------------------------------------------------------
