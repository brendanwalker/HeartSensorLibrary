//-- includes -----
#include "PolarPacketProcessor.h"

#include "BluetoothLEDeviceManager.h"
#include "BluetoothLEServiceIDs.h"
#include "PolarSensor.h"
#include "SensorDeviceEnumerator.h"
#include "SensorBluetoothLEDeviceEnumerator.h"
#include "StackBuffer.h"
#include "Logger.h"
#include "Utility.h"
#include "WorkerThread.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <chrono>
#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <thread>

const BluetoothUUID k_Service_PMD_UUID("FB005C80-02E7-F387-1CAD-8ACD2D8DF0C8");
const BluetoothUUID k_Characteristic_PMD_ControlPoint_UUID("FB005C81-02E7-F387-1CAD-8ACD2D8DF0C8");
const BluetoothUUID k_Descriptor_PMD_ControlPoint_UUID("2902");
const BluetoothUUID k_Characteristic_PMD_DataMTU_UUID("FB005C82-02E7-F387-1CAD-8ACD2D8DF0C8");
const BluetoothUUID k_Descriptor_PMD_DataMTU_UUID("2902");

// -- PolarPacketProcessor
PolarPacketProcessor::PolarPacketProcessor(const PolarSensorConfig& config)
	: m_GATT_Profile(nullptr)
	, m_PMD_Service(nullptr)
	, m_PMDCtrlPoint_Characteristic(nullptr)
	, m_PMDCtrlPoint_CharacteristicValue(nullptr)
	, m_PMDCtrlPoint_Descriptor(nullptr)
	, m_PMDCtrlPoint_DescriptorValue(nullptr)
	, m_PMDDataMTU_Characteristic(nullptr)
	, m_PMDDataMTU_Descriptor(nullptr)
	, m_PMDDataMTU_DescriptorValue(nullptr)
	, m_HeartRate_Service(nullptr)
	, m_HeartRateMeasurement_Characteristic(nullptr)
	, m_HeartRateMeasurement_Descriptor(nullptr)
	, m_HeartRateMeasurement_DescriptorValue(nullptr)
	, m_deviceHandle(k_invalid_ble_device_handle)
	, m_sensorListener(nullptr)
	, m_streamCapabilitiesBitmask(0)
	, m_streamListenerBitmask(0)
	, m_bIsRunning(false)
	, m_streamActiveBitmask(0)
	, m_accStreamStartTimestamp(0)
	, m_ecgStreamStartTimestamp(0)
	, m_ppgStreamStartTimestamp(0)
	, m_ppiStreamStartTimestamp(0)
	, m_bIsPMDControlPointIndicationEnabled(false)
	, m_bIsPMDDataMTUNotificationEnabled(false)
	, m_bIsHeartRateNotificationEnabled(false)
	, m_PMDDataMTUCallbackHandle(k_invalid_ble_gatt_event_handle)
	, m_HeartRateCallbackHandle(k_invalid_ble_gatt_event_handle)
{
	m_config= config;
}

void PolarPacketProcessor::setConfig(const PolarSensorConfig& config)
{
	m_config= config;
	// TODO: Process config changes (sample rates, etc)
}

