// -- includes -----
#include "HSLClient_CAPI.h"
#include "HSLClient.h"
#include "HSLService.h"
#include "ServiceRequestHandler.h"
#include "Logger.h"

#include <assert.h>

#ifdef _MSC_VER
	#pragma warning(disable:4996)  // ignore strncpy warning
#endif

// -- macros -----
#define IS_VALID_SENSOR_INDEX(x) ((x) >= 0 && (x) < HSLSERVICE_MAX_SENSOR_COUNT)

// -- constants ----

// -- private data ---
HSLService *g_HSL_service= nullptr;
HSLClient *g_HSL_client= nullptr;

// -- public interface -----
bool HSL_GetIsInitialized()
{
	return g_HSL_client != nullptr && g_HSL_service != nullptr;
}

bool HSL_HasSensorListChanged()
{
	return g_HSL_client != nullptr && g_HSL_client->pollHasSensorListChanged();
}

bool HSL_Initialize(HSLLogSeverityLevel log_level)
{
	bool result= true;

	if (g_HSL_service == nullptr)
	{
		g_HSL_service= new HSLService();
	}

	if (g_HSL_client == nullptr)
	{
		g_HSL_client= new HSLClient();
	}

	if (!g_HSL_service->getIsInitialized())
	{
		if (!g_HSL_service->startup(log_level, g_HSL_client))
		{
			result= false;
		}
	}
	
	if (result == true)
	{
		if (!g_HSL_client->startup(log_level, g_HSL_service->getRequestHandler()))
		{
			result= false;
		}
	}

	if (result != true)
	{
		if (g_HSL_service != nullptr)
		{
			g_HSL_service->shutdown();
			delete g_HSL_service;
			g_HSL_service = nullptr;
		}

		if (g_HSL_client != nullptr)
		{
			g_HSL_client->shutdown();
			delete g_HSL_client;
			g_HSL_client = nullptr;
		}
	}

	return result;
}

bool HSL_GetVersionString(char *out_version_string, size_t max_version_string)
{
		bool result= false;

		if (g_HSL_client != nullptr)
		{
			result = g_HSL_service->getRequestHandler()->getServiceVersion(out_version_string, max_version_string);
		}

		return result;
}

bool HSL_Shutdown()
{
	bool result= false;

	if (g_HSL_client != nullptr)
	{
		g_HSL_client->shutdown();

		delete g_HSL_client;
		g_HSL_client= nullptr;

		result= true;
	}	
	
	if (g_HSL_service != nullptr)
	{
		g_HSL_service->shutdown();

		delete g_HSL_service;
		g_HSL_service= nullptr;

		result= true;
	}	

	return result;
}

bool HSL_Update()
{
	bool result = false;

	// Do normal update stuff and generate server events
	if (HSL_UpdateNoPollEvents() == true)
	{
		// Process all events.
		// Any incoming events become status flags we can poll (ex: pollHasConnectionStatusChanged).
		g_HSL_client->fetchMessagesFromServer();

		result= true;
	}

	return result;
}

bool HSL_UpdateNoPollEvents()
{
	if (g_HSL_service != nullptr && g_HSL_client != nullptr)
	{
		// Flush any messages not processed from the previous update
		g_HSL_client->flushAllServerMessages();

		// Update all the connected sensors
		// Generate new server events (i.e. device list changed)
		g_HSL_service->update();

		// Propagate sensor state to the client sensor views
		g_HSL_client->update();

		return true;
	}

	return false;
}

bool HSL_PollNextMessage(HSLEventMessage *message)
{
	// Poll events queued up by the call to g_HSL_client->update()
	if (g_HSL_client != nullptr)
		return g_HSL_client->fetchNextServerMessage(message) ? true : false;
	else
		return false;
}

/// Sensor Pool
HSLSensor *HSL_GetSensor(HSLSensorID sensor_id)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->getClientSensorView(sensor_id);
	else
		return nullptr;
}

static HSLBufferIterator CreateInvalidIterator()
{
	HSLBufferIterator iter;
	HSL_BufferIteratorReset(&iter);
	return iter;
}

HSLBufferIterator HSL_GetCapabilityBuffer(HSLSensorID sensor_id, HSLSensorCapabilityType cap_type)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->getCapabilityBuffer(sensor_id, cap_type);
	else
		return CreateInvalidIterator();
}

HSLBufferIterator HSL_GetHeartHrvBuffer(
	HSLSensorID sensor_id,
	HSLHeartRateVariabityFilterType filter)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->getHeartRateVariabilityBuffer(sensor_id, filter);
	else
		return CreateInvalidIterator();
}

bool HSL_FlushCapabilityBuffer(HSLSensorID sensor_id, HSLSensorCapabilityType cap_type)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->flushCapabilityBuffer(sensor_id, cap_type);
	else
		return false;
}

bool HSL_FlushHeartHrvBuffer(HSLSensorID sensor_id, HSLHeartRateVariabityFilterType filter)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->flushHeartHrvBuffer(sensor_id, filter);
	else
		return false;
}

bool HSL_IsBufferIteratorValid(HSLBufferIterator *iterator)
{
	return iterator != nullptr && iterator->remaining > 0;
}

void HSL_BufferIteratorReset(HSLBufferIterator* iterator)
{
	assert (iterator != nullptr);
	memset(iterator, 0, sizeof(HSLBufferIterator));
}

bool HSL_BufferIteratorNext(HSLBufferIterator *iterator)
{
	if (HSL_IsBufferIteratorValid(iterator))
	{
		assert(iterator->remaining > 0);
		assert(iterator->bufferCapacity > 0);
		iterator->currentIndex = (iterator->currentIndex + 1) % iterator->bufferCapacity;
		--iterator->remaining;
	}

	return false;
}

