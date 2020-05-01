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

struct HSLClentFilterState
{
	CircularBuffer<HSLHeartVariabilityFrame> *hrvBuffer;
};

struct HSLClentSensorState
{
	HSLSensor sensor;

	CircularBuffer<HSLHeartRateFrame> *heartRateBuffer;
	CircularBuffer<HSLHeartECGFrame> *heartECGBuffer;
	CircularBuffer<HSLHeartPPGFrame> *heartPPGBuffer;
	CircularBuffer<HSLHeartPPIFrame> *heartPPIBuffer;
	CircularBuffer<HSLAccelerometerFrame> *heartAccBuffer;

	std::array<HSLClentFilterState, HRVFilter_COUNT> hrvFilters;
};

// -- methods -----
HSLClient::HSLClient()
	: m_requestHandler(nullptr)
	, m_bHasSensorListChanged(false)
{
	m_clientSensors = new HSLClentSensorState[HSLSERVICE_MAX_SENSOR_COUNT];
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
		m_clientSensors[sensor_id].sensor.sensorID= sensor_id;
	}
	
	return success;
}

void HSLClient::update()
{
	// Drop an unread messages from the previous call to update
	m_message_queue.clear();
}

void HSLClient::process_messages()
{
	HSLEventMessage message;
	while(poll_next_message(&message))
	{
		// Only handle events
		process_event_message(&message);
	}
}

bool HSLClient::poll_next_message(HSLEventMessage *message)
{
	bool bHasMessage = false;

	if (m_message_queue.size() > 0)
	{
		const HSLEventMessage &first = m_message_queue.front();

		assert(message != nullptr);
		memcpy(message, &first, sizeof(HSLEventMessage));

		m_message_queue.pop_front();

		bHasMessage = true;
	}

	return bHasMessage;
}

void HSLClient::shutdown()
{
	// Drop an unread messages from the previous call to update
	m_message_queue.clear();
}

// -- ClientHSLAPI Requests -----
bool HSLClient::allocate_sensor_listener(HSLSensorID sensor_id)
{
	bool bSuccess= false;

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		HSLClentSensorState &clientSensorState = m_clientSensors[sensor_id];
		HSLSensor &sensor = clientSensorState.sensor;

		if (sensor.listenerCount == 0)
		{
			ServerSensorView *sensor_view= m_requestHandler->get_sensor_view_or_null(sensor_id);

			memset(&clientSensorState, 0, sizeof(HSLClentSensorState));
			sensor.sensorID = sensor_id;

			clientSensorState.heartAccBuffer = sensor_view->getHeartAccBuffer();
			clientSensorState.heartECGBuffer = sensor_view->getHeartECGBuffer();
			clientSensorState.heartPPGBuffer = sensor_view->getHeartPPGBuffer();
			clientSensorState.heartPPIBuffer = sensor_view->getHeartPPIBuffer();
			clientSensorState.heartRateBuffer = sensor_view->getHeartRateBuffer();

			for (int filter_index = 0; filter_index < HRVFilter_COUNT; ++filter_index)
			{
				HSLHeartRateVariabityFilterType filter = HSLHeartRateVariabityFilterType(filter_index);

				clientSensorState.hrvFilters[filter_index].hrvBuffer = sensor_view->getHeartHrvBuffer(filter);
			}
		}

		++sensor.listenerCount;
		bSuccess = true;
	}
		
	return bSuccess;
}

void HSLClient::free_sensor_listener(HSLSensorID sensor_id)
{
	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		HSLClentSensorState &clientSensorState= m_clientSensors[sensor_id];
		HSLSensor &sensor= clientSensorState.sensor;

		assert(sensor.listenerCount > 0);
		--sensor.listenerCount;

		if (sensor.listenerCount <= 0)
		{
			memset(&clientSensorState, 0, sizeof(HSLClentSensorState));
			sensor.sensorID= sensor_id;
		}
	}
}

HSLSensor* HSLClient::get_sensor_view(HSLSensorID sensor_id)
{
	return IS_VALID_SENSOR_INDEX(sensor_id) ? &m_clientSensors[sensor_id].sensor : nullptr;
}

bool HSLClient::getHeartRateBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator)
{
	assert(out_iterator != nullptr);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLHeartRateFrame> *heartRateBuffer = m_clientSensors[sensor_id].heartRateBuffer;

		if (heartRateBuffer != nullptr && heartRateBuffer->getSize() > 0)
		{
			out_iterator->bufferType = HSLBufferType_HRData;
			out_iterator->buffer = heartRateBuffer->getBuffer();
			out_iterator->startIndex = (int)heartRateBuffer->getHeadIndex();
			out_iterator->endIndex = (int)heartRateBuffer->getTailIndex();
			out_iterator->stride = sizeof(HSLHeartRateFrame);
			out_iterator->currentIndex = out_iterator->startIndex;

			return true;
		}
	}

	return false;
}

