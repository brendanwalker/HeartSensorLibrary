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

// Shared Constants
//-----------------

/// Result enum in response to a client API request
typedef enum
{
	HSLResult_Canceled		= -3,	///< Request Was Canceled
	HSLResult_NoData		= -2,	///< Request Returned No Data
	HSLResult_Error			= -1,	///< General Error Result
	HSLResult_Success		= 0,	///< General Success Result
} HSLResult;

typedef enum
{
	HSLContactStatus_Invalid		= 0,
	HSLContactStatus_NoContact		= 1,
	HSLContactStatus_Contact		= 2
} HSLContactSensorStatus;

/// Device data stream options
typedef enum
{
    HSLBufferType_HRData  = 0,		///< Heart Rate (measured in BPM)
    HSLBufferType_ECGData = 1,		///< Electrocardiography (Electrical signal of the heart)
    HSLBufferType_PPGData = 2,		///< Photoplethysmography Data (optical vessel measurement)
    HSLBufferType_PPIData = 3,		///< Pulse Internal (extracted from PPG)
    HSLBufferType_AccData = 4,		///< Accelerometer
	HSLBufferType_HRVData = 5,		///< Heart Rate Variablility data 

    HSLBufferType_COUNT
} HSLSensorBufferType;

/// Device data stream options
typedef enum
{
	HSLStreamFlags_HRData = 0,		///< Heart Rate (measured in BPM)
	HSLStreamFlags_ECGData = 1,		///< Electrocardiography (Electrical signal of the heart)
	HSLStreamFlags_PPGData = 2,		///< Photoplethysmography Data (optical vessel measurement)
	HSLStreamFlags_PPIData = 3,		///< Pulse Internal (extracted from PPG)
	HSLStreamFlags_AccData = 4,		///< Accelerometer

	HSLStreamFlags_COUNT
} HSLSensorDataStreamFlags;

typedef unsigned int t_hsl_stream_bitmask;

typedef enum
{
	HRVFilter_SDNN		= 0,	///< the standard deviation of NN intervals.Often calculated over a 24 - hour period.SDANN, the standard deviation of the average NN intervals calculated over short periods, usually 5 minutes.SDNN is therefore a measure of changes in heart rate due to cycles longer than 5 minutes.SDNN reflects all the cyclic components responsible for variability in the period of recording, therefore it represents total variability.
	HRVFilter_RMSSD		= 1,	///< the square root of the mean of the squares of the successive differences between adjacent NNs.[36]
	HRVFilter_SDSD		= 2,	///< the standard deviation of the successive differences between adjacent NNs.
	HRVFilter_NN50		= 3,	///< the number of pairs of successive NNs that differ by more than 50 ms.
	HRVFilter_pNN50		= 4,	///< the proportion of NN50 divided by total number of NNs.
	HRVFilter_NN20		= 5,	///< the number of pairs of successive NNs that differ by more than 20 ms.
	HRVFilter_pNN20		= 6,	///< the proportion of NN20 divided by total number of NNs.

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
	uint16_t				ecgValues[10];		// microvolts
	uint16_t				ecgValueCount;
	double					timeInSeconds;
} HSLHeartECGFrame;

// https://www.polar.com/blog/optical-heart-rate-tracking-polar/
typedef struct
{
    uint16_t				ppgValue0;
    uint16_t				ppgValue1;
    uint16_t				ppgValue2;
    uint16_t				ambient;
} HSLHeartPPGSample;

typedef struct
{
	HSLHeartPPGSample		ppgSamples[10];
	uint16_t				ppgSampleCount;
	double					timeInSeconds;
} HSLHeartPPGFrame;

typedef struct
{
	HSLContactSensorStatus	contactStatus;
	int						beatsPerMinute;
	float					pulseDuration;			// milliseconds
	float					pulseDurationErrorEst;	// milliseconds
	double					timeInSeconds;
} HSLHeartPPIFrame;

/// Normalized Accelerometer Data
typedef struct
{
	HSLVector3f				accSamples[5];	// g-units
	uint16_t				accSampleCount;
	double					timeInSeconds;
} HSLAccelerometerFrame;

/// Derived Heart Rate
typedef struct
{
	float					hrvValue;
	double					timeInSeconds;
} HSLHeartVariabilityFrame;

/// Device strings 
typedef struct
{
    char systemID[32];
    char modelNumberString[32];
    char serialNumberString[32];
    char firmwareRevisionString[32];
    char hardwareRevisionString[32];
    char softwareRevisionString[32];
    char manufacturerNameString[64];
} HSLDeviceInformation;

