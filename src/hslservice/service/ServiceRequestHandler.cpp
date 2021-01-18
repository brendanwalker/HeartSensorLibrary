//-- includes -----
#include "ServiceRequestHandler.h"

#include "DeviceManager.h"
#include "SensorManager.h"
#include "DeviceEnumerator.h"
#include "Logger.h"
#include "ServerSensorView.h"
#include "ServerDeviceView.h"
#include "ServiceVersion.h"
#include "Utility.h"

#include <cassert>
#include <string>

#ifdef _MSC_VER
    #pragma warning (disable: 4996) // 'This function or variable may be unsafe': strncpy
#endif

ServiceRequestHandler *ServiceRequestHandler::m_instance= nullptr;

//-- implementation -----
ServiceRequestHandler::ServiceRequestHandler()
	: m_deviceManager(nullptr)
	, m_notificationListener(nullptr)
{
}

ServiceRequestHandler::~ServiceRequestHandler()
{
}

bool ServiceRequestHandler::startup(
	class DeviceManager *device_manager,
	class INotificationListener *notification_listener)
{
	m_deviceManager= device_manager;
	m_notificationListener= notification_listener;

	m_instance= this;
	return true;
}	
	
void ServiceRequestHandler::shutdown()
{           
	m_instance= nullptr;
}

void ServiceRequestHandler::publish_notification(const HSLEventMessage &message)
{
    m_notificationListener->handle_notification(message);
}
	
// -- sensor requests -----
ServerSensorView *ServiceRequestHandler::get_sensor_view_or_null(HSLSensorID sensor_id)
{
    ServerSensorView *sensor_view = nullptr;

    if (Utility::is_index_valid(sensor_id, m_deviceManager->getSensorViewMaxCount()))
    {
        ServerSensorViewPtr sensor_view_ptr = m_deviceManager->getSensorViewPtr(sensor_id);

		sensor_view = sensor_view_ptr.get();
    }

    return sensor_view;
}

HSLResult ServiceRequestHandler::get_sensor_list(
	HSLSensorList *out_sensor_list)
{
    memset(out_sensor_list, 0, sizeof(HSLSensorList));
    for (int sensor_id = 0; sensor_id < m_deviceManager->getSensorViewMaxCount(); ++sensor_id)
    {
        ServerSensorViewPtr sensor_view = m_deviceManager->getSensorViewPtr(sensor_id);

        if (out_sensor_list->count < HSLSERVICE_MAX_SENSOR_COUNT && 
			sensor_view->getIsOpen())
        {
			HSLSensor *sensor_info = &out_sensor_list->Sensors[out_sensor_list->count++];

			sensor_view->fetchSensorInfo(sensor_info);
        }
    }

	strncpy(out_sensor_list->host_serial, 
		m_deviceManager->getSensorManager()->getBluetoothHostAddress().c_str(), 
		sizeof(out_sensor_list->host_serial));

    return HSLResult_Success;
}

HSLResult ServiceRequestHandler::setActiveSensorDataStreams(
	HSLSensorID sensor_id, 
	t_hsl_stream_bitmask data_stream_flags,
	t_hrv_filter_bitmask filter_stream_bitmask)
{
	HSLResult result= HSLResult_Error;

    ServerSensorViewPtr sensor_view = m_deviceManager->getSensorViewPtr(sensor_id);

    if (sensor_view && sensor_view->getIsOpen())
    {
		bool bSuccess= sensor_view->setActiveSensorDataStreams(data_stream_flags, filter_stream_bitmask);

        // Return the name of the shared memory block the video frames will be written to
		result= bSuccess ? HSLResult_Success : HSLResult_Error;
	}

	return result;
}

HSLResult ServiceRequestHandler::stopAllActiveSensorDataStreams(HSLSensorID sensor_id)
{
	return setActiveSensorDataStreams(sensor_id, 0, 0);
}

HSLResult ServiceRequestHandler::get_service_version(
    char *out_version_string, 
	size_t max_version_string)
{
    // Return the protocol version
    strncpy(out_version_string, HSL_SERVICE_VERSION_STRING, max_version_string);
		
	return HSLResult_Success;
}