bool HSLClient::getHeartECGBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator)
{
	assert(out_iterator != nullptr);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLHeartECGFrame> *heartECGBuffer = m_clientSensors[sensor_id].heartECGBuffer;
		
		if (heartECGBuffer != nullptr && heartECGBuffer->getSize() > 0)
		{
			out_iterator->bufferType = HSLBufferType_ECGData;
			out_iterator->buffer = heartECGBuffer->getBuffer();
			out_iterator->startIndex = (int)heartECGBuffer->getHeadIndex();
			out_iterator->endIndex = (int)heartECGBuffer->getTailIndex();
			out_iterator->stride = sizeof(HSLHeartECGFrame);
			out_iterator->currentIndex = out_iterator->startIndex;

			return true;
		}
	}

	return false;
}

bool HSLClient::getHeartPPGBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator)
{
	assert(out_iterator != nullptr);


	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLHeartPPGFrame> *heartPPGBuffer = m_clientSensors[sensor_id].heartPPGBuffer;

		if (heartPPGBuffer != nullptr && heartPPGBuffer->getSize() > 0)
		{
			out_iterator->bufferType = HSLBufferType_PPGData;
			out_iterator->buffer = heartPPGBuffer->getBuffer();
			out_iterator->startIndex = (int)heartPPGBuffer->getHeadIndex();
			out_iterator->endIndex = (int)heartPPGBuffer->getTailIndex();
			out_iterator->stride = sizeof(HSLHeartPPGFrame);
			out_iterator->currentIndex = out_iterator->startIndex;

			return true;
		}
	}

	return false;
}

bool HSLClient::getHeartPPIBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator)
{
	assert(out_iterator != nullptr);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLHeartPPIFrame> *heartPPIBuffer = m_clientSensors[sensor_id].heartPPIBuffer;

		if (heartPPIBuffer != nullptr && heartPPIBuffer->getSize() > 0)
		{
			out_iterator->bufferType = HSLBufferType_PPIData;
			out_iterator->buffer = heartPPIBuffer->getBuffer();
			out_iterator->startIndex = (int)heartPPIBuffer->getHeadIndex();
			out_iterator->endIndex = (int)heartPPIBuffer->getTailIndex();
			out_iterator->stride = sizeof(HSLHeartPPIFrame);
			out_iterator->currentIndex = out_iterator->startIndex;

			return true;
		}
	}

	return false;
}

bool HSLClient::getHeartAccBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator)
{
	assert(out_iterator != nullptr);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLAccelerometerFrame> *heartAccBuffer = m_clientSensors[sensor_id].heartAccBuffer;

		if (heartAccBuffer != nullptr && heartAccBuffer->getSize() > 0)
		{
			out_iterator->bufferType = HSLBufferType_AccData;
			out_iterator->buffer = heartAccBuffer->getBuffer();
			out_iterator->startIndex = (int)heartAccBuffer->getHeadIndex();
			out_iterator->endIndex = (int)heartAccBuffer->getTailIndex();
			out_iterator->stride = sizeof(HSLAccelerometerFrame);
			out_iterator->currentIndex = out_iterator->startIndex;

			return true;
		}
	}

	return false;
}

bool HSLClient::getHeartHrvBuffer(
	HSLSensorID sensor_id,
	HSLHeartRateVariabityFilterType filter,
	HSLBufferIterator *out_iterator)
{
	assert(out_iterator != nullptr);

	if (IS_VALID_SENSOR_INDEX(sensor_id))
	{
		CircularBuffer<HSLHeartVariabilityFrame> *hrvBuffer = m_clientSensors[sensor_id].hrvFilters[filter].hrvBuffer;

		if (hrvBuffer != nullptr && hrvBuffer->getSize() > 0)
		{
			out_iterator->bufferType = HSLBufferType_HRVData;
			out_iterator->buffer = hrvBuffer->getBuffer();
			out_iterator->startIndex = (int)hrvBuffer->getHeadIndex();
			out_iterator->endIndex = (int)hrvBuffer->getTailIndex();
			out_iterator->stride = sizeof(HSLHeartVariabilityFrame);
			out_iterator->currentIndex = out_iterator->startIndex;

			return true;
		}
	}

	return false;
}

// INotificationListener
void HSLClient::handle_notification(const HSLEventMessage &event)
{
	m_message_queue.push_back(event);
}

// Message Helpers
//-----------------
void HSLClient::process_event_message(
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
