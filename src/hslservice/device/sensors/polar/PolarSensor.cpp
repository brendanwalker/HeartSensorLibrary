//-- includes -----
#include "BluetoothLEApiInterface.h"
#include "BluetoothLEDeviceManager.h"
#include "BluetoothLEServiceIDs.h"
#include "PolarSensor.h"
#include "PolarPacketProcessor.h"
#include "SensorDeviceEnumerator.h"
#include "SensorBluetoothLEDeviceEnumerator.h"
#include "Logger.h"
#include "Utility.h"

#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif
#include <math.h>

// -- PolarBluetoothLEDetails
void PolarBluetoothInfo::reset()
{
	deviceHandle= k_invalid_ble_device_handle;
	gattProfile= nullptr;
	bluetoothAddress= "";
	memset(&deviceInfo, 0, sizeof(HSLDeviceInformation));
}

// -- Polar Sensor -----
IDeviceInterface *PolarSensor::PolarSensorFactory()
{
	return new PolarSensor();
}

PolarSensor::PolarSensor()
    : m_packetProcessor(nullptr)
	, m_sensorListener(nullptr)
{	
    m_bluetoothLEDetails.reset();
}

PolarSensor::~PolarSensor()
{
    if (getIsOpen())
    {
        HSL_LOG_ERROR("~PolarSensor") << "Sensor deleted without calling close() first!";
    }

	if (m_packetProcessor)
	{
		delete m_packetProcessor;
	}
}

bool PolarSensor::open()
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

bool PolarSensor::open(
	const DeviceEnumerator *deviceEnum)
{
	const SensorDeviceEnumerator *sensorEnum = static_cast<const SensorDeviceEnumerator *>(deviceEnum);
	const SensorBluetoothLEDeviceEnumerator *sensorBluetoothLEEnum = sensorEnum->getBluetoothLESensorEnumerator();
	const BluetoothLEDeviceEnumerator *deviceBluetoothLEEnum = sensorBluetoothLEEnum->getBluetoothLEDeviceEnumerator();
	const std::string &cur_dev_path= sensorBluetoothLEEnum->getPath();

    bool bSuccess= true;

    if (getIsOpen())
    {
        HSL_LOG_WARNING("PolarSensor::open") << "PolarSensor(" << cur_dev_path << ") already open. Ignoring request.";
    }
    else
    {
		// Remember the friendly name of the device
		Utility::copyCString(
			sensorBluetoothLEEnum->getFriendlyName().c_str(),
			m_bluetoothLEDetails.deviceInfo.deviceFriendlyName, 
			sizeof(m_bluetoothLEDetails.deviceInfo.deviceFriendlyName));

		// Attempt to open device
        HSL_LOG_INFO("PolarSensor::open") << "Opening PolarSensor(" << cur_dev_path << ")";
		Utility::copyCString(
			cur_dev_path.c_str(),
			m_bluetoothLEDetails.deviceInfo.devicePath,
			sizeof(m_bluetoothLEDetails.deviceInfo.devicePath));
		m_bluetoothLEDetails.deviceHandle= bluetoothle_device_open(deviceBluetoothLEEnum, &m_bluetoothLEDetails.gattProfile);		
        if (!getIsOpen())
        {
			HSL_LOG_ERROR("PolarSensor::open") << "Failed to open PolarSensor(" << cur_dev_path << ")";
			bSuccess = false;
        }
        
		// Get the bluetooth address of the sensor.
		// This served as a unique id for the config file.
        char szBluetoothAddress[256];
		if (bluetoothle_device_get_bluetooth_address(
				m_bluetoothLEDetails.deviceHandle,
				szBluetoothAddress, sizeof(szBluetoothAddress)))
		{
			m_bluetoothLEDetails.bluetoothAddress= szBluetoothAddress;
		}
		else
		{
			HSL_LOG_WARNING("PolarSensor::open") << "  EMPTY Bluetooth Address!";
			bSuccess= false;
		}

		// Load the config file (if it exists yet)
		std::string config_suffix = m_bluetoothLEDetails.deviceInfo.bodyLocation;
		config_suffix.erase(std::remove(config_suffix.begin(), config_suffix.end(), ' '), config_suffix.end());
		m_config = PolarSensorConfig(config_suffix);
		m_config.load();
		m_config.deviceName= m_bluetoothLEDetails.deviceInfo.deviceFriendlyName;

		if (!fetchDeviceInformation())
		{
            HSL_LOG_WARNING("PolarSensor::open") << "Failed to fetch device information";
            bSuccess = false;
		}

		if (!fetchBodySensorLocation())
		{
            HSL_LOG_WARNING("PolarSensor::open") << "No body sensor location available";
			Utility::copyCString( 
				"Unknown",
				m_bluetoothLEDetails.deviceInfo.bodyLocation,
				sizeof(m_bluetoothLEDetails.deviceInfo.bodyLocation));
		}

		// Create the sensor processor thread
		if (bSuccess)
		{
            m_packetProcessor = new PolarPacketProcessor(m_config);
            bSuccess= m_packetProcessor->start(
                m_bluetoothLEDetails.deviceHandle,
                m_bluetoothLEDetails.gattProfile,
                m_sensorListener);
		}

		// Cache off the stream capabilities of the device
		if (bSuccess)
		{
			bSuccess= m_packetProcessor->getStreamCapabilities(m_bluetoothLEDetails.deviceInfo.capabilities);
		}

		// Write back out the config if we got a valid bluetooth address
		if (bSuccess)
		{
			m_config.save();
		}
    }

    return bSuccess;
}

