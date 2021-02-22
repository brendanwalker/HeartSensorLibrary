//-- includes -----
#include "HSLClient.h"
#include "CircularBuffer.h"
#include "Logger.h"
#include "HSLServiceInterface.h"
#include "ServiceRequestHandler.h"
#include "ServerSensorView.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <thread>
#include <memory>

#include <assert.h>

#ifdef _MSC_VER
	#pragma warning(disable:4996)	 // ignore strncpy warning
#endif

//-- typedefs -----
typedef std::deque<HSLEventMessage> t_message_queue;

// -- macros -----
#define IS_VALID_SENSOR_INDEX(x) ((x) >= 0 && (x) < HSLSERVICE_MAX_SENSOR_COUNT)

template <typename t_buffer_type>
void init_buffer_iterator(HSLSensorBufferType buffer_type, CircularBuffer<t_buffer_type>* circular_buffer, HSLBufferIterator* out_iterator);

template <typename t_buffer_type>
struct HSLClientBufferState
{
	HSLSensorBufferType bufferType;
	CircularBuffer<t_buffer_type> *buffer;
	double newestSampleTime;

	void init(HSLSensorBufferType buffer_type, size_t initial_capacity)
	{
		bufferType= buffer_type;
		buffer = new CircularBuffer<t_buffer_type>(initial_capacity);
		newestSampleTime = -1.0;
	}

	void dispose()
	{
		delete buffer;
		buffer= nullptr;
		newestSampleTime = -1.0;
	}

	void clearSensorData()
	{
		if (buffer != nullptr)
		{
			buffer->reset();
		}
	}

	void copyLatestValues(CircularBuffer<t_buffer_type>& source_buffer)
	{
		// Make sure target buffer has the same capacity
		if (buffer->getCapacity() != source_buffer.getCapacity())
		{
			buffer->setCapacity(source_buffer.getCapacity());
		}

		// Only copy over data the client hasn't seen before
		HSLBufferIterator iter;
		init_buffer_iterator(bufferType, &source_buffer, &iter);

		while (HSL_IsBufferIteratorValid(&iter))
		{
			const t_buffer_type *data= (t_buffer_type *)HSL_BufferIteratorGetValueRaw(&iter);

			if (data->timeInSeconds > newestSampleTime)
			{
				buffer->writeItem(*data);
				newestSampleTime= data->timeInSeconds;
			}

			HSL_BufferIteratorNext(&iter);
		}
	}
};
using HSLClentFilterState= HSLClientBufferState<HSLHeartVariabilityFrame>;

struct HSLClentSensorState
{
	HSLSensor sensor;

	HSLClientBufferState<HSLHeartRateFrame> heartRateBuffer;
	HSLClientBufferState<HSLHeartECGFrame> heartECGBuffer;
	HSLClientBufferState<HSLHeartPPGFrame> heartPPGBuffer;
	HSLClientBufferState<HSLHeartPPIFrame> heartPPIBuffer;
	HSLClientBufferState<HSLAccelerometerFrame> heartAccBuffer;
	HSLClientBufferState<HSLGalvanicSkinResponseFrame> gsrBuffer;

	std::array<HSLClentFilterState, HRVFilter_COUNT> hrvFilters;

	void clearSensorData()
	{
		HSLSensorID sensor_id= sensor.sensorID;
		memset(&sensor, 0, sizeof(HSLSensor));
		sensor.sensorID= sensor_id; // preserve the sensor id assigned in initClientSensorState()

		heartRateBuffer.clearSensorData();
		heartECGBuffer.clearSensorData();
		heartPPGBuffer.clearSensorData();
		heartPPIBuffer.clearSensorData();
		heartAccBuffer.clearSensorData();
		gsrBuffer.clearSensorData();

		for (int filter_index = 0; filter_index < HRVFilter_COUNT; ++filter_index)
		{
			hrvFilters[filter_index].clearSensorData();
		}
	}
};

// -- methods -----
HSLClient::HSLClient()
	: m_requestHandler(nullptr)
	, m_bHasSensorListChanged(false)
{
	m_clientSensors = new HSLClentSensorState[HSLSERVICE_MAX_SENSOR_COUNT];
	memset(m_clientSensors, 0, sizeof(HSLClentSensorState)*HSLSERVICE_MAX_SENSOR_COUNT);
}

HSLClient::~HSLClient()
{
	delete[] m_clientSensors;
}

// -- State Queries ----
bool HSLClient::pollHasSensorListChanged()
{ 
	bool bHasSensorListChanged= m_bHasSensorListChanged;

	m_bHasSensorListChanged= false;

	return bHasSensorListChanged; 
}

