cmake_minimum_required(VERSION 3.0)

set(ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set(HSL_MATH_INCL_DIRS)

# Source files that are needed for the static library
file(GLOB HSL_MATH_LIBRARY_SRC
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/*.h"
)

# Static library
add_library(HSLMath STATIC ${HSL_MATH_LIBRARY_SRC})

target_include_directories(HSLMath PUBLIC ${HSL_MATH_INCL_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(HSLMath PROPERTIES COMPILE_FLAGS "-DBUILDING_STATIC_LIBRARY -fPIC")