bool PolarSensor::fetchBodySensorLocation()
{
    const BLEGattService* heartRate_Service = m_bluetoothLEDetails.gattProfile->findService(*k_Service_HeartRate_UUID);
    if (heartRate_Service == nullptr)
        return false;

    BLEGattCharacteristic* bodySensorLocation_Characteristic = heartRate_Service->findCharacteristic(*k_Characteristic_BodySensorLocation_UUID);
    if (bodySensorLocation_Characteristic == nullptr)
        return false;

	uint8_t location_enum= 0;
	BLEGattCharacteristicValue* value = bodySensorLocation_Characteristic->getCharacteristicValue();
	if (!value->getByte(location_enum))
		return false;

	HSLDeviceInformation &devInfo= m_bluetoothLEDetails.deviceInfo;

	const char *szLocationName= nullptr;
	switch (location_enum)
	{
	case 0:
		szLocationName= "Other";
		break;
    case 1:
        szLocationName = "Chest";
        break;
    case 2:
        szLocationName = "Wrist";
        break;
    case 3:
        szLocationName = "Finger";
        break;
    case 4:
        szLocationName = "Hand";
        break;
    case 5:
        szLocationName = "Ear Lobe";
        break;
    case 6:
        szLocationName = "Foot";
        break;
	default:
		szLocationName = "Unknown";
		break;
	}

	Utility::copyCString(szLocationName, devInfo.bodyLocation, sizeof(devInfo.bodyLocation));

	return true;
}

static void fetchDeviceInfoString(
	const BLEGattService* deviceInfo_Service,
	const BluetoothUUID* characteristicUUID, 
	size_t max_string_length,
	char *out_string)
{
    BLEGattCharacteristic* deviceInfo_Characteristic = deviceInfo_Service->findCharacteristic(*characteristicUUID);
    if (deviceInfo_Characteristic == nullptr)
        return;

	BLEGattCharacteristicValue *value= deviceInfo_Characteristic->getCharacteristicValue();

	uint8_t *result_buffer= nullptr;
	size_t result_buffer_size = 0;
	if (value->getData(&result_buffer, &result_buffer_size))
	{
		size_t copy_size= std::min(result_buffer_size, max_string_length);

		memcpy(out_string, result_buffer, copy_size);
		out_string[max_string_length-1]= '\0';
	}
}

