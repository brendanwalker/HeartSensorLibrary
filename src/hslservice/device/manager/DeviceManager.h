#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

//-- includes -----
#include "DeviceInterface.h"
#include "DevicePlatformInterface.h"
#include "HSLServiceInterface.h"

#include <memory>
#include <chrono>
#include <vector>
#include <map>
#include <functional>

//-- constants -----
enum eDevicePlatformApiType
{
	_eDevicePlatformApiType_None,
#ifdef WIN32
	_eDevicePlatformApiType_Win32,
#endif // WIN32
};

//-- typedefs -----
class DeviceManagerConfig;
typedef std::shared_ptr<DeviceManagerConfig> DeviceManagerConfigPtr;

class ServerSensorView;
typedef std::shared_ptr<ServerSensorView> ServerSensorViewPtr;

//-- definitions -----
struct DeviceHotplugListener
{
	DeviceClass device_class;
	IDeviceHotplugListener *listener;
};

/// This is the class that is actually used by the HSLSERVICE.
class DeviceManager : public IDeviceHotplugListener
{
public:
	DeviceManager();
	virtual ~DeviceManager();

	// -- System ----
	bool startup(); /**< Initialize the interfaces for each specific manager. */
	void update();  /**< Poll all connected devices for each specific manager. */
	void shutdown();/**< Shutdown the interfaces for each specific manager. */

	static inline DeviceManager *getInstance()
	{ return m_instance; }

	// -- Device Factory ---
	using DeviceFactoryFunction = std::function<IDeviceInterface *()>;
	void registerDeviceFactory(const std::string &deviceFriendlyName, DeviceFactoryFunction factoryFunc);
	DeviceFactoryFunction getFactoryFunction(const std::string &deviceFriendlyName) const;

	// -- Accessors ---
	class SensorManager *getSensorManager() { return m_sensor_manager; }
	ServerSensorViewPtr getSensorViewPtr(int sensor_id);

	// -- Queries ---
	inline eDevicePlatformApiType getApiType() const { return m_platform_api_type; }
	bool get_device_property(
		const DeviceClass deviceClass,
		const int vendor_id,
		const int product_id,
		const char *property_name,
		char *buffer,
		const int buffer_size);
	int getSensorViewMaxCount() const;  

	// -- Notification --
	void registerHotplugListener(const DeviceClass device_class, IDeviceHotplugListener *listener);
	void handle_device_connected(enum DeviceClass device_class, const std::string &device_path) override;
	void handle_device_disconnected(enum DeviceClass device_class, const std::string &device_path) override;
		
private:
	/// Singleton instance of the class
	/// Assigned in startup, cleared in teardown
	static DeviceManager *m_instance;

	DeviceManagerConfigPtr m_config;

	// OS specific implementation of hotplug notification
	eDevicePlatformApiType m_platform_api_type;
	IPlatformDeviceAPI *m_platform_api;

	// List of registered hot-plug listeners
	std::vector<DeviceHotplugListener> m_listeners;

	// Table of registered functions that can create devices indexed by device name
	std::map<std::string, DeviceFactoryFunction> m_deviceFactoryTable;

	class SensorManager *m_sensor_manager;
};

#endif  // DEVICE_MANAGER_H