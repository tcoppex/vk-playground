# -----------------------------------------------------------------------------

function(add_simple_demo dirname)
  set(SAMPLE_PATH ${SAMPLES_PATH}/${dirname})

  file(GLOB Source
    ${SAMPLE_PATH}/*.cc
    ${SAMPLE_PATH}/*.h
  )

  compile_shaders(
    ${SAMPLE_PATH}/shaders/glsl
    ${SAMPLE_PATH}/shaders/spirv
    shaders_bin
    shaders_src
  )

  add_executable(${dirname} ${Source} ${shaders_bin} ${shaders_src})

  target_link_libraries(${dirname} PRIVATE
    ${FRAMEWORK_LIBRARIES}
    ${Vulkan_LIBRARY}
    # ${GLFW_LIBRARY}
    # volk
    # VulkanMemoryAllocator
  )

  target_include_directories(${dirname} PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${SAMPLE_PATH}
    ${Vulkan_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
  )

  target_compile_definitions(${dirname} PRIVATE
    COMPILED_SHADERS_DIR="${SAMPLE_PATH}/shaders/spirv"

    # Define VK_NO_PROTOTYPES to avoid including Vulkan prototypes
    # This is necessary because we are using volk to load Vulkan functions
    VK_NO_PROTOTYPES
  )
endfunction(add_simple_demo)

# -----------------------------------------------------------------------------

add_simple_demo(00_hello)
add_simple_demo(01_triangle)
# add_simple_demo(02_descriptor)

# -----------------------------------------------------------------------------
