/**
\file
*/ 

#ifndef __HSLCLIENT_CAPI_H
#define __HSLCLIENT_CAPI_H
#include "HSLClient_export.h"
#include "ClientConstants.h"
#include "ClientMath_CAPI.h"
#include <stdbool.h>
#include <stdint.h>

//cut_before

/** 
\brief Client Interface for HSLService
\defgroup HSLClient_CAPI Client Interface
\addtogroup HSLClient_CAPI 
@{ 
*/

// Macros
//-------
#define HSL_BITMASK_GET_FLAG(bitmask, flag) (((bitmask) & (1 << (flag))) > 0)
#define HSL_BITMASK_SET_FLAG(bitmask, flag) ((bitmask) |= (1 << (flag)))
#define HSL_BITMASK_CLEAR_FLAG(bitmask, flag) ((bitmask) &= (~(1) << (flag)))

// Defines a standard _PAUSE function
#if __cplusplus >= 199711L  // if C++11
#include <thread>
#define HSL_SLEEP(ms) (std::this_thread::sleep_for(std::chrono::milliseconds(ms)))
#elif defined(_WIN32)       // if windows system
#include <windows.h>
#define HSL_SLEEP(ms) (Sleep(ms))
#else                       // assume this is Unix system
#include <unistd.h>
#define HSL_SLEEP(ms) (usleep(1000 * ms))
#endif

// Shared Constants
//-----------------

typedef enum
{
	HSLContactStatus_Invalid		= 0,
	HSLContactStatus_NoContact		= 1,
	HSLContactStatus_Contact		= 2
} HSLContactSensorStatus;

/// Device data stream options
typedef enum
{
	// Sensor Stream Buffer Types
    HSLBufferType_HRData  = 0,		///< Heart Rate (measured in BPM)
    HSLBufferType_ECGData = 1,		///< Electrocardiography (Electrical signal of the heart)
    HSLBufferType_PPGData = 2,		///< Photoplethysmography Data (optical vessel measurement)
    HSLBufferType_PPIData = 3,		///< Pulse Internal (extracted from PPG)
    HSLBufferType_AccData = 4,		///< Accelerometer
	HSLBufferType_EDAData = 5,		///< Electrodermal Activity (in microSiemens) 
	// Filtered Data Buffer Types
	HSLBufferType_HRVData = 6,		///< Heart Rate Variability data 

    HSLBufferType_COUNT
} HSLSensorBufferType;

/// Device data stream options
typedef enum
{
	HSLCapability_HeartRate = 0,				///< Heart Rate (measured in BPM)
	HSLCapability_Electrocardiography = 1,		///< Electrocardiography (Electrical signal of the heart)
	HSLCapability_Photoplethysmography = 2,		///< Photoplethysmography Data (optical vessel measurement)
	HSLCapability_PulseInterval = 3,			///< Pulse Internal (extracted from PPG)
	HSLCapability_Accelerometer = 4,			///< Accelerometer
	HSLCapability_ElectrodermalActivity = 5,	///< Electrodermal Activity (in microSiemens) 

	HSLCapability_COUNT
} HSLSensorCapabilityType;

typedef unsigned int t_hsl_caps_bitmask;

typedef enum
{
	HRVFilter_SDANN		= 0,	///< The standard deviation of the average NN intervals calculated over short periods, usually 5 minutes.
	HRVFilter_RMSSD		= 1,	///< The square root of the mean of the squares of the successive differences between adjacent NNs.[36]
	HRVFilter_SDSD		= 2,	///< The standard deviation of the successive differences between adjacent NNs.
	HRVFilter_NN50		= 3,	///< The number of pairs of successive NNs that differ by more than 50 ms.
	HRVFilter_pNN50		= 4,	///< The proportion of NN50 divided by total number of NNs.
	HRVFilter_NN20		= 5,	///< The number of pairs of successive NNs that differ by more than 20 ms.
	HRVFilter_pNN20		= 6,	///< The proportion of NN20 divided by total number of NNs.

	HRVFilter_COUNT
} HSLHeartRateVariabityFilterType;

typedef unsigned int t_hrv_filter_bitmask;

/// Tracking Debug flags
typedef enum
{
	HSLTrackerDebugFlags_none = 0x00,						///< Turn off all
} HSLTrackerDebugFlags;

// Sensor State
//------------------

