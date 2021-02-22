//-- includes -----
#include "DeviceManager.h"

#include "DeviceEnumerator.h"
#include "SensorManager.h"
#ifdef WIN32
#include "PlatformDeviceAPIWin32.h"
#endif // WIN32
#include "ServerSensorView.h"
#include "ServiceRequestHandler.h"
#include "Logger.h"
#include "ServerDeviceView.h"
#include "Utility.h"
#include "HSLConfig.h"

#include <chrono>
#include <utility>

// Devices
#include <AdafruitSensor.h>
#include <PolarSensor.h>

//-- constants -----
static const int k_default_sensor_reconnect_interval= 1000; // ms
static const int k_default_sensor_poll_interval= 2; // ms

class DeviceManagerConfig : public HSLConfig
{
public:
	static const int CONFIG_VERSION= 1;

	DeviceManagerConfig(const std::string &fnamebase = "DeviceManagerConfig")
		: HSLConfig(fnamebase)
		, version(DeviceManagerConfig::CONFIG_VERSION)
		, sensor_reconnect_interval(k_default_sensor_reconnect_interval)
		, sensor_poll_interval(k_default_sensor_poll_interval)
		, platform_api_enabled(true)
	{};

	const configuru::Config writeToJSON()
	{
		configuru::Config pt{
				{"version", DeviceManagerConfig::CONFIG_VERSION},
				{"sensor_reconnect_interval", sensor_reconnect_interval},
				{"sensor_poll_interval", sensor_poll_interval},
				{"platform_api_enabled", platform_api_enabled}
			};

		return pt;
	}

	void readFromJSON(const configuru::Config &pt)
	{
		version = pt.get_or<int>("version", 0);

		if (version == (DeviceManagerConfig::CONFIG_VERSION+0))
		{
			sensor_reconnect_interval = pt.get_or<int>("sensor_reconnect_interval", k_default_sensor_reconnect_interval);
			sensor_poll_interval = pt.get_or<int>("sensor_poll_interval", k_default_sensor_poll_interval);
			platform_api_enabled = pt.get_or<bool>("platform_api_enabled", platform_api_enabled);
		}
		else
		{
			HSL_LOG_WARNING("DeviceManagerConfig") <<
					"Config version " << version << " does not match expected version " <<
					(DeviceManagerConfig::CONFIG_VERSION+0) << ", Using defaults.";
		}
	}

	int version;
	int sensor_reconnect_interval;
	int sensor_poll_interval;
	bool platform_api_enabled;
};

// DeviceManager - This is the interface used by HSLSERVICE
DeviceManager *DeviceManager::m_instance= nullptr;

DeviceManager::DeviceManager()
	: m_config() // NULL config until startup
	, m_platform_api_type(_eDevicePlatformApiType_None)
	, m_platform_api(nullptr)
	, m_sensor_manager(new SensorManager())
{
}

DeviceManager::~DeviceManager()
{
	delete m_sensor_manager;

	if (m_platform_api != nullptr)
	{
		delete m_platform_api;
	}
}

bool DeviceManager::startup()
{
	bool success= true;

	m_config = DeviceManagerConfigPtr(new DeviceManagerConfig);

	// Load the config from disk
	m_config->load();

	// Save the config back out again in case defaults changed
	m_config->save();

	// Optionally create the platform device hot plug API
	if (m_config->platform_api_enabled)
	{
		#ifdef WIN32
		m_platform_api_type = _eDevicePlatformApiType_Win32;
		m_platform_api = new PlatformDeviceAPIWin32;
		#endif
		HSL_LOG_INFO("DeviceManager::startup") << "Platform Hotplug API is ENABLED";
	}
	else
	{
		HSL_LOG_INFO("DeviceManager::startup") << "Platform Hotplug API is DISABLED";
	}

	if (m_platform_api != nullptr)
	{
		success &= m_platform_api->startup(this);
	}

	// Register device factory functions
	registerDeviceFactory("Polar H10", PolarSensor::PolarSensorFactory);
	registerDeviceFactory("Polar OH1", PolarSensor::PolarSensorFactory);
	registerDeviceFactory("Bluefruit52", AdafruitSensor::AdafruitSensorFactory);	

	// Register for hotplug events if this platform supports them
	int sensor_reconnect_interval = m_config->sensor_reconnect_interval;
	if (success && m_platform_api_type != _eDevicePlatformApiType_None)
	{
		registerHotplugListener(DeviceClass::DeviceClass_BLE, m_sensor_manager);
		sensor_reconnect_interval = -1;
	}

	m_sensor_manager->reconnect_interval = sensor_reconnect_interval;
	m_sensor_manager->poll_interval = m_config->sensor_poll_interval;
	success &= m_sensor_manager->startup();

	m_instance= this;

	return success;
}

