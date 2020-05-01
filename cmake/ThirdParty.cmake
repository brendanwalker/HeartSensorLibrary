# When not using MSVC, we recommend using system-wide libraries
# (installed via homebrew on Mac or apt-get in Linux/Ubuntu)
# In MSVC, we auto-download the source and make it an external_project

# Platform specific libraries
SET(PLATFORM_LIBS)
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    list(APPEND PLATFORM_LIBS bthprops mfplat setupapi BluetoothApis)
ENDIF()