/// https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.heart_rate_measurement.xml
/// Heart rate data from the "Heart Rate Measurement" GATT characteristic
typedef struct
{
	HSLContactSensorStatus	contactStatus;
	uint16_t				beatsPerMinute;
	uint16_t				energyExpended; // kilo-Joules
	uint16_t				RRIntervals[9]; // ms (max 9 samples possible in a frame)
	uint16_t				RRIntervalCount;
	double					timeInSeconds;
} HSLHeartRateFrame;

typedef struct
{
	uint32_t				ecgValues[10];		// microvolts
	uint16_t				ecgValueCount;
	double					timeInSeconds;
	double					timeDeltaInSeconds;
} HSLHeartECGFrame;

// https://www.polar.com/blog/optical-heart-rate-tracking-polar/
typedef struct
{
    int32_t				ppgValue0;
    int32_t				ppgValue1;
    int32_t				ppgValue2;
    int32_t				ambient;
} HSLHeartPPGSample;

typedef struct
{
	HSLHeartPPGSample		ppgSamples[10];
	uint16_t				ppgSampleCount;
	double					timeInSeconds;
	double					timeDeltaInSeconds;
} HSLHeartPPGFrame;

typedef struct
{
	uint8_t					beatsPerMinute;
	uint16_t				pulseDuration;			// milliseconds
	uint16_t				pulseDurationErrorEst;	// milliseconds
	uint8_t					blockerBit : 1;
	uint8_t					skinContactBit : 1;
	uint8_t					supportsSkinContactBit : 1;
} HSLHeartPPISample;

typedef struct
{
	HSLHeartPPISample		ppiSamples[10];
	uint16_t				ppiSampleCount;
	double					timeInSeconds;
} HSLHeartPPIFrame;

/// Normalized Accelerometer Data
typedef struct
{
	HSLVector3f				accSamples[5];	// g-units
	uint16_t				accSampleCount;
	double					timeInSeconds;
	double					timeDeltaInSeconds;
} HSLAccelerometerFrame;

/// Derived Heart Rate
typedef struct
{
	float					hrvValue;
	double					timeInSeconds;
} HSLHeartVariabilityFrame;

/// Skin Electrodermal Activity conductance/resistance measurement
typedef struct
{
	uint16_t				adcValue; // (0-1023) value from sensor analog to digital converter
	double					resistanceOhms; // adcValue converted to resistance value in ohms
	double					conductanceMicroSiemens; // resistance converted to conductance in microSiemens
	double					timeInSeconds;
} HSLElectrodermalActivityFrame;

/// Device strings 
typedef struct
{
	// OS Device Information
	char					deviceFriendlyName[256];
	char					devicePath[256];

	// DeviceInformation GATT Service
	char					systemID[32];
    char					modelNumberString[32];
    char					serialNumberString[32];
    char					firmwareRevisionString[32];
    char					hardwareRevisionString[32];
    char					softwareRevisionString[32];
    char					manufacturerNameString[64];

	// HeartRate GATT Service
	char					bodyLocation[32];

	// HSL Device Information
	HSLSensorID				sensorID;
	t_hsl_caps_bitmask		capabilities;
} HSLDeviceInformation;

/// Sensor Pool Entry
typedef struct
{
	// Static Data
	HSLSensorID				sensorID;
	HSLDeviceInformation	deviceInformation;

	// Dynamic Data
	bool					isConnected;
	uint16_t				beatsPerMinute;
	t_hsl_caps_bitmask		activeSensorStreams;
	t_hrv_filter_bitmask	activeFilterStreams;
} HSLSensor;

typedef struct 
{
	HSLSensorBufferType bufferType;
	void *buffer;
	size_t bufferCapacity;
	size_t stride;
	size_t currentIndex;
	size_t endIndex;
	size_t remaining;
} HSLBufferIterator;

// Service Events
//------------------

typedef enum 
{
	HSLEvent_SensorListUpdated,
} HSLEventType;

/// A container for all HSLService events
typedef struct
{
	HSLEventType event_type;
} HSLEventMessage;

// Service Responses
//------------------

/// Current version of HSLService
typedef struct
{
	char version_string[HSLSERVICE_MAX_VERSION_STRING_LEN];
} HSLServiceVersion;

/// Static sensor state in the sensor list entry
typedef struct
{
	// Device ID remains constant while the device is open
	HSLSensorID				sensorID;
	// Static device information for the device
	HSLDeviceInformation	deviceInformation;
} HSLSensorListEntry;

