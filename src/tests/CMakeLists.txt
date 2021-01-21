#
# TEST_CONSOLE_CAPI
#
add_executable(test_console_CAPI test_console_CAPI.cpp)
target_include_directories(test_console_CAPI PUBLIC ${ROOT_DIR}/src/hslservice/client/)
target_link_libraries(test_console_CAPI HSLService)
SET_TARGET_PROPERTIES(test_console_CAPI PROPERTIES FOLDER Test)

# Install
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
install(TARGETS test_console_CAPI
    RUNTIME DESTINATION ${HSL_ROOT_INSTALL_PATH}/bin/${ARCH_LABEL}
    LIBRARY DESTINATION ${HSL_ROOT_INSTALL_PATH}/lib/${ARCH_LABEL}
    ARCHIVE DESTINATION ${HSL_ROOT_INSTALL_PATH}/lib/${ARCH_LABEL}
    PUBLIC_HEADER DESTINATION ${HSL_ROOT_INSTALL_PATH}/include)
ELSE() #Linux/Darwin
ENDIF()