bool PolarSensor::fetchDeviceInformation()
{
    const BLEGattService* deviceInfo_Service = m_bluetoothLEDetails.gattProfile->findService(*k_Service_DeviceInformation_UUID);
    if (deviceInfo_Service == nullptr)
        return false;

	HSLDeviceInformation &devInfo= m_bluetoothLEDetails.deviceInfo;
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

void PolarSensor::close()
{
    if (getIsOpen())
    {
        HSL_LOG_INFO("PolarSensor::close") << "Closing PolarSensor(" << m_bluetoothLEDetails.deviceInfo.devicePath << ")";

		if (m_packetProcessor != nullptr)
		{
			// halt the HID packet processing thread
			m_packetProcessor->stop();
			delete m_packetProcessor;
			m_packetProcessor= nullptr;
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
        HSL_LOG_INFO("PolarSensor::close") << "PolarSensor(" << m_bluetoothLEDetails.deviceInfo.devicePath << ") already closed. Ignoring request.";
    }
}

bool PolarSensor::setActiveSensorDataStreams(t_hsl_stream_bitmask data_stream_flags)
{
	if (m_packetProcessor != nullptr)
	{
		m_packetProcessor->setActiveSensorDataStreams(data_stream_flags);
		return true;
	}

	return false;
}

t_hsl_stream_bitmask PolarSensor::getActiveSensorDataStreams() const
{
	if (m_packetProcessor != nullptr)
	{
		return m_packetProcessor->getActiveSensorDataStreams();
	}

	return 0;
}

// Getters
bool PolarSensor::matchesDeviceEnumerator(const DeviceEnumerator *enumerator) const
{
    // Down-cast the enumerator so we can use the correct get_path.
    const SensorDeviceEnumerator *pEnum = static_cast<const SensorDeviceEnumerator *>(enumerator);

	bool matches = false;

	if (pEnum->getApiType() == SensorDeviceEnumerator::CommunicationType_BLE)
	{
		const SensorBluetoothLEDeviceEnumerator *sensorBluetoothLEEnum = pEnum->getBluetoothLESensorEnumerator();
		const std::string enumerator_path = pEnum->getPath();

		matches = (enumerator_path == m_bluetoothLEDetails.deviceInfo.devicePath);
	}

    return matches;
}

const std::string PolarSensor::getFriendlyName() const
{
	return m_bluetoothLEDetails.deviceInfo.deviceFriendlyName;
}

const std::string PolarSensor::getDevicePath() const
{
    return m_bluetoothLEDetails.deviceInfo.devicePath;
}

t_hsl_stream_bitmask PolarSensor::getSensorCapabilities() const
{
	t_hsl_stream_bitmask bitmask = 0;

	m_packetProcessor->getStreamCapabilities(bitmask);

	return bitmask;
}

bool PolarSensor::getDeviceInformation(HSLDeviceInformation* out_device_info) const
{
	if (getIsOpen())
	{
        *out_device_info= m_bluetoothLEDetails.deviceInfo;
		return true;
	}

	return false;
}

int PolarSensor::getCapabilitySampleRate(HSLSensorDataStreamFlags flag) const
{
	int sample_rate = 0;

	switch (flag)
	{
	case HSLStreamFlags_HRData:
		sample_rate= 10; // This is just an average rate that we get the data back at
		break;
	case HSLStreamFlags_ECGData:
		sample_rate = m_config.ecgSampleRate;
		break;
	case HSLStreamFlags_AccData:
		sample_rate = m_config.accSampleRate;
		break;
	case HSLStreamFlags_PPGData:
		sample_rate = m_config.ppgSampleRate;
		break;
	case HSLStreamFlags_PPIData:
		sample_rate = 10; // This is just an average rate that we get the data back at
		break;
	}

	return sample_rate;
}

void PolarSensor::getAvailableCapabilitySampleRates(
	HSLSensorDataStreamFlags flag,
	const int **out_rates, 
	int *out_rate_count) const
{
	m_config.getAvailableCapabilitySampleRates(flag, out_rates, out_rate_count);
}

void PolarSensor::setCapabilitySampleRate(HSLSensorDataStreamFlags flag, int sample_rate)
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

float PolarSensor::getSampleHistoryDuration() const
{
	return m_config.sampleHistoryDuration;
}

void PolarSensor::setSampleHistoryDuration(float duration)
{
	if (m_config.sampleHistoryDuration != duration)
	{
		m_config.sampleHistoryDuration = duration;

		// Packet processor thread doesn't care about sample history duration

		// Save the updated config to disk
		m_config.save();
	}
}

int PolarSensor::getHeartRateVariabliyHistorySize() const
{
	return m_config.hrvHistorySize;
}

void PolarSensor::setHeartRateVariabliyHistorySize(int sample_count)
{
	if (m_config.hrvHistorySize != sample_count)
	{
		m_config.hrvHistorySize = sample_count;

		// Packet processor thread doesn't care about hrv history size

		// Save the updated config to disk
		m_config.save();
	}
}

const std::string PolarSensor::getBluetoothAddress() const
{
    return m_bluetoothLEDetails.bluetoothAddress;
}

bool PolarSensor::getIsOpen() const
{
    return (m_bluetoothLEDetails.deviceHandle != k_invalid_ble_device_handle);
}

void PolarSensor::setSensorListener(ISensorListener *listener)
{
	m_sensorListener= listener;
}

// Setters
void PolarSensor::setConfig(const PolarSensorConfig *config)
{
	m_config= *config;

	if (m_packetProcessor != nullptr)
	{
		m_packetProcessor->setConfig(*config);
	}

	m_config.save();
}