/// List of Sensors attached to HSLService
typedef struct
{
	char hostSerial[HSLSERVICE_SENSOR_SERIAL_LEN];
	HSLSensorListEntry sensors[HSLSERVICE_MAX_SENSOR_COUNT];
	int count;
} HSLSensorList;

// Interface
//----------

/** \brief Initializes a connection to HSLService.
 Attempts to initialize HSLervice. 
 This function must be called before calling any other client functions. 
 Calling this function again after a connection is already started will return true.

 \param log_level The level of logging to emit
 \returns true on success, false on a general init error.
 */
HSL_PUBLIC_FUNCTION(bool) HSL_Initialize(HSLLogSeverityLevel log_level); 

/** \brief Shuts down connection to HSLService
 Shuts down HSLService. 
 This function should be called when closing down the client.
 Calling this function again after a connection is already closed will return false.

	\returns true on success or false if there was no valid connection.
 */
HSL_PUBLIC_FUNCTION(bool) HSL_Shutdown();

// Update
/** \brief Poll the connection and process messages.
	This function will poll the connection for new messages from HSLService.
	If new events are received they are processed right away and the the appropriate status flag will be set.
	The following state polling functions can be called after an update:
		- \ref HSL_GetIsInitialized()
		- \ref HSL_HasSensorListChanged()
		
	\return true if initialize or false otherwise
 */
HSL_PUBLIC_FUNCTION(bool) HSL_Update();

/** \brief Poll the connection and DO NOT process messages.
	This function will poll the connection for new messages from HSLService.
	If new events are received they are put in a queue. The messages are extracted using \ref HSL_PollNextMessage().
	Messages not read from the queue will get cleared on the next update call. 
	
	\return true if there is an active connection or false if there is no valid connection
 */
HSL_PUBLIC_FUNCTION(bool) HSL_UpdateNoPollEvents();

// System State Queries
/** \brief Get the API initialization status
	\return true if the client API is initialized
 */
HSL_PUBLIC_FUNCTION(bool) HSL_GetIsInitialized();

/** \brief Get the Sensor list change flag
	This flag is only filled in when \ref HSL_Update() is called.
	If you instead call HSL_UpdateNoPollMessages() you'll need to process the event queue yourself to get Sensor
	list change events.
	
	\return true if the Sensor list changed
 */
HSL_PUBLIC_FUNCTION(bool) HSL_HasSensorListChanged();

// System Queries
/** \brief Get the client API version string from HSLService
	\param[out] out_version_string The string buffer to write the version into
	\param max_version_string The size of the output buffer
	\return true upon receiving result or false on request error.
 */
HSL_PUBLIC_FUNCTION(bool) HSL_GetVersionString(char *out_version_string, size_t max_version_string);

// Message Handling API
/** \brief Retrieve the next message from the message queue.
	A call to \ref HSL_UpdateNoPollMessages will queue messages received from HSLService.
	Use this function to processes the queued event and response messages one by one.
	If a response message does not have a callback registered with \ref HSL_RegisterCallback it will get returned here.	
	\param[out] out_messaage The next \ref HSLEventMessage read from the incoming message queue.
	\param message_size The size of the message structure. Pass in sizeof(HSLEventMessage).
	\return true or bool_NoData if no more messages are available.
 */
HSL_PUBLIC_FUNCTION(bool) HSL_PollNextMessage(HSLEventMessage *out_message);

// Sensor Pool
/** \brief Fetches the \ref HSLSensor data for the given Sensor
	The client API maintains a pool of Sensor structs. 
	We can fetch a given Sensor by \ref HSLSensorID.
	DO NOT DELETE the Sensor pointer returned by this function.
	It is safe to copy this pointer on to other structures so long as the pointer is cleared once the client API is shutdown.
	\param sensor_ The id of the Sensor structure to fetch
	\return A pointer to a \ref PSMSensor
 */
HSL_PUBLIC_FUNCTION(HSLSensor *) HSL_GetSensor(HSLSensorID sensor_id);

HSL_PUBLIC_FUNCTION(HSLBufferIterator) HSL_GetCapabilityBuffer(HSLSensorID sensor_id, HSLSensorCapabilityType cap_type);
HSL_PUBLIC_FUNCTION(HSLBufferIterator) HSL_GetHeartHrvBuffer(HSLSensorID sensor_id, HSLHeartRateVariabityFilterType filter);

