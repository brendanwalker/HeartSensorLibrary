#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

//-- includes -----
#include <memory>
#include <string>
#include <vector>
#include "DeviceTypeManager.h"
#include "DeviceEnumerator.h"
#include "HSLConfig.h"

//-- typedefs -----
class ServerSensorView;
typedef std::shared_ptr<ServerSensorView> ServerSensorViewPtr;

//-- definitions -----
class SensorManagerConfig : public HSLConfig
{
public:
	static const int CONFIG_VERSION;

	SensorManagerConfig(const std::string &fnamebase = "SensorManagerConfig");

	virtual const configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config &pt);

	int version;
};

class SensorManager : public DeviceTypeManager
{
public:
	SensorManager();

	virtual bool startup() override;
	virtual void shutdown() override;

	static const int k_max_devices = HSLSERVICE_MAX_SENSOR_COUNT;
	int getMaxDevices() const override
	{
			return SensorManager::k_max_devices;
	}

	ServerSensorViewPtr getSensorViewPtr(int device_id);

	inline const SensorManagerConfig& getConfig() const { return m_config; }
	inline const std::string& getBluetoothHostAddress() const { return m_bluetooth_host_address; }

	// Process the update packets from the IMU threads
	void processDevicePacketQueues();

protected:
	bool can_update_connected_devices() override;
	class DeviceEnumerator *allocate_device_enumerator() override;
	void free_device_enumerator(class DeviceEnumerator *) override;
	ServerDeviceView *allocate_device_view(int device_id) override;
	int getListUpdatedResponseType() override;

private:
    class SensorCapabilitiesSet *m_supportedSensors;
	std::string m_bluetooth_host_address;
	SensorManagerConfig m_config;
};

#endif // SENSOR_MANAGER_H