/// Sensor Pool Entry
typedef struct
{
	// Static Data
	HSLSensorID				sensorID;
	t_hsl_stream_bitmask	capabilities;
	char					deviceFriendlyName[256];
	char					devicePath[256];
	HSLDeviceInformation	deviceInformation;

	// Dynamic Data
	t_hsl_stream_bitmask	active_streams;
	t_hrv_filter_bitmask	active_filters;
	int						outputSequenceNum;
	int						inputSequenceNum;
	long long				dataFrameLastReceivedTime;
	float					dataFrameAverageFPS;
	int						listenerCount;
	bool					isValid;
	bool					isConnected;
} HSLSensor;

typedef struct 
{
	// Buffer Properties
	HSLSensorBufferType bufferType;
	void *buffer;
	size_t bufferSize;
	size_t stride;
	int startIndex;
	int endIndex;

	// Buffer Index
	int currentIndex;
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

/// List of Sensors attached to PSMoveService
typedef struct
{
	char host_serial[HSLSERVICE_SENSOR_SERIAL_LEN];
	HSLSensor Sensors[HSLSERVICE_MAX_SENSOR_COUNT];
	int count;
} HSLSensorList;

// Interface
//----------

/** \brief Initializes a connection to HSLService.
 Attempts to initialize HSLervice. 
 This function must be called before calling any other client functions. 
 Calling this function again after a connection is already started will return HSLResult_Success.

 \param log_level The level of logging to emit
 \returns HSLResult_Success on success, HSLResult_Timeout, or HSLResult_Error on a general connection error.
 */
HSL_PUBLIC_FUNCTION(HSLResult) HSL_Initialize(HSLLogSeverityLevel log_level); 

/** \brief Shuts down connection to HSLService
 Shuts down HSLService. 
 This function should be called when closing down the client.
 Calling this function again after a connection is alread closed will return HSLResult_Error.

	\returns HSLResult_Success on success or HSLResult_Error if there was no valid connection.
 */
HSL_PUBLIC_FUNCTION(HSLResult) HSL_Shutdown();

// Update
/** \brief Poll the connection and process messages.
	This function will poll the connection for new messages from HSLService.
	If new events are received they are processed right away and the the appropriate status flag will be set.
	The following state polling functions can be called after an update:
		- \ref HSL_GetIsInitialized()
		- \ref HSL_HasSensorListChanged()
		
	\return HSLResult_Success if initialize or HSLResult_Error otherwise
 */
HSL_PUBLIC_FUNCTION(HSLResult) HSL_Update();

/** \brief Poll the connection and DO NOT process messages.
	This function will poll the connection for new messages from HSLService.
	If new events are received they are put in a queue. The messages are extracted using \ref HSL_PollNextMessage().
	Messages not read from the queue will get cleared on the next update call. 
	
	\return HSLResult_Success if there is an active connection or HSLResult_Error if there is no valid connection
 */
HSL_PUBLIC_FUNCTION(HSLResult) HSL_UpdateNoPollEvents();

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
	\return HSLResult_Success upon receiving result, HSLResult_Timeoout, or HSLResult_Error on request error.
 */
HSL_PUBLIC_FUNCTION(HSLResult) HSL_GetVersionString(char *out_version_string, size_t max_version_string);

// Message Handling API
/** \brief Retrieve the next message from the message queue.
	A call to \ref HSL_UpdateNoPollMessages will queue messages received from HSLService.
	Use this function to processes the queued event and response messages one by one.
	If a response message does not have a callback registered with \ref HSL_RegisterCallback it will get returned here.	
	\param[out] out_messaage The next \ref HSLEventMessage read from the incoming message queue.
	\param message_size The size of the message structure. Pass in sizeof(HSLEventMessage).
	\return HSLResult_Success or HSLResult_NoData if no more messages are available.
 */
HSL_PUBLIC_FUNCTION(HSLResult) HSL_PollNextMessage(HSLEventMessage *out_message, size_t message_size);

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

HSL_PUBLIC_FUNCTION(HSLResult) HSL_GetHeartRateBuffer(
	HSLSensorID sensor_id, HSLBufferIterator *out_iterator);

HSL_PUBLIC_FUNCTION(HSLResult) HSL_GetHeartECGBuffer(
	HSLSensorID sensor_id, HSLBufferIterator *out_iterator);

HSL_PUBLIC_FUNCTION(HSLResult) HSL_GetHeartPPGBuffer(
	HSLSensorID sensor_id, HSLBufferIterator *out_iterator);

HSL_PUBLIC_FUNCTION(HSLResult) HSL_GetHeartPPIBuffer(
	HSLSensorID sensor_id, HSLBufferIterator *out_iterator);

HSL_PUBLIC_FUNCTION(HSLResult) HSL_GetHeartAccBuffer(
	HSLSensorID sensor_id, HSLBufferIterator *out_iterator);

HSL_PUBLIC_FUNCTION(HSLResult) HSL_GetHeartHrvBuffer(
	HSLSensorID sensor_id, HSLHeartRateVariabityFilterType filter, HSLBufferIterator *out_iterator);

HSL_PUBLIC_FUNCTION(bool) HSL_IsBufferIteratorValid(HSLBufferIterator *iterator);

HSL_PUBLIC_FUNCTION(bool) HSL_BufferIteratorNext(HSLBufferIterator *iterator);

HSL_PUBLIC_FUNCTION(HSLHeartRateFrame *) HSL_BufferIteratorGetHRData(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(HSLHeartECGFrame *) HSL_BufferIteratorGetECGData(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(HSLHeartPPGFrame *) HSL_BufferIteratorGetPPGData(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(HSLHeartPPIFrame *) HSL_BufferIteratorGetPPIData(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(HSLAccelerometerFrame *) HSL_BufferIteratorGetAccData(HSLBufferIterator *iterator);
HSL_PUBLIC_FUNCTION(HSLHeartVariabilityFrame *) HSL_BufferIteratorGetHRVData(HSLBufferIterator *iterator);

/** \brief Allocate a reference to a Sensor.
	This function tells the client API to increment a reference count for a given Sensor.
	This function should be called before fetching the Sensor data using \ref HSL_GetSensor.
	When done with the Sensor, make sure to call \ref HSL_FreeSensorListener.
	\param sensor_ The id of the Sensor we want to allocate a listener for
	\return PSMResult_Success if a valid Sensor id is given
 */
HSL_PUBLIC_FUNCTION(HSLResult) HSL_AllocateSensorListener(HSLSensorID sensor_id);

/** \brief Free a reference to a Sensor
	This function tells the client API to decrement a reference count for a given Sensor.
	\param sensor_ The of of the Sensor we want to free the listener for.
	\return PSMResult_Success if a valid Sensor id is given that has a non-zero ref count
 */
HSL_PUBLIC_FUNCTION(HSLResult) HSL_FreeSensorListener(HSLSensorID sensor_id);

// Sensor Requests
/** \brief Requests a list of the streamable Sensors currently connected to HSLService.
	Sends a request to HSLService to get the list of currently streamable Sensors.
	\param[out] out_Sensor_list The Sensor list to write the result into.
	\return HSLResult_Success upon receiving result, HSLResult_Timeoout, or HSLResult_Error on request error.
 */
HSL_PUBLIC_FUNCTION(HSLResult) HSL_GetSensorList(HSLSensorList *out_sensor_list);

/** \brief Requests start/stop of a data streams for a given Sensor
	Asks HSLService to start or stop stream data for the given Sensor with the given set of stream properties.
	The data in the associated \ref HSLSensor state will get updated automatically in calls to \ref HSL_Update or 
	\ref HSL_UpdateNoPollMessages.
	Requests to restart an already started stream will be ignored.
	\param sensor_ The id of the Sensor to start the stream for.
	\param data_stream_bitmask One or more of the flags from \ref HSLSensorDataStreamFlags
	\param filter_stream_bitmask One or more of the flags from \ref HSLHeartRateVariabityFilterType
	\return HSLResult_Success upon receiving result, HSLResult_Timeoout, or HSLResult_Error on request error.
 */
HSL_PUBLIC_FUNCTION(HSLResult) HSL_SetActiveSensorDataStreams(
	HSLSensorID sensor_id, 
	t_hsl_stream_bitmask data_stream_bitmask,
	t_hrv_filter_bitmask filter_stream_bitmask);

/** \brief Requests stop of data stream for a given Sensor
	Asks HSLService to start stream data for the given Sensor with the given set of stream properties.
	The data in the associated \ref HSLSensor state will get updated automatically in calls to \ref HSL_Update or 
	\ref HSL_UpdateNoPollMessages.
	Requests to restart an already started stream will return an error.
	\param sensor_ The id of the Sensor to start the stream for.
	\return HSLResult_Success upon receiving result, HSLResult_Timeoout, or HSLResult_Error on request error. */
HSL_PUBLIC_FUNCTION(HSLResult) HSL_StopAllSensorDataStreams(HSLSensorID sensor_id);

/** 
@} 
*/ 

//cut_after
#endif
