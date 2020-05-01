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

HSLResult HSL_Initialize(HSLLogSeverityLevel log_level)
{
	HSLResult result= HSLResult_Success;

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
			result= HSLResult_Error;
		}
	}
	
	if (result == HSLResult_Success)
	{
		if (!g_HSL_client->startup(log_level, g_HSL_service->getRequestHandler()))
		{
			result= HSLResult_Error;
		}
	}

		if (result != HSLResult_Success)
		{
				if (g_HSL_service != nullptr)
				{
					g_HSL_service->shutdown();
					delete g_HSL_service;
					g_HSL_service= nullptr;
				}

				if (g_HSL_client != nullptr)
				{
					g_HSL_client->shutdown();
					delete g_HSL_client;
					g_HSL_client= nullptr;
				}
		}

		return result;
}

HSLResult HSL_GetVersionString(char *out_version_string, size_t max_version_string)
{
		HSLResult result= HSLResult_Error;

		if (g_HSL_client != nullptr)
		{
				result = g_HSL_service->getRequestHandler()->get_service_version(out_version_string, max_version_string);
		}

		return result;
}

HSLResult HSL_Shutdown()
{
	HSLResult result= HSLResult_Error;

	if (g_HSL_client != nullptr)
	{
		g_HSL_client->shutdown();

		delete g_HSL_client;
		g_HSL_client= nullptr;

		result= HSLResult_Success;
	}	
	
	if (g_HSL_service != nullptr)
	{
		g_HSL_service->shutdown();

		delete g_HSL_service;
		g_HSL_service= nullptr;

		result= HSLResult_Success;
	}	

		return result;
}

HSLResult HSL_Update()
{
		HSLResult result = HSLResult_Error;

		if (HSL_UpdateNoPollEvents() == HSLResult_Success)
		{
			// Process all events and responses
			// Any incoming events become status flags we can poll (ex: pollHasConnectionStatusChanged)
			g_HSL_client->process_messages();

				result= HSLResult_Success;
		}

		return result;
}

HSLResult HSL_UpdateNoPollEvents()
{
		HSLResult result= HSLResult_Error;

		if (g_HSL_service != nullptr)
		{
			g_HSL_service->update();

			if (g_HSL_client != nullptr)
			{
				g_HSL_client->update();

				result= HSLResult_Success;
			}
		}	

		return result;
}

HSLResult HSL_PollNextMessage(HSLEventMessage *message, size_t message_size)
{
	// Poll events queued up by the call to g_HSL_client->update()
	if (g_HSL_client != nullptr)
		return g_HSL_client->poll_next_message(message) ? HSLResult_Success : HSLResult_Error;
	else
		return HSLResult_Error;
}

/// Sensor Pool
HSLSensor *HSL_GetSensor(HSLSensorID sensor_id)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->get_sensor_view(sensor_id);
	else
		return nullptr;
}

HSLResult HSL_GetHeartRateBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->getHeartRateBuffer(sensor_id, out_iterator) ? HSLResult_Success : HSLResult_Error;
	else
		return HSLResult_Error;
}

HSLResult HSL_GetHeartECGBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->getHeartECGBuffer(sensor_id, out_iterator) ? HSLResult_Success : HSLResult_Error;
	else
		return HSLResult_Error;
}

HSLResult HSL_GetHeartPPGBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->getHeartPPGBuffer(sensor_id, out_iterator) ? HSLResult_Success : HSLResult_Error;
	else
		return HSLResult_Error;
}

HSLResult HSL_GetHeartPPIBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->getHeartPPIBuffer(sensor_id, out_iterator) ? HSLResult_Success : HSLResult_Error;
	else
		return HSLResult_Error;
}

HSLResult HSL_GetHeartAccBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->getHeartAccBuffer(sensor_id, out_iterator) ? HSLResult_Success : HSLResult_Error;
	else
		return HSLResult_Error;
}

HSLResult HSL_GetHeartHrvBuffer(
	HSLSensorID sensor_id,
	HSLHeartRateVariabityFilterType filter,
	HSLBufferIterator *out_iterator)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->getHeartHrvBuffer(sensor_id, filter, out_iterator) ? HSLResult_Success : HSLResult_Error;
	else
		return HSLResult_Error;
}

bool HSL_IsBufferIteratorValid(HSLBufferIterator *iterator)
{
	return iterator != nullptr && iterator->currentIndex != iterator->endIndex;
}

bool HSL_BufferIteratorNext(HSLBufferIterator *iterator)
{
	if (HSL_IsBufferIteratorValid(iterator))
	{
		const size_t total_elems = iterator->bufferSize / iterator->stride;

		iterator->currentIndex = (iterator->currentIndex + 1) % total_elems;
	}

	return false;
}

static void * HSL_BufferIteratorGetValueRaw(HSLBufferIterator *iterator)
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

HSLResult HSL_AllocateSensorListener(HSLSensorID sensor_id)
{
	if (g_HSL_client != nullptr)
		return g_HSL_client->allocate_sensor_listener(sensor_id) ? HSLResult_Success : HSLResult_Error;
	else
		return HSLResult_Error;
}

HSLResult HSL_FreeSensorListener(HSLSensorID sensor_id)
{
		HSLResult result= HSLResult_Error;

		if (g_HSL_client != nullptr && IS_VALID_SENSOR_INDEX(sensor_id))
		{
		g_HSL_client->free_sensor_listener(sensor_id);

				result= HSLResult_Success;
		}

		return result;
}

/// Sensor Requests
HSLResult HSL_GetSensorList(HSLSensorList *out_sensor_list)
{
	HSLResult result= HSLResult_Error;

	if (g_HSL_service != nullptr)
	{
		result= g_HSL_service->getRequestHandler()->get_sensor_list(out_sensor_list);
	}
		
	return result;
}

HSLResult HSL_SetActiveSensorDataStreams(
	HSLSensorID sensor_id, 
	t_hsl_stream_bitmask data_stream_flags,
	t_hrv_filter_bitmask filter_stream_bitmask)
{
	HSLResult result= HSLResult_Error;

	if (g_HSL_service != nullptr && IS_VALID_SENSOR_INDEX(sensor_id))
	{
		result= g_HSL_service->getRequestHandler()->setActiveSensorDataStreams(
			sensor_id, data_stream_flags, filter_stream_bitmask);
	}

	return result;
}

HSLResult HSL_StopAllSensorDataStreams(HSLSensorID sensor_id)
{
	HSLResult result= HSLResult_Error;

	if (g_HSL_service != nullptr && IS_VALID_SENSOR_INDEX(sensor_id))
	{
		result= g_HSL_service->getRequestHandler()->stopAllActiveSensorDataStreams(sensor_id);
	}

	return result;
}