void* HSL_BufferIteratorGetValueRaw(HSLBufferIterator *iterator)
{
	if (HSL_IsBufferIteratorValid(iterator))
	{
		unsigned char *byte_buffer = reinterpret_cast<unsigned char *>(iterator->buffer);
		unsigned char *element = &byte_buffer[iterator->currentIndex*iterator->stride];

		return element;
	}

	return nullptr;
}

HSLHeartRateFrame* HSL_BufferIteratorGetHRData(HSLBufferIterator* iterator)
{
	return 
		(iterator->bufferType == HSLBufferType_HRData)
		? (HSLHeartRateFrame *)HSL_BufferIteratorGetValueRaw(iterator) 
		: nullptr;
}

HSLHeartECGFrame* HSL_BufferIteratorGetECGData(HSLBufferIterator* iterator)
{
    return
        (iterator->bufferType == HSLBufferType_ECGData)
        ? (HSLHeartECGFrame*)HSL_BufferIteratorGetValueRaw(iterator)
        : nullptr;
}

HSLHeartPPGFrame* HSL_BufferIteratorGetPPGData(HSLBufferIterator* iterator)
{
    return
        (iterator->bufferType == HSLBufferType_PPGData)
        ? (HSLHeartPPGFrame*)HSL_BufferIteratorGetValueRaw(iterator)
        : nullptr;
}

HSLHeartPPIFrame* HSL_BufferIteratorGetPPIData(HSLBufferIterator* iterator)
{
    return
        (iterator->bufferType == HSLBufferType_PPIData)
        ? (HSLHeartPPIFrame*)HSL_BufferIteratorGetValueRaw(iterator)
        : nullptr;
}

HSLAccelerometerFrame* HSL_BufferIteratorGetAccData(HSLBufferIterator* iterator)
{
    return
        (iterator->bufferType == HSLBufferType_AccData)
        ? (HSLAccelerometerFrame*)HSL_BufferIteratorGetValueRaw(iterator)
        : nullptr;
}

HSLHeartVariabilityFrame* HSL_BufferIteratorGetHRVData(HSLBufferIterator* iterator)
{
    return
        (iterator->bufferType == HSLBufferType_HRVData)
        ? (HSLHeartVariabilityFrame*)HSL_BufferIteratorGetValueRaw(iterator)
        : nullptr;
}

HSLElectrodermalActivityFrame* HSL_BufferIteratorGetEDAData(HSLBufferIterator* iterator)
{
	return
		(iterator->bufferType == HSLBufferType_EDAData)
		? (HSLElectrodermalActivityFrame*)HSL_BufferIteratorGetValueRaw(iterator)
		: nullptr;
}

bool HSL_GetCapabilitySamplingRate(HSLSensorID sensor_id, HSLSensorCapabilityType cap_type, int* out_sampling_rate)
{
	bool result = false;

	if (g_HSL_service != nullptr && IS_VALID_SENSOR_INDEX(sensor_id) && out_sampling_rate != nullptr)
	{
		result = g_HSL_service->getRequestHandler()->getCapabilitySamplingRate(sensor_id, cap_type, *out_sampling_rate);
	}

	return result;
}

bool HSL_GetCapabilityBitResolution(HSLSensorID sensor_id, HSLSensorCapabilityType cap_type, int* out_resolution)
{
	bool result = false;

	if (g_HSL_service != nullptr && IS_VALID_SENSOR_INDEX(sensor_id) && out_resolution != nullptr)
	{
		result = g_HSL_service->getRequestHandler()->getCapabilityBitResolution(sensor_id, cap_type, *out_resolution);
	}

	return result;
}

/// Sensor Requests
bool HSL_GetSensorList(HSLSensorList *out_sensor_list)
{
	bool result= false;

	if (g_HSL_service != nullptr)
	{
		result= g_HSL_service->getRequestHandler()->getSensorList(out_sensor_list);
	}
		
	return result;
}

bool HSL_SetActiveSensorCapabilityStreams(
	HSLSensorID sensor_id, 
	t_hsl_caps_bitmask data_stream_flags)
{
	bool result= false;

	if (g_HSL_service != nullptr && IS_VALID_SENSOR_INDEX(sensor_id))
	{
		result= g_HSL_service->getRequestHandler()->setActiveSensorDataStreams(sensor_id, data_stream_flags);

		if (result)
		{
			HSLSensor *client_sensor= HSL_GetSensor(sensor_id);
			assert(client_sensor != nullptr);

			// Update the set of active data streams on the client sensor state
			client_sensor->activeSensorStreams= g_HSL_service->getRequestHandler()->getActiveSensorDataStreams(sensor_id);
		}
	}

	return result;
}

bool HSL_SetActiveSensorFilterStreams(
	HSLSensorID sensor_id,
	t_hrv_filter_bitmask filter_stream_bitmask)
{
	bool result = false;

	if (g_HSL_service != nullptr && IS_VALID_SENSOR_INDEX(sensor_id))
	{
		result = g_HSL_service->getRequestHandler()->setActiveSensorFilterStreams(sensor_id, filter_stream_bitmask);

		if (result)
		{
			HSLSensor* client_sensor = HSL_GetSensor(sensor_id);
			assert(client_sensor != nullptr);

			// Update the set of active filter streams on the client sensor state
			client_sensor->activeFilterStreams = g_HSL_service->getRequestHandler()->getActiveSensorFilterStreams(sensor_id);
		}
	}

	return result;
}

bool HSL_StopAllSensorStreams(HSLSensorID sensor_id)
{
	bool result= false;

	if (g_HSL_service != nullptr && IS_VALID_SENSOR_INDEX(sensor_id))
	{
		result= g_HSL_service->getRequestHandler()->stopAllActiveSensorStreams(sensor_id);
	}

	return result;
}