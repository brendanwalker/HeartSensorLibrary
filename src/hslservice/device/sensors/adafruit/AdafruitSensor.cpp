//-- includes -----
#include "BluetoothLEApiInterface.h"
#include "BluetoothLEDeviceManager.h"
#include "BluetoothLEServiceIDs.h"
#include "AdafruitSensor.h"
#include "AdafruitPacketProcessor.h"
#include "SensorDeviceEnumerator.h"
#include "SensorBluetoothLEDeviceEnumerator.h"
#include "Logger.h"
#include "Utility.h"

#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif
#include <math.h>

// -- AdafruitBluetoothLEDetails
void AdafruitBluetoothInfo::reset()
{
	deviceHandle = k_invalid_ble_device_handle;
	gattProfile = nullptr;
	bluetoothAddress = "";
	memset(&deviceInfo, 0, sizeof(HSLDeviceInformation));
}

// -- Adafruit Sensor -----
IDeviceInterface* AdafruitSensor::AdafruitSensorFactory()
{
	return new AdafruitSensor();
}

AdafruitSensor::AdafruitSensor()
	: m_packetProcessor(nullptr)
	, m_sensorListener(nullptr)
{
	m_bluetoothLEDetails.reset();
}

AdafruitSensor::~AdafruitSensor()
{
	if (getIsOpen())
	{
		HSL_LOG_ERROR("~AdafruitSensor") << "Sensor deleted without calling close() first!";
	}

	if (m_packetProcessor)
	{
		delete m_packetProcessor;
	}
}

bool AdafruitSensor::open()
{
	SensorDeviceEnumerator enumerator(SensorDeviceEnumerator::CommunicationType_BLE);
	while (enumerator.isValid())
	{
		if (open(&enumerator))
		{
			return true;
		}

		enumerator.next();
	}

	return false;
}

bool AdafruitSensor::open(
	const DeviceEnumerator* deviceEnum)
{
	const SensorDeviceEnumerator* sensorEnum = static_cast<const SensorDeviceEnumerator*>(deviceEnum);
	const SensorBluetoothLEDeviceEnumerator* sensorBluetoothLEEnum = sensorEnum->getBluetoothLESensorEnumerator();
	const BluetoothLEDeviceEnumerator* deviceBluetoothLEEnum = sensorBluetoothLEEnum->getBluetoothLEDeviceEnumerator();
	const std::string& cur_dev_path = sensorBluetoothLEEnum->getPath();

	bool bSuccess = true;

	if (getIsOpen())
	{
		HSL_LOG_WARNING("AdafruitSensor::open") << "AdafruitSensor(" << cur_dev_path << ") already open. Ignoring request.";
	}
	else
	{
		// Remember the friendly name of the device
		Utility::copyCString(
			sensorBluetoothLEEnum->getFriendlyName().c_str(),
			m_bluetoothLEDetails.deviceInfo.deviceFriendlyName,
			sizeof(m_bluetoothLEDetails.deviceInfo.deviceFriendlyName));

		// Attempt to open device
		HSL_LOG_INFO("AdafruitSensor::open") << "Opening AdafruitSensor(" << cur_dev_path << ")";
		Utility::copyCString(
			cur_dev_path.c_str(),
			m_bluetoothLEDetails.deviceInfo.devicePath,
			sizeof(m_bluetoothLEDetails.deviceInfo.devicePath));
		m_bluetoothLEDetails.deviceHandle = bluetoothle_device_open(deviceBluetoothLEEnum, &m_bluetoothLEDetails.gattProfile);
		if (!getIsOpen())
		{
			HSL_LOG_ERROR("AdafruitSensor::open") << "Failed to open AdafruitSensor(" << cur_dev_path << ")";
			bSuccess = false;
		}

		// Get the bluetooth address of the sensor.
		// This served as a unique id for the config file.
		char szBluetoothAddress[256];
		if (bluetoothle_device_get_bluetooth_address(
			m_bluetoothLEDetails.deviceHandle,
			szBluetoothAddress, sizeof(szBluetoothAddress)))
		{
			m_bluetoothLEDetails.bluetoothAddress = szBluetoothAddress;
		}
		else
		{
			HSL_LOG_WARNING("AdafruitSensor::open") << "  EMPTY Bluetooth Address!";
			bSuccess = false;
		}

		// Load the config file (if it exists yet)
		std::string config_suffix = m_bluetoothLEDetails.deviceInfo.bodyLocation;
		config_suffix.erase(std::remove(config_suffix.begin(), config_suffix.end(), ' '), config_suffix.end());
		m_config = AdafruitSensorConfig(config_suffix);
		m_config.load();
		m_config.deviceName = m_bluetoothLEDetails.deviceInfo.deviceFriendlyName;

		if (!fetchDeviceInformation())
		{
			HSL_LOG_WARNING("AdafruitSensor::open") << "Failed to fetch device information";
			bSuccess = false;
		}

		// Create the sensor processor thread
		if (bSuccess)
		{
			m_packetProcessor = new AdafruitPacketProcessor(m_config);
			bSuccess = m_packetProcessor->start(
				m_bluetoothLEDetails.deviceHandle,
				m_bluetoothLEDetails.gattProfile,
				m_sensorListener);
		}

		// Cache off the stream capabilities of the device
		if (bSuccess)
		{
			bSuccess = m_packetProcessor->getStreamCapabilities(m_bluetoothLEDetails.deviceInfo.capabilities);
		}

		// Write back out the config if we got a valid bluetooth address
		if (bSuccess)
		{
			m_config.save();
		}
	}

	return bSuccess;
}

