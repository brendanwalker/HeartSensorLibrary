#ifndef CLIENT_CONSTANTS_H
#define CLIENT_CONSTANTS_H

//-- includes -----
/** 
\addtogroup HSLClient_CAPI 
@{ 
*/

//-- constants -----
typedef enum
{
    HSLLogSeverityLevel_trace,
    HSLLogSeverityLevel_debug,
    HSLLogSeverityLevel_info,
    HSLLogSeverityLevel_warning,
    HSLLogSeverityLevel_error,
    HSLLogSeverityLevel_fatal
} HSLLogSeverityLevel;

// See SensorManager.h in PSMoveService
#define HSLSERVICE_MAX_SENSOR_COUNT  5

// The max length of the service version string
#define HSLSERVICE_MAX_VERSION_STRING_LEN 32

// The length of a Sensor serial string: "xx:xx:xx:xx:xx:xx\0"
#define HSLSERVICE_SENSOR_SERIAL_LEN  18

// Defines a standard _PAUSE function
#if __cplusplus >= 199711L  // if C++11
    #include <thread>
    #define _PAUSE(ms) (std::this_thread::sleep_for(std::chrono::milliseconds(ms)))
#elif defined(_WIN32)       // if windows system
    #include <windows.h>
    #define _PAUSE(ms) (Sleep(ms))
#else                       // assume this is Unix system
    #include <unistd.h>
    #define _PAUSE(ms) (usleep(1000 * ms))
#endif

// Wrapper Types
//--------------

/// The ID of a Sensor in the Sensor pool
typedef int HSLSensorID;

/** 
@} 
*/ 

#endif // CLIENT_CONSTANTS_H