bool PolarPacketProcessor::start(t_bluetoothle_device_handle deviceHandle, BLEGattProfile* gattProfile, ISensorListener* sensorListener)
{
	if (m_bIsRunning)
		return true;

	m_deviceHandle = deviceHandle;
	m_GATT_Profile = gattProfile;
	m_sensorListener = sensorListener;

	// PMD Service
	m_PMD_Service = m_GATT_Profile->findService(k_Service_PMD_UUID);
	if (m_PMD_Service == nullptr)
		return false;

	// PMD Control Point
	m_PMDCtrlPoint_Characteristic = m_PMD_Service->findCharacteristic(k_Characteristic_PMD_ControlPoint_UUID);
	if (m_PMDCtrlPoint_Characteristic == nullptr)
		return false;

	m_PMDCtrlPoint_CharacteristicValue = m_PMDCtrlPoint_Characteristic->getCharacteristicValue();
	if (m_PMDCtrlPoint_CharacteristicValue == nullptr)
		return false;

	m_PMDCtrlPoint_Descriptor = m_PMDCtrlPoint_Characteristic->findDescriptor(k_Descriptor_PMD_ControlPoint_UUID);
	if (m_PMDCtrlPoint_Descriptor == nullptr)
		return false;

	m_PMDCtrlPoint_DescriptorValue = m_PMDCtrlPoint_Descriptor->getDescriptorValue();
	if (m_PMDCtrlPoint_DescriptorValue == nullptr)
		return false;

	// PMD Data MTU
	m_PMDDataMTU_Characteristic = m_PMD_Service->findCharacteristic(k_Characteristic_PMD_DataMTU_UUID);
	if (m_PMDDataMTU_Characteristic == nullptr)
		return false;

	m_PMDDataMTU_Descriptor = m_PMDDataMTU_Characteristic->findDescriptor(k_Descriptor_PMD_DataMTU_UUID);
	if (m_PMDDataMTU_Descriptor == nullptr)
		return false;

	m_PMDDataMTU_DescriptorValue = m_PMDDataMTU_Descriptor->getDescriptorValue();
	if (m_PMDDataMTU_DescriptorValue == nullptr)
		return false;

	// Heart Rate Service
	m_HeartRate_Service = m_GATT_Profile->findService(*k_Service_HeartRate_UUID);
	if (m_HeartRate_Service == nullptr)
		return false;

	m_HeartRateMeasurement_Characteristic = m_HeartRate_Service->findCharacteristic(*k_Characteristic_HeartRateMeasurement_UUID);
	if (m_HeartRateMeasurement_Characteristic == nullptr)
		return false;

	m_HeartRateMeasurement_Descriptor = m_HeartRateMeasurement_Characteristic->findDescriptor(*k_Descriptor_HeartRateMeasurement_UUID);
	if (m_HeartRateMeasurement_Descriptor == nullptr)
		return false;

	m_HeartRateMeasurement_DescriptorValue = m_HeartRateMeasurement_Descriptor->getDescriptorValue();
	if (m_HeartRateMeasurement_DescriptorValue == nullptr)
		return false;

	setPMDControlPointIndicationEnabled(true);
	setPMDDataMTUNotificationEnabled(true);

	fetchStreamCapabilities();

	m_bIsRunning= true;

	return true;
}

void PolarPacketProcessor::stop()
{
	if (!m_bIsRunning)
		return;

	if (HSL_BITMASK_GET_FLAG(m_streamActiveBitmask, HSLStreamFlags_AccData))
		stopAccStream();
	if (HSL_BITMASK_GET_FLAG(m_streamActiveBitmask, HSLStreamFlags_ECGData))
		stopECGStream();
	if (HSL_BITMASK_GET_FLAG(m_streamActiveBitmask, HSLStreamFlags_PPGData))
		stopPPGStream();
	if (HSL_BITMASK_GET_FLAG(m_streamActiveBitmask, HSLStreamFlags_PPIData))
		stopPPIStream();
	if (HSL_BITMASK_GET_FLAG(m_streamActiveBitmask, HSLStreamFlags_HRData))
		stopHeartRateStream();

	m_streamActiveBitmask = 0;

	setPMDControlPointIndicationEnabled(false);
	setPMDDataMTUNotificationEnabled(false);

	m_bIsRunning= false;
}

// Called from main thread
void PolarPacketProcessor::setActiveSensorDataStreams(t_hsl_stream_bitmask data_stream_flags)
{
	// Keep track of the streams being listened for
	m_streamListenerBitmask= data_stream_flags;

	// Process stream activation/deactivation requests
	if (m_streamListenerBitmask != m_streamActiveBitmask)
	{
		for (int stream_index = 0; stream_index < HSLStreamFlags_COUNT; ++stream_index)
		{
			HSLSensorDataStreamFlags stream_flag = (HSLSensorDataStreamFlags)stream_index;
			bool wants_active = HSL_BITMASK_GET_FLAG(m_streamListenerBitmask, stream_flag);
			bool is_active = HSL_BITMASK_GET_FLAG(m_streamActiveBitmask, stream_flag);

			if (wants_active && !is_active)
			{
				bool can_activate = HSL_BITMASK_GET_FLAG(m_streamCapabilitiesBitmask, stream_flag);

				if (can_activate)
				{
					switch (stream_flag)
					{
						case HSLStreamFlags_AccData:
							startAccStream(m_config);
							break;
						case HSLStreamFlags_ECGData:
							startECGStream(m_config);
							break;
						case HSLStreamFlags_PPGData:
							startPPGStream(m_config);
							break;
						case HSLStreamFlags_PPIData:
							startPPIStream(m_config);
							break;
						case HSLStreamFlags_HRData:
							startHeartRateStream();
							break;
					}
				}
			}
			else if (!wants_active && is_active)
			{
				switch (stream_flag)
				{
					case HSLStreamFlags_AccData:
						stopAccStream();
						break;
					case HSLStreamFlags_ECGData:
						stopECGStream();
						break;
					case HSLStreamFlags_PPGData:
						stopPPGStream();
						break;
					case HSLStreamFlags_PPIData:
						stopPPIStream();
						break;
					case HSLStreamFlags_HRData:
						stopHeartRateStream();
						break;
				}
			}
		}
	}
}