static void fetchDeviceInfoString(
	const BLEGattService* deviceInfo_Service,
	const BluetoothUUID* characteristicUUID,
	size_t max_string_length,
	char* out_string)
{
	BLEGattCharacteristic* deviceInfo_Characteristic = deviceInfo_Service->findCharacteristic(*characteristicUUID);
	if (deviceInfo_Characteristic == nullptr)
		return;

	BLEGattCharacteristicValue* value = deviceInfo_Characteristic->getCharacteristicValue();

	uint8_t* result_buffer = nullptr;
	size_t result_buffer_size = 0;
	if (value->getData(&result_buffer, &result_buffer_size))
	{
		size_t copy_size = std::min(result_buffer_size, max_string_length);

		memcpy(out_string, result_buffer, copy_size);
		out_string[max_string_length - 1] = '\0';
	}
}

bool AdafruitSensor::fetchDeviceInformation()
{
	const BLEGattService* deviceInfo_Service = m_bluetoothLEDetails.gattProfile->findService(*k_Service_DeviceInformation_UUID);
	if (deviceInfo_Service == nullptr)
		return false;

	HSLDeviceInformation& devInfo = m_bluetoothLEDetails.deviceInfo;
	fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_SystemID_UUID,
		sizeof(devInfo.systemID), devInfo.systemID);
	fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_ModelNumberString_UUID,
		sizeof(devInfo.modelNumberString), devInfo.modelNumberString);
	fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_SerialNumberString_UUID,
		sizeof(devInfo.serialNumberString), devInfo.serialNumberString);
	fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_FirmwareRevisionString_UUID,
		sizeof(devInfo.firmwareRevisionString), devInfo.firmwareRevisionString);
	fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_HardwareRevisionString_UUID,
		sizeof(devInfo.hardwareRevisionString), devInfo.hardwareRevisionString);
	fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_SoftwareRevisionString_UUID,
		sizeof(devInfo.softwareRevisionString), devInfo.softwareRevisionString);
	fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_ManufacturerNameString_UUID,
		sizeof(devInfo.manufacturerNameString), devInfo.manufacturerNameString);

	return true;
}

void AdafruitSensor::close()
{
	if (getIsOpen())
	{
		HSL_LOG_INFO("AdafruitSensor::close") << "Closing AdafruitSensor(" << m_bluetoothLEDetails.deviceInfo.devicePath << ")";

		if (m_packetProcessor != nullptr)
		{
			// halt the HID packet processing thread
			m_packetProcessor->stop();
			delete m_packetProcessor;
			m_packetProcessor = nullptr;
		}

		if (m_bluetoothLEDetails.deviceHandle != k_invalid_ble_device_handle)
		{
			bluetoothle_device_close(m_bluetoothLEDetails.deviceHandle);
			m_bluetoothLEDetails.gattProfile = nullptr;
			m_bluetoothLEDetails.deviceHandle = k_invalid_ble_device_handle;
		}
	}
	else
	{
		HSL_LOG_INFO("AdafruitSensor::close") << "AdafruitSensor(" << m_bluetoothLEDetails.deviceInfo.devicePath << ") already closed. Ignoring request.";
	}
}

bool AdafruitSensor::setActiveSensorDataStreams(t_hsl_caps_bitmask data_stream_flags)
{
	if (m_packetProcessor != nullptr)
	{
		m_packetProcessor->setActiveSensorDataStreams(data_stream_flags);
		return true;
	}

	return false;
}

