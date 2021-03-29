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

void ServiceRequestHandler::publishNotification(const HSLEventMessage &message)
{
    m_notificationListener->handleNotification(message);
}
	
// -- sensor requests -----
ServerSensorView *ServiceRequestHandler::getServerSensorView(HSLSensorID sensor_id)
{
    ServerSensorView *sensor_view = nullptr;

    if (Utility::is_index_valid(sensor_id, m_deviceManager->getSensorViewMaxCount()))
    {
        ServerSensorViewPtr sensor_view_ptr = m_deviceManager->getSensorViewPtr(sensor_id);

		sensor_view = sensor_view_ptr.get();
    }

    return sensor_view;
}

bool ServiceRequestHandler::getSensorList(HSLSensorList *out_sensor_list) const
{
	strncpy(out_sensor_list->hostSerial,
			m_deviceManager->getSensorManager()->getBluetoothHostAddress().c_str(),
			sizeof(out_sensor_list->hostSerial));

    memset(out_sensor_list, 0, sizeof(HSLSensorList));
    for (int sensor_id = 0; sensor_id < m_deviceManager->getSensorViewMaxCount(); ++sensor_id)
    {
        ServerSensorViewPtr sensor_view = m_deviceManager->getSensorViewPtr(sensor_id);

        if (sensor_view->getIsOpen() && out_sensor_list->count < HSLSERVICE_MAX_SENSOR_COUNT)
        {
			HSLSensorListEntry *sensor_list_entry = &out_sensor_list->sensors[out_sensor_list->count++];

			sensor_list_entry->sensorID= sensor_view->getDeviceID();
			sensor_view->fetchDeviceInformation(&sensor_list_entry->deviceInformation);
        }
    }

    return true;
}

bool ServiceRequestHandler::setActiveSensorDataStreams(
	HSLSensorID sensor_id, 
	t_hsl_caps_bitmask data_stream_flags)
{
    ServerSensorViewPtr sensor_view = m_deviceManager->getSensorViewPtr(sensor_id);

    if (sensor_view && sensor_view->getIsOpen())
    {
		return sensor_view->setActiveSensorDataStreams(data_stream_flags);
	}

	return false;
}

t_hsl_caps_bitmask ServiceRequestHandler::getActiveSensorDataStreams(HSLSensorID sensor_id) const
{
	ServerSensorViewPtr sensor_view = m_deviceManager->getSensorViewPtr(sensor_id);

	if (sensor_view && sensor_view->getIsOpen())
	{
		return sensor_view->getActiveSensorDataStreams();
	}

	return 0;
}

bool ServiceRequestHandler::setActiveSensorFilterStreams(
	HSLSensorID sensor_id,
	t_hrv_filter_bitmask filter_stream_bitmask)
{
	ServerSensorViewPtr sensor_view = m_deviceManager->getSensorViewPtr(sensor_id);

	if (sensor_view && sensor_view->getIsOpen())
	{
		return sensor_view->setActiveSensorFilterStreams(filter_stream_bitmask);
	}

	return false;
}

t_hrv_filter_bitmask ServiceRequestHandler::getActiveSensorFilterStreams(HSLSensorID sensor_id) const
{
	bool result = false;

	ServerSensorViewPtr sensor_view = m_deviceManager->getSensorViewPtr(sensor_id);

	if (sensor_view && sensor_view->getIsOpen())
	{
		return sensor_view->getActiveSensorFilterStreams();
	}

	return 0;
}

bool ServiceRequestHandler::stopAllActiveSensorStreams(HSLSensorID sensor_id)
{
	return setActiveSensorDataStreams(sensor_id, 0) && setActiveSensorFilterStreams(sensor_id, 0);
}

bool ServiceRequestHandler::getCapabilitySamplingRate(
	HSLSensorID sensor_id,
	HSLSensorCapabilityType cap_type,
	int& out_sampling_rate)
{
	ServerSensorViewPtr sensor_view = m_deviceManager->getSensorViewPtr(sensor_id);

	if (sensor_view && sensor_view->getIsOpen())
	{
		return sensor_view->getCapabilitySamplingRate(cap_type, out_sampling_rate);
	}

	return false;
}

bool ServiceRequestHandler::getCapabilityBitResolution(
	HSLSensorID sensor_id,
	HSLSensorCapabilityType cap_type,
	int& out_resolution)
{
	ServerSensorViewPtr sensor_view = m_deviceManager->getSensorViewPtr(sensor_id);

	if (sensor_view && sensor_view->getIsOpen())
	{
		return sensor_view->getCapabilityBitResolution(cap_type, out_resolution);
	}

	return false;
}

bool ServiceRequestHandler::getServiceVersion(
    char *out_version_string, 
	size_t max_version_string) const
{
    // Return the protocol version
    strncpy(out_version_string, HSL_SERVICE_VERSION_STRING, max_version_string);
		
	return true;
}