// -- ClientHSLAPI System -----
bool HSLClient::startup(
	HSLLogSeverityLevel log_level,
	class ServiceRequestHandler * request_handler)
{
	bool success = true;

	m_requestHandler= request_handler;

	// Reset status flags
	m_bHasSensorListChanged= false;

	HSL_LOG_INFO("HSLClient") << "Successfully initialized HSLClient";

	memset(m_clientSensors, 0, sizeof(HSLClentSensorState)*HSLSERVICE_MAX_SENSOR_COUNT);
	for (HSLSensorID sensor_id= 0; sensor_id < HSLSERVICE_MAX_SENSOR_COUNT; ++sensor_id)		
	{
		initClientSensorState(sensor_id);
	}
	
	return success;
}

void HSLClient::update()
{
	updateAllClientSensorStates(false);
}

void HSLClient::fetchMessagesFromServer()
{
	HSLEventMessage message;
	while(fetchNextServerMessage(&message))
	{
		// Only handle events
		processServerMessage(&message);
	}
}

bool HSLClient::fetchNextServerMessage(HSLEventMessage *message)
{
	bool bHasMessage = false;

	if (m_serverMessageQueue.size() > 0)
	{
		const HSLEventMessage &first = m_serverMessageQueue.front();

		assert(message != nullptr);
		memcpy(message, &first, sizeof(HSLEventMessage));

		// Special handling for HSLEvent_SensorListUpdated
		// Make sure to refresh all client sensor state when the sensor list changes
		if (message->event_type == HSLEvent_SensorListUpdated)
		{
			updateAllClientSensorStates(true);			
		}

		m_serverMessageQueue.pop_front();

		bHasMessage = true;
	}

	return bHasMessage;
}

void HSLClient::flushAllServerMessages()
{
	// Drop an unread messages from the previous call to update
	m_serverMessageQueue.clear();
}

void HSLClient::shutdown()
{
	// Free memory associated with the client circular buffers
	for (HSLSensorID sensor_id = 0; sensor_id < HSLSERVICE_MAX_SENSOR_COUNT; ++sensor_id)
	{
		disposeClientSensorState(sensor_id);
	}

	// Zero out all client sensor state
	memset(m_clientSensors, 0, sizeof(HSLClentSensorState)*HSLSERVICE_MAX_SENSOR_COUNT);

	// Drop an unread messages from the previous call to update
	m_serverMessageQueue.clear();
}

// -- ClientHSLAPI Requests -----
bool HSLClient::initClientSensorState(HSLSensorID sensor_id)
{
	bool bSuccess= false;

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		HSLClentSensorState &clientSensorState = m_clientSensors[sensor_id];
		HSLSensor &sensor = clientSensorState.sensor;

		ServerSensorView *sensor_view= m_requestHandler->getServerSensorView(sensor_id);

		memset(&clientSensorState, 0, sizeof(HSLClentSensorState));
		sensor.sensorID = sensor_id;

		clientSensorState.heartAccBuffer.init(HSLBufferType_AccData, sensor_view->getHeartAccBuffer()->getCapacity());
		clientSensorState.heartECGBuffer.init(HSLBufferType_ECGData, sensor_view->getHeartECGBuffer()->getCapacity());
		clientSensorState.heartPPGBuffer.init(HSLBufferType_PPGData, sensor_view->getHeartPPGBuffer()->getCapacity());
		clientSensorState.heartPPIBuffer.init(HSLBufferType_PPIData, sensor_view->getHeartPPIBuffer()->getCapacity());
		clientSensorState.heartRateBuffer.init(HSLBufferType_HRData, sensor_view->getHeartRateBuffer()->getCapacity());
		clientSensorState.gsrBuffer.init(HSLBufferType_GSRData, sensor_view->getGalvanicSkinResponseBuffer()->getCapacity());

		for (int filter_index = 0; filter_index < HRVFilter_COUNT; ++filter_index)
		{
			HSLHeartRateVariabityFilterType filter = HSLHeartRateVariabityFilterType(filter_index);

			clientSensorState.hrvFilters[filter_index].init(HSLBufferType_HRVData, sensor_view->getHeartHrvBuffer(filter)->getCapacity());
		}

		bSuccess = true;
	}
		
	return bSuccess;
}