t_hsl_stream_bitmask PolarPacketProcessor::getActiveSensorDataStreams() const
{
	return m_streamListenerBitmask;
}

// Callable from either main thread or worker thread
bool PolarPacketProcessor::getStreamCapabilities(t_hsl_stream_bitmask& outStreamCapabilitiesBitmask)
{
	if (m_bIsRunning)
	{
		outStreamCapabilitiesBitmask = m_streamCapabilitiesBitmask;
		return true;
	}

	return false;
}

void PolarPacketProcessor::setPMDControlPointIndicationEnabled(bool bIsEnabled)
{
	if (m_bIsPMDControlPointIndicationEnabled == bIsEnabled)
		return;

	if (!m_PMDCtrlPoint_DescriptorValue->readDescriptorValue())
		return;

	BLEGattDescriptorValue::ClientCharacteristicConfiguration clientConfig;
	if (!m_PMDCtrlPoint_DescriptorValue->getClientCharacteristicConfiguration(clientConfig))
		return;

	clientConfig.IsSubscribeToIndication = bIsEnabled;
	if (!m_PMDCtrlPoint_DescriptorValue->setClientCharacteristicConfiguration(clientConfig))
		return;

	if (!m_PMDCtrlPoint_DescriptorValue->writeDescriptorValue())
		return;

	m_bIsPMDControlPointIndicationEnabled = bIsEnabled;
}

void PolarPacketProcessor::setPMDDataMTUNotificationEnabled(bool bIsEnabled)
{
	if (m_bIsPMDDataMTUNotificationEnabled == bIsEnabled)
		return;

	if (!m_PMDDataMTU_DescriptorValue->readDescriptorValue())
		return;

	BLEGattDescriptorValue::ClientCharacteristicConfiguration clientConfig;
	if (!m_PMDDataMTU_DescriptorValue->getClientCharacteristicConfiguration(clientConfig))
		return;

	clientConfig.IsSubscribeToNotification = bIsEnabled;
	if (!m_PMDDataMTU_DescriptorValue->setClientCharacteristicConfiguration(clientConfig))
		return;

	if (bIsEnabled)
	{
		m_PMDDataMTUCallbackHandle =
			m_PMDDataMTU_Characteristic->registerChangeEvent(
				std::bind(
					&PolarPacketProcessor::OnReceivedPMDDataMTUPacket, this,
					std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	else
	{
		if (m_PMDDataMTUCallbackHandle != k_invalid_ble_gatt_event_handle)
		{
			m_PMDDataMTU_Characteristic->unregisterChangeEvent(m_PMDDataMTUCallbackHandle);
			m_PMDDataMTUCallbackHandle = k_invalid_ble_gatt_event_handle;
		}
	}

	if (!m_PMDDataMTU_DescriptorValue->writeDescriptorValue())
		return;

	m_bIsPMDDataMTUNotificationEnabled = bIsEnabled;
}

void PolarPacketProcessor::fetchStreamCapabilities()
{
	bool bCanFetchCaps= true;

	// Reset the stream caps
	m_streamCapabilitiesBitmask = 0;

	if (!m_PMDCtrlPoint_Characteristic->getIsReadable())
		return;

	BLEGattCharacteristicValue* value = m_PMDCtrlPoint_Characteristic->getCharacteristicValue();

	uint8_t* feature_buffer = nullptr;
	size_t feature_buffer_size = 0;
	if (!value->getData(&feature_buffer, &feature_buffer_size))
		return;

	if (feature_buffer_size == 0)
		return;

	// 0x0F = control point feature read response
	if (feature_buffer[0] == 0x0F)
	{
		const int feature_bitmask = feature_buffer[1];
		const bool ecg_supported = HSL_BITMASK_GET_FLAG(feature_bitmask, 0);
		const bool ppg_supported = HSL_BITMASK_GET_FLAG(feature_bitmask, 1);
		const bool acc_supported = HSL_BITMASK_GET_FLAG(feature_bitmask, 2);
		const bool ppi_supported = HSL_BITMASK_GET_FLAG(feature_bitmask, 3);

		if (ecg_supported)
			HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_ECGData);
		if (ppg_supported)
			HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_PPGData);
		if (acc_supported)
			HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_AccData);
		if (ppi_supported)
			HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_PPIData);

		// This is always supported
		HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_HRData);
	}
	//TODO: This shouldn't be needed but sometimes this query fails.
	// fallback to assuming capabilities based in device name.
	else
	{
		if (m_config.deviceName.find("Polar H10") == 0)
		{
			m_streamCapabilitiesBitmask = 0;
			HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_ECGData);
			HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_AccData);
		}
		else if (m_config.deviceName.find("Polar OH1") == 0)
		{
			m_streamCapabilitiesBitmask = 0;
			HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_PPGData);
			HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_AccData);
			HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_PPIData);
		}

		// Available in all cases
		HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_HRData);
	}

}

