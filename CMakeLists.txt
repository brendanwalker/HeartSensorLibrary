cmake_minimum_required(VERSION 3.2)
project(HeartSensorLib VERSION 0.0.1)

set(ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(cmake/Environment.cmake)
include(cmake/Version.cmake)

set(HSL_PROJECT_NAME "HeartSensorLib_${HSL_VERSION_STRING}")
set(HSL_ROOT_INSTALL_PATH "${ROOT_DIR}/dist")
file(MAKE_DIRECTORY "${HSL_ROOT_INSTALL_PATH}")
file(WRITE "${HSL_ROOT_INSTALL_PATH}/version.txt" "${HSL_PROJECT_NAME}")

include(cmake/ThirdParty.cmake)

# Step into the subdirectories
add_subdirectory(src)