void HSLClient::disposeClientSensorState(HSLSensorID sensor_id)
{
	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		HSLClentSensorState& clientSensorState = m_clientSensors[sensor_id];
		HSLSensor& sensor = clientSensorState.sensor;

		ServerSensorView* sensor_view = m_requestHandler->getServerSensorView(sensor_id);

		clientSensorState.heartAccBuffer.dispose();
		clientSensorState.heartECGBuffer.dispose();
		clientSensorState.heartPPGBuffer.dispose();
		clientSensorState.heartPPIBuffer.dispose();
		clientSensorState.heartRateBuffer.dispose();
		clientSensorState.gsrBuffer.dispose();

		for (int filter_index = 0; filter_index < HRVFilter_COUNT; ++filter_index)
		{
			HSLHeartRateVariabityFilterType filter = HSLHeartRateVariabityFilterType(filter_index);

			clientSensorState.hrvFilters[filter_index].dispose();
		}

		memset(&clientSensorState, 0, sizeof(HSLClentSensorState));
	}
}

void HSLClient::updateClientSensorState(HSLSensorID sensor_id, bool updateDeviceInformation)
{
	ServerSensorView* sensor_view = m_requestHandler->getServerSensorView(sensor_id);

	if (sensor_view != nullptr)
	{
		HSLClentSensorState& clientSensorState = m_clientSensors[sensor_id];
		HSLSensor& sensor_capi = clientSensorState.sensor;
		bool wasConnected= sensor_capi.isConnected;
		bool isConnected= sensor_view->getIsOpen();

		if (isConnected)
		{
			sensor_capi.isConnected = true;

			// Copy BPM value from sensor
			sensor_capi.beatsPerMinute = sensor_view->getHeartRateBPM();

			// Copy latest sensor buffer values from the sensor view
			clientSensorState.heartAccBuffer.copyLatestValues(*sensor_view->getHeartAccBuffer());
			clientSensorState.heartECGBuffer.copyLatestValues(*sensor_view->getHeartECGBuffer());
			clientSensorState.heartPPGBuffer.copyLatestValues(*sensor_view->getHeartPPGBuffer());
			clientSensorState.heartPPIBuffer.copyLatestValues(*sensor_view->getHeartPPIBuffer());
			clientSensorState.heartRateBuffer.copyLatestValues(*sensor_view->getHeartRateBuffer());
			clientSensorState.gsrBuffer.copyLatestValues(*sensor_view->getGalvanicSkinResponseBuffer());
			for (int filter_index = 0; filter_index < HRVFilter_COUNT; ++filter_index)
			{
				HSLHeartRateVariabityFilterType filter = HSLHeartRateVariabityFilterType(filter_index);

				clientSensorState.hrvFilters[filter_index].copyLatestValues(*sensor_view->getHeartHrvBuffer(filter));
			}

			// fetch device information if this device just opened
			if (updateDeviceInformation)
			{
				sensor_view->fetchDeviceInformation(&sensor_capi.deviceInformation);
			}
		}
		else if (wasConnected && !isConnected)
		{
			clientSensorState.clearSensorData();
		}
	}
}

void HSLClient::updateAllClientSensorStates(bool updateDeviceInformation)
{
	for (HSLSensorID sensor_id = 0; sensor_id < HSLSERVICE_MAX_SENSOR_COUNT; ++sensor_id)
	{
		updateClientSensorState(sensor_id, updateDeviceInformation);
	}
}

HSLSensor* HSLClient::getClientSensorView(HSLSensorID sensor_id)
{
	return IS_VALID_SENSOR_INDEX(sensor_id) ? &m_clientSensors[sensor_id].sensor : nullptr;
}

template <typename t_buffer_type>
void init_buffer_iterator(
	HSLSensorBufferType buffer_type, 
	CircularBuffer<t_buffer_type> *circular_buffer, 
	HSLBufferIterator *out_iterator)
{
	memset(out_iterator, 0, sizeof(HSLBufferIterator));
	out_iterator->bufferType= buffer_type;
	out_iterator->stride = sizeof(t_buffer_type);

	if (circular_buffer != nullptr)
	{
		out_iterator->buffer = circular_buffer->getBuffer();
		out_iterator->bufferCapacity = circular_buffer->getCapacity();
		out_iterator->remaining = circular_buffer->getSize();
		out_iterator->currentIndex = circular_buffer->getReadIndex();
		out_iterator->endIndex = circular_buffer->getWriteIndex();
	}
}

HSLBufferIterator HSLClient::getHeartRateBuffer(HSLSensorID sensor_id)
{
	HSLBufferIterator iter;
	HSL_BufferIteratorReset(&iter);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLHeartRateFrame> *heartRateBuffer = m_clientSensors[sensor_id].heartRateBuffer.buffer;

		init_buffer_iterator(HSLBufferType_HRData, heartRateBuffer, &iter);
	}

	return iter;
}