bool PolarPacketProcessor::startAccStream(const PolarSensorConfig& config)
{
	StackBuffer<14> stream_settings;

	stream_settings.writeByte(0x02); // Start Measurement
	stream_settings.writeByte(0x02); // Accelerometer Stream

	stream_settings.writeByte(0x00); // Setting Type: 0 = SAMPLE_RATE
	stream_settings.writeByte(0x01); // 1 = array_count(1) 
	stream_settings.writeShort((uint16_t)config.accSampleRate);

	stream_settings.writeByte(0x01); // Setting Type: 1 = RESOLUTION
	stream_settings.writeByte(0x01); // 1 = array_count(1) 
	stream_settings.writeShort(0x0001); // 1 = 16-bit

	stream_settings.writeByte(0x02); // Setting Type: 2 = RANGE
	stream_settings.writeByte(0x01); // 1 = array_count(1) 
	stream_settings.writeShort(0x0008); //  2 = 2G , 4 = 4G , 8 = 8G

	if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getCapacity()))
		return false;

	uint8_t* response_buffer = nullptr;
	size_t response_buffer_size = 0;
	if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
		return false;

	if (response_buffer_size < 4)
		return false;

	uint8_t expected_response_prefix[4] = {
		0xF0, // control point response
		0x02, // op_code(Start Measurement)
		0x02, // measurement_type(ACC)
		0x00 // 00 = error_code(success)
	};
	if (memcmp(response_buffer, expected_response_prefix, 4) != 0)
		return false;

	// Reset the stream start timestamp
	m_accStreamStartTimestamp = 0;

	return true;
}

bool PolarPacketProcessor::stopAccStream()
{
	StackBuffer<2> stream_settings;

	stream_settings.writeByte(0x03); // Stop Measurement
	stream_settings.writeByte(0x02); // Accelerometer Stream

	if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getCapacity()))
		return false;

	uint8_t* response_buffer = nullptr;
	size_t response_buffer_size = 0;
	if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
		return false;

	if (response_buffer_size < 4)
		return false;

	uint8_t expected_response_prefix[4] = {
		0xF0, // control point response
		0x03, // op_code(Stop Measurement)
		0x02, // measurement_type(ACC)
		0x00 // 00 = error_code(success)
	};
	if (memcmp(response_buffer, expected_response_prefix, 4) != 0)
		return false;

	return true;
}

bool PolarPacketProcessor::startECGStream(const PolarSensorConfig& config)
{
	StackBuffer<10> stream_settings;

	stream_settings.writeByte(0x02); // Start Measurement
	stream_settings.writeByte(0x00); // ECG Stream

	stream_settings.writeByte(0x00); // Setting Type: 0 = SAMPLE_RATE
	stream_settings.writeByte(0x01); // 1 = array_count(1) 
	stream_settings.writeShort((uint16_t)config.ecgSampleRate);

	stream_settings.writeByte(0x01); // Setting Type: 1 = RESOLUTION
	stream_settings.writeByte(0x01); // 1 = array_count(1) 
	stream_settings.writeShort(0x000E); // 1 = 14-bit

	if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getCapacity()))
		return false;

	uint8_t* response_buffer = nullptr;
	size_t response_buffer_size = 0;
	if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
		return false;

	if (response_buffer_size < 4)
		return false;

	uint8_t expected_response_prefix[4] = {
		0xF0, // control point response
		0x02, // op_code(Start Measurement)
		0x00, // measurement_type(ECG)
		0x00 // 00 = error_code(success)
	};
	if (memcmp(response_buffer, expected_response_prefix, 4) != 0)
		return false;

	// Reset the stream start timestamp
	m_ecgStreamStartTimestamp = 0;

	return true;
}

