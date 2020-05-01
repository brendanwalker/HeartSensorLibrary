cmake_minimum_required(VERSION 3.0)

# Dependencies
set(PSVR_SERVICE_INCL_DIRS)
set(PSVR_SERVICE_REQ_LIBS)

list(APPEND HSL_SERVICE_REQ_LIBS ${PLATFORM_LIBS})

# Source files for HSLService
file(GLOB HSL_CLIENT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/client/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/client/*.h"
)
source_group("Client" FILES ${HSL_CLIENT_SRC})

# Source files for HSLService
file(GLOB HSL_CONFIG_SRC
    "${CMAKE_CURRENT_LIST_DIR}/config/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/config/*.h"
)
source_group("Config" FILES ${HSL_CONFIG_SRC})

file(GLOB HSL_DEVICE_BLUETOOTHLE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/device/bluetoothle/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/device/bluetoothle/*.h"
)
source_group("Device\\BluetoothLE" FILES ${HSL_DEVICE_BLUETOOTHLE_SRC})

file(GLOB HSL_DEVICE_ENUM_SRC
    "${CMAKE_CURRENT_LIST_DIR}/device/enumerator/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/device/enumerator/*.h"
)
source_group("Device\\Enumerator" FILES ${HSL_DEVICE_ENUM_SRC})

file(GLOB HSL_DEVICE_INT_SRC
    "${CMAKE_CURRENT_LIST_DIR}/device/interface/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/device/interface/*.h"
)
source_group("Device\\Interface" FILES ${HSL_DEVICE_INT_SRC})

file(GLOB HSL_DEVICE_MGR_SRC
    "${CMAKE_CURRENT_LIST_DIR}/device/manager/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/device/manager/*.h"
)
source_group("Device\\Manager" FILES ${HSL_DEVICE_MGR_SRC})

file(GLOB HSL_DEVICE_SENSORS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/device/sensors/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/device/sensors/*.h"
)
source_group("Device\\Sensors" FILES ${HSL_DEVICE_SENSORS_SRC})

file(GLOB HSL_DEVICE_SENSORS_POLARH10_SRC
    "${CMAKE_CURRENT_LIST_DIR}/device/sensors/polar_h10/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/device/sensors/polar_h10/*.h"
)
source_group("Device\\Sensors\\PolarH10" FILES ${HSL_DEVICE_SENSORS_POLARH10_SRC})

file(GLOB HSL_DEVICE_VIEW_SRC
    "${CMAKE_CURRENT_LIST_DIR}/device/view/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/device/view/*.h"
)
source_group("Device\\View" FILES ${HSL_DEVICE_VIEW_SRC})

file(GLOB HSL_SERVICE_SRC
    "${CMAKE_CURRENT_LIST_DIR}/service/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/service/*.h"
)
source_group("Service" FILES ${HSL_SERVICE_SRC})

file(GLOB HSL_UTILS_SRC
    "${CMAKE_CURRENT_LIST_DIR}/Utils/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Utils/*.h"
)
source_group("Utils" FILES ${HSL_UTILS_SRC})

set(HSL_SERVICE_SRC
	${HSL_CLIENT_SRC}
	${HSL_CONFIG_SRC}
	${HSL_DEVICE_BLUETOOTHLE_SRC}
	${HSL_DEVICE_ENUM_SRC}
	${HSL_DEVICE_INT_SRC}
	${HSL_DEVICE_SENSORS_SRC}
	${HSL_DEVICE_SENSORS_POLARH10_SRC}
	${HSL_DEVICE_MGR_SRC}
	${HSL_DEVICE_VIEW_SRC}
	${HSL_SERVICE_SRC} 
	${HSL_UTILS_SRC}
)

list(APPEND HSL_SERVICE_INCL_DIRS
	${CMAKE_CURRENT_LIST_DIR}/client
	${CMAKE_CURRENT_LIST_DIR}/config
	${CMAKE_CURRENT_LIST_DIR}/device/bluetoothle
	${CMAKE_CURRENT_LIST_DIR}/device/enumerator
	${CMAKE_CURRENT_LIST_DIR}/device/interface
	${CMAKE_CURRENT_LIST_DIR}/device/manager
	${CMAKE_CURRENT_LIST_DIR}/device/sensors
	${CMAKE_CURRENT_LIST_DIR}/device/sensors/polar_h10
	${CMAKE_CURRENT_LIST_DIR}/device/view
	${CMAKE_CURRENT_LIST_DIR}/platform
	${CMAKE_CURRENT_LIST_DIR}/service
	${CMAKE_CURRENT_LIST_DIR}/utils
)

# Configuru
list(APPEND HSL_SERVICE_INCL_DIRS ${ROOT_DIR}/thirdparty/Configuru)

# Lockfree Queue
list(APPEND HSL_SERVICE_INCL_DIRS ${ROOT_DIR}/thirdparty/lockfreequeue)

# Platform Specific Device Management
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	# Windows utilities for querying driver infomation (provider name)
	list(APPEND HSL_SERVICE_INCL_DIRS ${ROOT_DIR}/src/hslservice/platform)	
	file(GLOB HSL_WIN32_PLATFORM_SRC
		"${CMAKE_CURRENT_LIST_DIR}/platform/*.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/platform/*.h"
	)	
	list(APPEND HSL_SERVICE_SRC ${HSL_WIN32_PLATFORM_SRC})	
	source_group("Device\\BluetoothLE" FILES ${HSL_WIN32_PLATFORM_SRC})
		
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
ELSE()
ENDIF()

# PSMoveMath
list(APPEND HSL_SERVICE_INCL_DIRS ${ROOT_DIR}/src/hslmath/)
list(APPEND HSL_SERVICE_REQ_LIBS HSLMath)

#
# HSLService Static library
#

# TODO: Build HSLClient as a STATIC or OBJECT w/ $<TARGET_OBJECTS:objlib>
add_library(HSLService_static STATIC ${HSL_SERVICE_SRC})
target_include_directories(HSLService_static PUBLIC ${HSL_SERVICE_INCL_DIRS})
target_link_libraries(HSLService_static PUBLIC ${HSL_SERVICE_REQ_LIBS})
target_compile_definitions(HSLService_static PRIVATE HSLService_STATIC) # See HSLClient_export.h
target_compile_definitions(HSLService_static PRIVATE HSLSERVICE_CPP_API) # See HSLClient_export.h

#
# HSLService Shared library
#

set(HSL_SERVICE_CAPI_REQ_LIBS)

# HSLService_static
list(APPEND HSL_SERVICE_CAPI_REQ_LIBS HSLService_static)
#Via HSLService_static, transitively inherits HSLMath

# Source files to develop the shared library.
list(APPEND HSL_SERVICE_CAPI_LIBRARY_SRC
	"${CMAKE_CURRENT_LIST_DIR}/client/HSLClient_export.h"
	"${CMAKE_CURRENT_LIST_DIR}/client/ClientConstants.h"
	"${CMAKE_CURRENT_LIST_DIR}/client/ClientMath_CAPI.h"
	"${CMAKE_CURRENT_LIST_DIR}/client/ClientMath_CAPI.cpp"	
	"${CMAKE_CURRENT_LIST_DIR}/client/HSLClient_CAPI.h"
	"${CMAKE_CURRENT_LIST_DIR}/client/HSLClient_CAPI.cpp"
)

# Shared library
add_library(HSLService SHARED ${HSL_SERVICE_CAPI_LIBRARY_SRC})
target_include_directories(HSLService PUBLIC ${CMAKE_CURRENT_LIST_DIR}/client)
target_link_libraries(HSLService PRIVATE ${HSL_SERVICE_CAPI_REQ_LIBS})
target_compile_definitions(HSLService_static PRIVATE HSLClient_EXPORTS) # See HSLClient_export.h
set_target_properties(HSLService PROPERTIES PUBLIC_HEADER "client/ClientConstants.h;client/ClientMath_CAPI.h;client/HSLClient_CAPI.h;client/HSLClient_export.h")
set_target_properties(HSLService PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(HSLService PROPERTIES C_VISIBILITY_PRESET hidden)	
		
# Install
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    install(TARGETS HSLService
        CONFIGURATIONS Debug
        RUNTIME DESTINATION ${HSL_DEBUG_INSTALL_PATH}/bin
        LIBRARY DESTINATION ${HSL_DEBUG_INSTALL_PATH}/lib
        ARCHIVE DESTINATION ${HSL_DEBUG_INSTALL_PATH}/lib
		PUBLIC_HEADER DESTINATION ${HSL_DEBUG_INSTALL_PATH}/include)
    install(TARGETS HSLService
        CONFIGURATIONS Release
        RUNTIME DESTINATION ${HSL_RELEASE_INSTALL_PATH}/bin
        LIBRARY DESTINATION ${HSL_RELEASE_INSTALL_PATH}/lib
        ARCHIVE DESTINATION ${HSL_RELEASE_INSTALL_PATH}/lib
		PUBLIC_HEADER DESTINATION ${HSL_RELEASE_INSTALL_PATH}/include)  
ELSE() #Linux/Darwin
ENDIF()