t_hsl_caps_bitmask AdafruitSensor::getActiveSensorDataStreams() const
{
	if (m_packetProcessor != nullptr)
	{
		return m_packetProcessor->getActiveSensorDataStreams();
	}

	return 0;
}

// Getters
bool AdafruitSensor::matchesDeviceEnumerator(const DeviceEnumerator* enumerator) const
{
	// Down-cast the enumerator so we can use the correct get_path.
	const SensorDeviceEnumerator* pEnum = static_cast<const SensorDeviceEnumerator*>(enumerator);

	bool matches = false;

	if (pEnum->getApiType() == SensorDeviceEnumerator::CommunicationType_BLE)
	{
		const SensorBluetoothLEDeviceEnumerator* sensorBluetoothLEEnum = pEnum->getBluetoothLESensorEnumerator();
		const std::string enumerator_path = pEnum->getPath();

		matches = (enumerator_path == m_bluetoothLEDetails.deviceInfo.devicePath);
	}

	return matches;
}

const std::string AdafruitSensor::getFriendlyName() const
{
	return m_bluetoothLEDetails.deviceInfo.deviceFriendlyName;
}

const std::string AdafruitSensor::getDevicePath() const
{
	return m_bluetoothLEDetails.deviceInfo.devicePath;
}

t_hsl_caps_bitmask AdafruitSensor::getSensorCapabilities() const
{
	t_hsl_caps_bitmask bitmask = 0;

	m_packetProcessor->getStreamCapabilities(bitmask);

	return bitmask;
}

bool AdafruitSensor::getDeviceInformation(HSLDeviceInformation* out_device_info) const
{
	if (getIsOpen())
	{
		*out_device_info = m_bluetoothLEDetails.deviceInfo;
		return true;
	}

	return false;
}

bool AdafruitSensor::getCapabilitySamplingRate(HSLSensorCapabilityType cap_type, int& out_sampling_rate) const
{
	if (cap_type == HSLCapability_ElectrodermalActivity)
	{
		out_sampling_rate = m_config.edaSampleRate;
		return true;
	}

	return false;
}

bool AdafruitSensor::getCapabilityBitResolution(HSLSensorCapabilityType cap_type, int& out_resolution) const
{
	if (cap_type == HSLCapability_ElectrodermalActivity)
	{
		out_resolution= 10; // Always a 10-bit sampling resolution
		return true;
	}

	return false;
}

void AdafruitSensor::getAvailableCapabilitySampleRates(
	HSLSensorCapabilityType flag,
	const int** out_rates,
	int* out_rate_count) const
{
	m_config.getAvailableCapabilitySampleRates(flag, out_rates, out_rate_count);
}

void AdafruitSensor::setCapabilitySampleRate(HSLSensorCapabilityType flag, int sample_rate)
{
	if (m_config.setCapabilitySampleRate(flag, sample_rate))
	{
		// Push updated config to the packet processor
		if (m_packetProcessor != nullptr)
		{
			m_packetProcessor->setConfig(m_config);
		}

		// Save the updated config to disk
		m_config.save();
	}
}

float AdafruitSensor::getSampleHistoryDuration() const
{
	return m_config.sampleHistoryDuration;
}

void AdafruitSensor::setSampleHistoryDuration(float duration)
{
	if (m_config.sampleHistoryDuration != duration)
	{
		m_config.sampleHistoryDuration = duration;

		// Packet processor thread doesn't care about sample history duration

		// Save the updated config to disk
		m_config.save();
	}
}

int AdafruitSensor::getHeartRateVariabliyHistorySize() const
{
	return 0;
}

void AdafruitSensor::setHeartRateVariabliyHistorySize(int sample_count)
{
}

const std::string AdafruitSensor::getBluetoothAddress() const
{
	return m_bluetoothLEDetails.bluetoothAddress;
}

bool AdafruitSensor::getIsOpen() const
{
	return (m_bluetoothLEDetails.deviceHandle != k_invalid_ble_device_handle);
}

void AdafruitSensor::setSensorListener(ISensorListener* listener)
{
	m_sensorListener = listener;
}

// Setters
void AdafruitSensor::setConfig(const AdafruitSensorConfig* config)
{
	m_config = *config;

	if (m_packetProcessor != nullptr)
	{
		m_packetProcessor->setConfig(*config);
	}

	m_config.save();
}
