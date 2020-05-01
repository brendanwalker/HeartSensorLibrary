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
    CONFIGURATIONS Debug
    RUNTIME DESTINATION ${PSM_DEBUG_INSTALL_PATH}/bin
    LIBRARY DESTINATION ${PSM_DEBUG_INSTALL_PATH}/lib
    ARCHIVE DESTINATION ${PSM_DEBUG_INSTALL_PATH}/lib)
install(TARGETS test_console_CAPI
    CONFIGURATIONS Release
    RUNTIME DESTINATION ${PSM_RELEASE_INSTALL_PATH}/bin
    LIBRARY DESTINATION ${PSM_RELEASE_INSTALL_PATH}/lib
    ARCHIVE DESTINATION ${PSM_RELEASE_INSTALL_PATH}/lib)    
ELSE() #Linux/Darwin
ENDIF()