HSLBufferIterator HSLClient::getHeartECGBuffer(HSLSensorID sensor_id)
{
	HSLBufferIterator iter;
	HSL_BufferIteratorReset(&iter);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLHeartECGFrame> *heartECGBuffer = m_clientSensors[sensor_id].heartECGBuffer.buffer;
		
		init_buffer_iterator(HSLBufferType_ECGData, heartECGBuffer, &iter);
	}

	return iter;
}

HSLBufferIterator HSLClient::getHeartPPGBuffer(HSLSensorID sensor_id)
{
	HSLBufferIterator iter;
	HSL_BufferIteratorReset(&iter);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLHeartPPGFrame> *heartPPGBuffer = m_clientSensors[sensor_id].heartPPGBuffer.buffer;

		init_buffer_iterator(HSLBufferType_PPGData, heartPPGBuffer, &iter);
	}

	return iter;
}

HSLBufferIterator HSLClient::getHeartPPIBuffer(HSLSensorID sensor_id)
{
	HSLBufferIterator iter;
	HSL_BufferIteratorReset(&iter);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLHeartPPIFrame> *heartPPIBuffer = m_clientSensors[sensor_id].heartPPIBuffer.buffer;

		init_buffer_iterator(HSLBufferType_PPIData, heartPPIBuffer, &iter);
	}

	return iter;
}

HSLBufferIterator HSLClient::getHeartAccBuffer(HSLSensorID sensor_id)
{
	HSLBufferIterator iter;
	HSL_BufferIteratorReset(&iter);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLAccelerometerFrame> *heartAccBuffer = m_clientSensors[sensor_id].heartAccBuffer.buffer;

		init_buffer_iterator(HSLBufferType_AccData, heartAccBuffer, &iter);
	}

	return iter;
}

HSLBufferIterator HSLClient::getHeartHrvBuffer(HSLSensorID sensor_id, HSLHeartRateVariabityFilterType filter)
{
	HSLBufferIterator iter;
	HSL_BufferIteratorReset(&iter);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLHeartVariabilityFrame> *hrvBuffer = m_clientSensors[sensor_id].hrvFilters[filter].buffer;

		init_buffer_iterator(HSLBufferType_HRVData, hrvBuffer, &iter);
	}

	return iter;
}

HSLBufferIterator HSLClient::getGalvanicSkinResponseBuffer(HSLSensorID sensor_id)
{
	HSLBufferIterator iter;
	HSL_BufferIteratorReset(&iter);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLGalvanicSkinResponseFrame>* gsrBuffer = m_clientSensors[sensor_id].gsrBuffer.buffer;

		init_buffer_iterator(HSLBufferType_GSRData, gsrBuffer, &iter);
	}

	return iter;
}

bool HSLClient::flushHeartRateBuffer(HSLSensorID sensor_id)
{
	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		m_clientSensors[sensor_id].heartRateBuffer.clearSensorData();
		return true;
	}

	return false;
}

bool HSLClient::flushHeartECGBuffer(HSLSensorID sensor_id)
{
	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		m_clientSensors[sensor_id].heartECGBuffer.clearSensorData();
		return true;
	}

	return false;
}

bool HSLClient::flushHeartPPGBuffer(HSLSensorID sensor_id)
{
	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		m_clientSensors[sensor_id].heartPPGBuffer.clearSensorData();
		return true;
	}

	return false;
}

bool HSLClient::flushHeartPPIBuffer(HSLSensorID sensor_id)
{
	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		m_clientSensors[sensor_id].heartPPIBuffer.clearSensorData();
		return true;
	}

	return false;
}

bool HSLClient::flushHeartAccBuffer(HSLSensorID sensor_id)
{
	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		m_clientSensors[sensor_id].heartAccBuffer.clearSensorData();
		return true;
	}

	return false;
}

bool HSLClient::flushHeartHrvBuffer(HSLSensorID sensor_id, HSLHeartRateVariabityFilterType filter)
{
	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		m_clientSensors[sensor_id].hrvFilters[filter].clearSensorData();
		return true;
	}

	return false;
}

bool HSLClient::flushGalvanicSkinResponseBuffer(HSLSensorID sensor_id)
{
	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		m_clientSensors[sensor_id].gsrBuffer.clearSensorData();
		return true;
	}

	return false;
}

// INotificationListener
void HSLClient::handleNotification(const HSLEventMessage &event)
{
	m_serverMessageQueue.push_back(event);
}

// Message Helpers
//-----------------
void HSLClient::processServerMessage(
	const HSLEventMessage *event_message)
{
	switch (event_message->event_type)
	{
	// Service Events
	case HSLEvent_SensorListUpdated:
		m_bHasSensorListChanged = true;
		break;
	default:
		assert(0 && "unreachable");
		break;
	}
}
