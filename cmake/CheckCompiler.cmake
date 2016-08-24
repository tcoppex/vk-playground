

# Find a GCC version that can match the minimum required
# if none were found, check on CMAKE_FIND_ROOT_PATH
function(find_gcc MIN_GCC_REQUIRED)
  # Macro : check GCC version is greater than 4.8
  macro(check_gcc_version)
    set(OK_GCC FALSE)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
    if (GCC_VERSION VERSION_GREATER ${MIN_GCC_REQUIRED} OR
        GCC_VERSION VERSION_EQUAL ${MIN_GCC_REQUIRED})
      set(OK_GCC TRUE)
    endif()
  endmacro()

  check_gcc_version()

  # If no valid gcc found, check in CMAKE_FIND_ROOT_PATH
  if(NOT ${OK_GCC})
    if("${CMAKE_FIND_ROOT_PATH}" STREQUAL "")
      # default installation path in centos-6
      set(CMAKE_FIND_ROOT_PATH  /opt/rh/devtoolset-2/root/)
    endif()

    set(CMAKE_C_COMPILER   "${CMAKE_FIND_ROOT_PATH}/usr/bin/gcc")
    set(CMAKE_CXX_COMPILER "${CMAKE_FIND_ROOT_PATH}/usr/bin/g++")
  endif()
  
  check_gcc_version()

  # Still no chance, abort execution
  if(NOT ${OK_GCC})
    message(FATAL_ERROR "GCC ${MIN_GCC_REQUIRED}+ is required, please set CMAKE_FIND_ROOT_PATH.")
  endif()
endfunction(find_gcc)