bool PolarPacketProcessor::stopECGStream()
{
	StackBuffer<2> stream_settings;

	stream_settings.writeByte(0x03); // Stop Measurement
	stream_settings.writeByte(0x00); // ECG Stream

	if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getCapacity()))
		return false;

	uint8_t* response_buffer = nullptr;
	size_t response_buffer_size = 0;
	if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
		return false;

	if (response_buffer_size < 4)
		return false;

	uint8_t expected_response_prefix[4] = {
		0xF0, // control point response
		0x03, // op_code(Stop Measurement)
		0x00, // measurement_type(ECG)
		0x00 // 00 = error_code(success)
	};
	if (memcmp(response_buffer, expected_response_prefix, 4) != 0)
		return false;

	return true;
}

bool PolarPacketProcessor::startPPGStream(const PolarSensorConfig& config)
{
	StackBuffer<10> stream_settings;

	stream_settings.writeByte(0x02); // Start Measurement
	stream_settings.writeByte(0x01); // PPG Stream

	stream_settings.writeByte(0x00); // Setting Type: 0 = SAMPLE_RATE
	stream_settings.writeByte(0x01); // 1 = array_count(1) 
	stream_settings.writeShort((uint16_t)config.ppgSampleRate);

	stream_settings.writeByte(0x01); // Setting Type: 1 = RESOLUTION
	stream_settings.writeByte(0x01); // 1 = array_count(1) 
	stream_settings.writeShort(0x0016); // 1 = 22-bit

	if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getCapacity()))
		return false;

	uint8_t* response_buffer = nullptr;
	size_t response_buffer_size = 0;
	if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
		return false;

	if (response_buffer_size < 4)
		return false;

	uint8_t expected_response_prefix[4] = {
		0xF0, // control point response
		0x02, // op_code(Start Measurement)
		0x01, // measurement_type(PPG)
		0x00 // 00 = error_code(success)
	};
	if (memcmp(response_buffer, expected_response_prefix, 4) != 0)
		return false;

	// Reset the stream start timestamp
	m_ppgStreamStartTimestamp = 0;

	return true;
}

bool PolarPacketProcessor::stopPPGStream()
{
	StackBuffer<2> stream_settings;

	stream_settings.writeByte(0x03); // Stop Measurement
	stream_settings.writeByte(0x01); // PPG Stream

	if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getCapacity()))
		return false;

	uint8_t* response_buffer = nullptr;
	size_t response_buffer_size = 0;
	if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
		return false;

	if (response_buffer_size < 4)
		return false;

	uint8_t expected_response_prefix[4] = {
		0xF0, // control point response
		0x03, // op_code(Stop Measurement)
		0x01, // measurement_type(PPG)
		0x00 // 00 = error_code(success)
	};
	if (memcmp(response_buffer, expected_response_prefix, 4) != 0)
		return false;

	return true;
}

bool PolarPacketProcessor::startPPIStream(const PolarSensorConfig& config)
{
	StackBuffer<2> stream_settings;

	stream_settings.writeByte(0x02); // Start Measurement
	stream_settings.writeByte(0x03); // PPI Stream

	if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getCapacity()))
		return false;

	uint8_t* response_buffer = nullptr;
	size_t response_buffer_size = 0;
	if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
		return false;

	if (response_buffer_size < 4)
		return false;

	uint8_t expected_response_prefix[4] = {
		0xF0, // control point response
		0x02, // op_code(Start Measurement)
		0x03, // measurement_type(PPI)
		0x00 // 00 = error_code(success)
	};
	if (memcmp(response_buffer, expected_response_prefix, 4) != 0)
		return false;

	// Reset the stream start timestamp
	m_ppiStreamStartTimestamp = 0;

	return true;
}

bool PolarPacketProcessor::stopPPIStream()
{
	StackBuffer<2> stream_settings;

	stream_settings.writeByte(0x03); // Stop Measurement
	stream_settings.writeByte(0x03); // PPI Stream

	if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getCapacity()))
		return false;

	uint8_t* response_buffer = nullptr;
	size_t response_buffer_size = 0;
	if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
		return false;

	if (response_buffer_size < 4)
		return false;

	uint8_t expected_response_prefix[4] = {
		0xF0, // control point response
		0x03, // op_code(Stop Measurement)
		0x03, // measurement_type(PPI)
		0x00 // 00 = error_code(success)
	};
	if (memcmp(response_buffer, expected_response_prefix, 4) != 0)
		return false;

	return true;
}

