//-- includes -----
#include "BluetoothQueries.h"
#include "SensorManager.h"
#include "SensorDeviceEnumerator.h"
#include "SensorBluetoothLEDeviceEnumerator.h"
#include "Logger.h"
#include "HSLClient_CAPI.h"
#include "ServerSensorView.h"
#include "ServerDeviceView.h"

//-- methods -----
//-- Tracker Manager Config -----
const int SensorManagerConfig::CONFIG_VERSION = 1;

SensorManagerConfig::SensorManagerConfig(const std::string &fnamebase)
	: HSLConfig(fnamebase)
	, version(SensorManagerConfig::CONFIG_VERSION)
	, heartRateTimeoutMilliSeconds(3000)
{

};

const configuru::Config SensorManagerConfig::writeToJSON()
{
	configuru::Config pt{
		{"version", SensorManagerConfig::CONFIG_VERSION},
		{"heart_rate_timeout_milliseconds", heartRateTimeoutMilliSeconds}
	};

	return pt;
}

void SensorManagerConfig::readFromJSON(const configuru::Config &pt)
{
	version = pt.get_or<int>("version", 0);

	if (version == SensorManagerConfig::CONFIG_VERSION)
	{
		heartRateTimeoutMilliSeconds= pt.get_or<int>("heart_rate_timeout_milliseconds", heartRateTimeoutMilliSeconds);
	}
	else
	{
		HSL_LOG_WARNING("SensorManagerConfig") <<
				"Config version " << version << " does not match expected version " <<
				SensorManagerConfig::CONFIG_VERSION << ", Using defaults.";
	}
}

//-- Sensor Manager -----
SensorManager::SensorManager()
	: DeviceTypeManager(1000, 2)
{
}

bool SensorManager::startup()
{
	bool success = false;

	if (DeviceTypeManager::startup())
	{
		// Load any config from disk
		m_config.load();

		// Save back out the config in case there were updated defaults
		m_config.save();

		success = true;
	}

	if (success)
	{
		if (!bluetooth_get_host_address(m_bluetooth_host_address))
		{
			m_bluetooth_host_address = "00:00:00:00:00:00";
		}
	}

	return success;
}

void SensorManager::shutdown()
{
	DeviceTypeManager::shutdown();
}

void SensorManager::processDevicePacketQueues()
{
	for (int device_id = 0; device_id < getMaxDevices(); ++device_id)
	{
		ServerSensorViewPtr sensorView = getSensorViewPtr(device_id);

		if (sensorView->getIsOpen())
		{
			sensorView->processDevicePacketQueues();
		}
	}
}

ServerSensorViewPtr SensorManager::getSensorViewPtr(int device_id)
{
	assert(m_deviceViews != nullptr);

	return std::static_pointer_cast<ServerSensorView>(m_deviceViews[device_id]);
}

bool SensorManager::can_update_connected_devices()
{
	return true;
}

DeviceEnumerator * SensorManager::allocate_device_enumerator()
{
	return new SensorDeviceEnumerator(SensorDeviceEnumerator::CommunicationType_ALL);
}

void SensorManager::free_device_enumerator(DeviceEnumerator *enumerator)
{
	delete static_cast<SensorDeviceEnumerator *>(enumerator);
}

ServerDeviceView * SensorManager::allocate_device_view(int device_id)
{
	return new ServerSensorView(device_id);
}

int SensorManager::getListUpdatedResponseType()
{
	return HSLEvent_SensorListUpdated;
}