find_program(GLSL_LANG_VALIDATOR glslangValidator)

if (WIN32)
  if (CMAKE_CL_64)
    find_program(GLSL_LANG_VALIDATOR glslangValidator
      "$ENV{VULKAN_SDK}/Bin"
      "$ENV{VK_SDK_PATH}/Bin")
  else()
    find_program(GLSL_LANG_VALIDATOR glslangValidator
      "$ENV{VULKAN_SDK}/Bin32"
      "$ENV{VK_SDK_PATH}/Bin32")
  endif()
else()
    find_program(GLSL_LANG_VALIDATOR glslangValidator
      "$ENV{VULKAN_SDK}/bin")
endif()


# Macro
# convert a GLSL shader to its SPIRV version
macro(convert_glsl_to_spirv input_glsl output_spirv)  
  add_custom_command(
    OUTPUT
      ${output_spirv}
    COMMAND
      ${GLSL_LANG_VALIDATOR} -s -V -o ${output_spirv} ${input_glsl}
    DEPENDS
      ${input_glsl}
      ${GLSL_LANG_VALIDATOR}
    WORKING_DIRECTORY
      ${CMAKE_SOURCE_DIR}
    COMMENT
      "Converting shader ${input_glsl} to ${output_spirv}" VERBATIM
    SOURCES
      ${input_glsl}
  )
endmacro(convert_glsl_to_spirv)