
###############################################################################
function(add_simple_demo dir)
  set(GLOBAL_DIR ${CMAKE_CURRENT_LIST_DIR}/${dir})
  file(GLOB Sources ${GLOBAL_DIR}/*.cc ${GLOBAL_DIR}/*.h)
  
  # weirdly, if shaders are not compiled in the same file it's impossible to 
  # depends the targets on them..
  compile_shaders(${GLOBAL_DIR}/glsl ${GLOBAL_DIR}/spirv shaders_bin shaders_src)

  add_executable(${dir} ${EngineSources} ${Sources} ${shaders_bin} ${shaders_src})
  add_definitions(-DCOMPILED_SHADERS_DIR="${GLOBAL_DIR}/spirv/")

  target_link_libraries(${dir}
    ${VULKAN_LIBRARY}
    ${GLFW_LIBRARY}
  )
  target_compile_definitions(${dir} PRIVATE ${VALIDATION_LAYERS})
endfunction(add_simple_demo)
###############################################################################


# -------------------------------------------------------
# Engine source
# -------------------------------------------------------

file(GLOB_RECURSE EngineSources 
  engine/*.cc 
  engine/*.h
)

# -------------------------------------------------------
# Demos
# -------------------------------------------------------

add_simple_demo(01-cube)
