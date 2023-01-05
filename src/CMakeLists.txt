cmake_minimum_required(VERSION 3.22)

project(aycwabtu)

set(common_sources 
    main.c
    loop.c
    helper.c
    bs_algo.c
    bs_block.c
    bs_block_ab.c
    bs_sse2.c
    bs_stream.c
    bs_uint32.c
    ts.c
    libdvbcsa/dvbcsa_algo.c
    libdvbcsa/dvbcsa_block.c
    libdvbcsa/dvbcsa_key.c
    libdvbcsa/dvbcsa_stream.c
)

set(common_includes
    libdvbcsa/dvbcsa
)

set(common_defines
    GITHASH="${GIT_HASH}"
)

execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_executable(aycwabtu)
target_sources(aycwabtu PRIVATE ${common_sources})
target_include_directories(aycwabtu PRIVATE ${common_includes})
target_compile_definitions(aycwabtu PRIVATE ${common_defines})


# configuration specific
set(PARALLEL_MODE ENABLE_P_32_INT CACHE STRING "SIMD parallel Mode")
set_property(CACHE PARALLEL_MODE PROPERTY STRINGS ENABLE_P_32_INT;ENABLE_P_128_SSE2)
target_compile_definitions(aycwabtu PRIVATE ${PARALLEL_MODE})
#if("${PARALLEL_MODE}" STREQUAL "")
#    message(SEND_ERROR "Please specify PARALLEL_MODE")
#endif()

# build type - optimization level
set(default_build_type "Debug")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()
