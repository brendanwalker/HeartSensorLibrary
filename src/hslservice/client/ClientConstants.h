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

// Wrapper Types
//--------------

/// The ID of a Sensor in the Sensor pool
typedef int HSLSensorID;

/** 
@} 
*/ 

#endif // CLIENT_CONSTANTS_H