void
DeviceManager::update()
{
	if (m_platform_api != nullptr)
	{
		m_platform_api->pollSystemEvents(); // Send device hotplug events
	}

	m_sensor_manager->pollConnectedDevices(); // Update sensor count

	m_sensor_manager->processDevicePacketQueues(); // Process packets from the device i/o threads
}

void
DeviceManager::shutdown()
{
	if (m_config)
	{
		m_config->save();
	}

	if (m_sensor_manager != nullptr)
	{
	    m_sensor_manager->shutdown();
	}

	if (m_platform_api != nullptr)
	{
		m_platform_api->shutdown();
	}

	m_instance= nullptr;
}

// -- Device Factory ---
void DeviceManager::registerDeviceFactory(
	const std::string &deviceFriendlyName, 
	DeviceFactoryFunction factoryFunc)
{
	if (m_deviceFactoryTable.find(deviceFriendlyName) == m_deviceFactoryTable.end())
	{
		m_deviceFactoryTable.insert(std::make_pair(deviceFriendlyName, factoryFunc));
	}
}

DeviceManager::DeviceFactoryFunction DeviceManager::getFactoryFunction(
	const std::string &deviceFriendlyName) const
{
	for (auto it = m_deviceFactoryTable.begin(); it != m_deviceFactoryTable.end(); ++it)
	{
		const std::string &prefix= it->first;

		if (deviceFriendlyName.find(prefix) == 0)
		{
			return it->second;
		}		
	}

	return DeviceFactoryFunction();
}

// -- Queries ---
bool DeviceManager::get_device_property(
	const DeviceClass deviceClass,
	const int vendor_id,
	const int product_id,
	const char *property_name,
	char *buffer,
	const int buffer_size)
{
	bool bSuccess = false;

	if (m_platform_api != nullptr)
	{
		bSuccess = m_platform_api->get_device_property(
			deviceClass, vendor_id, product_id, property_name, buffer, buffer_size);
	}

	return bSuccess;
}

int  DeviceManager::getSensorViewMaxCount() const
{
	return m_sensor_manager->getMaxDevices();
}

ServerSensorViewPtr DeviceManager::getSensorViewPtr(int device_id)
{
	ServerSensorViewPtr result;
	if (Utility::is_index_valid(device_id, m_sensor_manager->getMaxDevices()))
	{
		result = m_sensor_manager->getSensorViewPtr(device_id);
	}

	return result;
}

// -- Notification --
void DeviceManager::registerHotplugListener(
	DeviceClass device_class,
	IDeviceHotplugListener *listener)
{
	switch (device_class)
	{
	case  DeviceClass::DeviceClass_BLE:
		{			
			DeviceHotplugListener entry;
			entry.listener = listener;
			entry.device_class = DeviceClass::DeviceClass_BLE;
			
			m_listeners.push_back(entry);
		}
		break;
	default:
		break;
	}
}

void
DeviceManager::handle_device_connected(enum DeviceClass device_class, const std::string &device_path)
{
	for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		if (it->device_class == device_class)
		{
			it->listener->handle_device_connected(device_class, device_path);
		}
	}
}

void
DeviceManager::handle_device_disconnected(enum DeviceClass device_class, const std::string &device_path)
{
	for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		if (it->device_class == device_class)
		{
			it->listener->handle_device_disconnected(device_class, device_path);
		}
	}
}