void PolarPacketProcessor::startHeartRateStream()
{
	if (m_bIsHeartRateNotificationEnabled)
		return;

	if (!m_HeartRateMeasurement_DescriptorValue->readDescriptorValue())
		return;

	BLEGattDescriptorValue::ClientCharacteristicConfiguration clientConfig;
	if (!m_HeartRateMeasurement_DescriptorValue->getClientCharacteristicConfiguration(clientConfig))
		return;

	clientConfig.IsSubscribeToNotification = true;
	if (!m_HeartRateMeasurement_DescriptorValue->setClientCharacteristicConfiguration(clientConfig))
		return;

	if (!m_HeartRateMeasurement_DescriptorValue->writeDescriptorValue())
		return;

	m_HeartRateCallbackHandle =
		m_HeartRateMeasurement_Characteristic->registerChangeEvent(
			std::bind(
				&PolarPacketProcessor::OnReceivedHRDataPacket, this,
				std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	// Time on Heart Rate samples are relative to stream start
	m_hrStreamStartTimestamp = std::chrono::high_resolution_clock::now();

	// From: https://www.bluetooth.com/xml-viewer/?src=https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.heart_rate_control_point.xml
	// resets the value of the Energy Expended field in the Heart Rate Measurement characteristic to 0
	BLEGattCharacteristic* heartRateControlPoint_Characteristic =
		m_HeartRate_Service->findCharacteristic(*k_Characteristic_HeartRateControlPoint_UUID);
	if (heartRateControlPoint_Characteristic != nullptr)
	{
		BLEGattCharacteristicValue* value = heartRateControlPoint_Characteristic->getCharacteristicValue();

		value->setByte(0x01);
	}

	m_bIsHeartRateNotificationEnabled = true;
}

void PolarPacketProcessor::stopHeartRateStream()
{
	if (!m_bIsHeartRateNotificationEnabled)
		return;

	if (!m_HeartRateMeasurement_DescriptorValue->readDescriptorValue())
		return;

	BLEGattDescriptorValue::ClientCharacteristicConfiguration clientConfig;
	clientConfig.IsSubscribeToNotification = false;

	if (!m_HeartRateMeasurement_DescriptorValue->setClientCharacteristicConfiguration(clientConfig))
		return;

	if (!m_HeartRateMeasurement_DescriptorValue->writeDescriptorValue())
		return;

	if (m_HeartRateCallbackHandle != k_invalid_ble_gatt_event_handle)
	{
		m_HeartRateMeasurement_Characteristic->unregisterChangeEvent(m_HeartRateCallbackHandle);
		m_HeartRateCallbackHandle = k_invalid_ble_gatt_event_handle;
	}

	m_bIsHeartRateNotificationEnabled = false;
}

void PolarPacketProcessor::OnReceivedPMDDataMTUPacket(BluetoothGattHandle attributeHandle, uint8_t* data, size_t data_size)
{
	// Send the sensor data for processing by filter
	if (m_sensorListener != nullptr)
	{
		StackBuffer<1024> packet_data(data, data_size);

		ISensorListener::SensorPacket packet;
		memset(&packet, 0, sizeof(ISensorListener::SensorPacket));

		uint8_t frame_type = packet_data.readByte();
		switch (frame_type)
		{
			case 0x00: // ECG
				{
					uint64_t timestamp = packet_data.readLong();
					if (m_ecgStreamStartTimestamp == 0)
					{
						m_ecgStreamStartTimestamp = timestamp;
					}

					const std::chrono::nanoseconds nanoseconds(timestamp - m_ecgStreamStartTimestamp);
					const auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(nanoseconds);

					if (packet_data.readByte() == 0x00) // ECG frame type
					{
						int ecg_value_capacity = ARRAY_SIZE(packet.payload.ecgFrame.ecgValues);

						packet.payloadType = ISensorListener::SensorPacketPayloadType::ECGFrame;
						packet.payload.ecgFrame.timeInSeconds = seconds.count();

						while (packet_data.canRead())
						{
							uint8_t raw_microvolt_value[4] = {0x00, 0x00, 0x00, 0x00};
							packet_data.readBytes(raw_microvolt_value, 3);
							uint32_t* microvolt_value = (uint32_t*)raw_microvolt_value;

							packet.payload.ecgFrame.ecgValues[packet.payload.ecgFrame.ecgValueCount] = (*microvolt_value);
							packet.payload.ecgFrame.ecgValueCount++;

							if (packet.payload.ecgFrame.ecgValueCount >= ecg_value_capacity)
							{
								m_sensorListener->notifySensorDataReceived(&packet);
								packet.payload.ecgFrame.ecgValueCount = 0;
							}
						}

						if (packet.payload.ecgFrame.ecgValueCount > 0)
						{
							m_sensorListener->notifySensorDataReceived(&packet);
						}
					}
				}
				break;
			case 0x01: // PPG
				{
					uint64_t timestamp = packet_data.readLong();
					if (m_ppgStreamStartTimestamp == 0)
					{
						m_ppgStreamStartTimestamp = timestamp;
					}

					const std::chrono::nanoseconds nanoseconds(timestamp - m_ppgStreamStartTimestamp);
					const auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(nanoseconds);

					if (packet_data.readByte() == 0x00) // 24-bit PPG frame type
					{
						int ppg_value_capacity = ARRAY_SIZE(packet.payload.ppgFrame.ppgSamples);

						packet.payloadType = ISensorListener::SensorPacketPayloadType::PPGFrame;
						packet.payload.ppgFrame.timeInSeconds = seconds.count();

						while (packet_data.canRead())
						{
							HSLHeartPPGSample ppgSample;
							memset(&ppgSample, 0, sizeof(HSLHeartPPGSample));

							ppgSample.ppgValue0 = packet_data.read24BitInt();
							ppgSample.ppgValue1 = packet_data.read24BitInt();
							ppgSample.ppgValue2 = packet_data.read24BitInt();
							ppgSample.ambient = packet_data.read24BitInt();

							packet.payload.ppgFrame.ppgSamples[packet.payload.ppgFrame.ppgSampleCount] = ppgSample;
							packet.payload.ppgFrame.ppgSampleCount++;

							if (packet.payload.ppgFrame.ppgSampleCount >= ppg_value_capacity)
							{
								m_sensorListener->notifySensorDataReceived(&packet);
								packet.payload.ppgFrame.ppgSampleCount = 0;
							}
						}

						if (packet.payload.ppgFrame.ppgSampleCount > 0)
						{
							m_sensorListener->notifySensorDataReceived(&packet);
						}
					}
				}
				break;
			case 0x02: // Acc
				{
					uint64_t timestamp = packet_data.readLong();
					if (m_accStreamStartTimestamp == 0)
					{
						m_accStreamStartTimestamp = timestamp;
					}

					const std::chrono::nanoseconds nanoseconds(timestamp - m_accStreamStartTimestamp);
					const auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(nanoseconds);

					if (packet_data.readByte() == 0x01) // 16-bit ACC frame type
					{
						int acc_value_capacity = ARRAY_SIZE(packet.payload.accFrame.accSamples);

						packet.payloadType = ISensorListener::SensorPacketPayloadType::ACCFrame;
						packet.payload.accFrame.timeInSeconds = seconds.count();

						while (packet_data.canRead())
						{
							uint16_t x_milli_g = packet_data.readShort();
							uint16_t y_milli_g = packet_data.readShort();
							uint16_t z_milli_g = packet_data.readShort();

							HSLVector3f& sample = packet.payload.accFrame.accSamples[packet.payload.accFrame.accSampleCount];
							sample.x = (float)x_milli_g / 1000.f;
							sample.y = (float)y_milli_g / 1000.f;
							sample.z = (float)z_milli_g / 1000.f;
							packet.payload.accFrame.accSampleCount++;

							if (packet.payload.accFrame.accSampleCount >= acc_value_capacity)
							{
								m_sensorListener->notifySensorDataReceived(&packet);
								packet.payload.accFrame.accSampleCount = 0;
							}
						}

						if (packet.payload.accFrame.accSampleCount >= 0)
						{
							m_sensorListener->notifySensorDataReceived(&packet);
						}
					}
				}
				break;
			case 0x03: // PPI
				{
					uint64_t timestamp = packet_data.readLong();
					if (timestamp == 0)
					{
						// The documentation says this event should provide a timestamp
						// but it appears to always be 0 for PPI data, so just use the host system time.
						timestamp= std::chrono::high_resolution_clock::now().time_since_epoch().count();   
					}

					if (m_ppiStreamStartTimestamp == 0)
					{
						m_ppiStreamStartTimestamp = timestamp;
					}

					const std::chrono::nanoseconds nanoseconds(timestamp - m_ppiStreamStartTimestamp);
					const auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(nanoseconds);

					if (packet_data.readByte() == 0x00) // PPI frame type
					{
						int ppi_value_capacity = ARRAY_SIZE(packet.payload.ppiFrame.ppiSamples);

						packet.payloadType = ISensorListener::SensorPacketPayloadType::PPIFrame;
						packet.payload.ppiFrame.timeInSeconds = seconds.count();

						while (packet_data.canRead())
						{
							HSLHeartPPISample ppiSample;
							memset(&ppiSample, 0, sizeof(HSLHeartPPISample));

							ppiSample.beatsPerMinute = packet_data.readByte();
							ppiSample.pulseDuration = packet_data.readShort();
							ppiSample.pulseDurationErrorEst = packet_data.readShort();

							uint8_t flags_field = packet_data.readByte();
							ppiSample.blockerBit = HSL_BITMASK_GET_FLAG(flags_field, 0);
							ppiSample.skinContactBit = HSL_BITMASK_GET_FLAG(flags_field, 1);
							ppiSample.supportsSkinContactBit = HSL_BITMASK_GET_FLAG(flags_field, 2);

							packet.payload.ppiFrame.ppiSamples[packet.payload.ppiFrame.ppiSampleCount] = ppiSample;
							packet.payload.ppiFrame.ppiSampleCount++;

							if (packet.payload.ppiFrame.ppiSampleCount >= ppi_value_capacity)
							{
								m_sensorListener->notifySensorDataReceived(&packet);
								packet.payload.ppiFrame.ppiSampleCount = 0;
							}
						}

						if (packet.payload.ppiFrame.ppiSampleCount > 0)
						{
							m_sensorListener->notifySensorDataReceived(&packet);
						}
					}
				}
				break;
			default:
				break;
		}
	}
}

void PolarPacketProcessor::OnReceivedHRDataPacket(BluetoothGattHandle attributeHandle, uint8_t* data, size_t data_size)
{
	StackBuffer<64> packet_data(data, data_size);

	ISensorListener::SensorPacket packet;
	memset(&packet, 0, sizeof(ISensorListener::SensorPacket));

	packet.payloadType = ISensorListener::SensorPacketPayloadType::HRFrame;

	auto packet_time = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time_in_stream = packet_time - m_hrStreamStartTimestamp;
	packet.payload.hrFrame.timeInSeconds = time_in_stream.count();

	// See Heart Rate Service spec: https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=239866
	uint8_t flags_field = packet_data.readByte();
	bool heart_rate_format_flag = HSL_BITMASK_GET_FLAG(flags_field, 0);
	bool contact_status_flag = HSL_BITMASK_GET_FLAG(flags_field, 1);
	bool contact_status_supported_flag = HSL_BITMASK_GET_FLAG(flags_field, 2);
	bool energy_expended_supported_flag = HSL_BITMASK_GET_FLAG(flags_field, 3);
	bool RR_interval_supported_flag = HSL_BITMASK_GET_FLAG(flags_field, 4);

	if (heart_rate_format_flag)
	{
		packet.payload.hrFrame.beatsPerMinute = packet_data.readShort();
	}
	else
	{
		packet.payload.hrFrame.beatsPerMinute = (uint16_t)packet_data.readByte();
	}

	if (contact_status_supported_flag)
	{
		packet.payload.hrFrame.contactStatus =
			contact_status_flag ? HSLContactStatus_Contact : HSLContactStatus_NoContact;
	}
	else
	{
		packet.payload.hrFrame.contactStatus = HSLContactStatus_Invalid;
	}

	if (energy_expended_supported_flag)
	{
		packet.payload.hrFrame.energyExpended = packet_data.readShort();
	}

	if (RR_interval_supported_flag)
	{
		int rr_value_capacity = ARRAY_SIZE(packet.payload.hrFrame.RRIntervals);

		while (packet_data.canRead())
		{
			packet.payload.hrFrame.RRIntervals[packet.payload.hrFrame.RRIntervalCount] = packet_data.readShort();
			packet.payload.hrFrame.RRIntervalCount++;

			if (packet.payload.hrFrame.RRIntervalCount >= rr_value_capacity)
			{
				m_sensorListener->notifySensorDataReceived(&packet);
				packet.payload.hrFrame.RRIntervalCount = 0;
			}
		}

		if (packet.payload.hrFrame.RRIntervalCount >= 0)
		{
			m_sensorListener->notifySensorDataReceived(&packet);
		}
	}

	m_sensorListener->notifySensorDataReceived(&packet);
}