HSL_PUBLIC_FUNCTION(bool) HSL_FlushCapabilityBuffer(HSLSensorID sensor_id, HSLSensorCapabilityType cap_type);
HSL_PUBLIC_FUNCTION(bool) HSL_FlushHeartHrvBuffer(HSLSensorID sensor_id, HSLHeartRateVariabityFilterType filter);

HSL_PUBLIC_FUNCTION(bool) HSL_IsBufferIteratorValid(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(void) HSL_BufferIteratorReset(HSLBufferIterator* iterator);
HSL_PUBLIC_FUNCTION(bool) HSL_BufferIteratorNext(HSLBufferIterator *iterator);

HSL_PUBLIC_FUNCTION(void *) HSL_BufferIteratorGetValueRaw(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(HSLHeartRateFrame *) HSL_BufferIteratorGetHRData(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(HSLHeartECGFrame *) HSL_BufferIteratorGetECGData(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(HSLHeartPPGFrame *) HSL_BufferIteratorGetPPGData(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(HSLHeartPPIFrame *) HSL_BufferIteratorGetPPIData(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(HSLAccelerometerFrame *) HSL_BufferIteratorGetAccData(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(HSLElectrodermalActivityFrame*) HSL_BufferIteratorGetEDAData(HSLBufferIterator* iterator);
HSL_PUBLIC_FUNCTION(HSLHeartVariabilityFrame *) HSL_BufferIteratorGetHRVData(HSLBufferIterator *iterator);

HSL_PUBLIC_FUNCTION(bool) HSL_GetCapabilitySamplingRate(HSLSensorID sensor_id, HSLSensorCapabilityType cap_type, int* out_sampling_rate);
HSL_PUBLIC_FUNCTION(bool) HSL_GetCapabilityBitResolution(HSLSensorID sensor_id, HSLSensorCapabilityType cap_type, int* out_resolution);

// Sensor Requests
/** \brief Requests a list of the streamable Sensors currently connected to HSLService.
	Sends a request to HSLService to get the list of currently streamable Sensors.
	\param[out] out_Sensor_list The Sensor list to write the result into.
	\return true upon receiving result or false on request error.
 */
HSL_PUBLIC_FUNCTION(bool) HSL_GetSensorList(HSLSensorList *out_sensor_list);

/** \brief Requests start/stop of a capability streams for a given Sensor
	Asks HSLService to start or stop stream data for the given Sensor with the given set of stream properties.
	The data in the associated \ref HSLSensor state will get updated automatically in calls to \ref HSL_Update or 
	\ref HSL_UpdateNoPollMessages.
	Requests to restart an already started stream will be ignored.
	\param sensor_id The id of the Sensor to start the stream for.
	\param data_stream_bitmask One or more of the flags from \ref HSLSensorDataStreamFlags
	\return true upon receiving result or false on request error.
 */
HSL_PUBLIC_FUNCTION(bool) HSL_SetActiveSensorCapabilityStreams(
	HSLSensorID sensor_id, 
	t_hsl_caps_bitmask data_stream_bitmask);

/** \brief Requests start/stop of a data streams for a given Sensor
	Asks HSLService to start or stop stream data for the given Sensor with the given set of stream properties.
	The data in the associated \ref HSLSensor state will get updated automatically in calls to \ref HSL_Update or 
	\ref HSL_UpdateNoPollMessages.
	Requests to restart an already started stream will be ignored.
	\param sensor_id The id of the Sensor to start the stream for.
	\param filter_stream_bitmask One or more of the flags from \ref HSLHeartRateVariabityFilterType
	\return true upon receiving result or false on request error.
 */
HSL_PUBLIC_FUNCTION(bool) HSL_SetActiveSensorFilterStreams(
	HSLSensorID sensor_id, 
	t_hrv_filter_bitmask filter_stream_bitmask);

/** \brief Requests stop of all data and filter streams for a given Sensor
	Asks HSLService to stop all stream data for the given sensor_id.
	The data in the associated \ref HSLSensor state will get updated automatically in calls to \ref HSL_Update or 
	\ref HSL_UpdateNoPollMessages.
	\param sensor_id The id of the Sensor to stop the streams for.
	\return true upon receiving result or false on request error. */
HSL_PUBLIC_FUNCTION(bool) HSL_StopAllSensorStreams(HSLSensorID sensor_id);

/** 
@} 
*/ 

//cut_after
#endif
