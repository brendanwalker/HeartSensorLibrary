//-- includes -----
#include "AdafruitPacketProcessor.h"

#include "BluetoothLEDeviceManager.h"
#include "BluetoothLEServiceIDs.h"
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

// -- AdafruitPacketProcessor
AdafruitPacketProcessor::AdafruitPacketProcessor(const AdafruitSensorConfig& config)
	: m_GATT_Profile(nullptr)
	, m_EDA_Service(nullptr)
	, m_EDAMeasurement_Characteristic(nullptr)
	, m_EDAMeasurement_CharacteristicValue(nullptr)
	, m_EDAPeriod_Characteristic(nullptr)
	, m_EDAPeriod_CharacteristicValue(nullptr)
	, m_deviceHandle(k_invalid_ble_device_handle)
	, m_sensorListener(nullptr)
	, m_streamCapabilitiesBitmask(0)
	, m_streamListenerBitmask(0)
	, m_bIsRunning(false)
	, m_streamActiveBitmask(0)
	, m_bIsEDANotificationEnabled(false)
	, m_EDACallbackHandle(k_invalid_ble_gatt_event_handle)
{
	m_config = config;
}

void AdafruitPacketProcessor::setConfig(const AdafruitSensorConfig& config)
{
	m_config = config;
	// TODO: Process config changes (sample rates, etc)
}

bool AdafruitPacketProcessor::start(t_bluetoothle_device_handle deviceHandle, BLEGattProfile* gattProfile, ISensorListener* sensorListener)
{
	if (m_bIsRunning)
		return true;

	m_deviceHandle = deviceHandle;
	m_GATT_Profile = gattProfile;
	m_sensorListener = sensorListener;

	// Electrodermal Activity Service
	m_EDA_Service = m_GATT_Profile->findService(*k_Service_EDA_UUID);
	if (m_EDA_Service == nullptr)
		return false;

	m_EDAMeasurement_Characteristic = m_EDA_Service->findCharacteristic(*k_Characteristic_EDA_Measurement_UUID);
	if (m_EDAMeasurement_Characteristic == nullptr)
		return false;

	m_EDAMeasurement_CharacteristicValue = m_EDAMeasurement_Characteristic->getCharacteristicValue();
	if (m_EDAMeasurement_CharacteristicValue == nullptr)
		return false;

	m_EDAMeasurement_Descriptor = m_EDAMeasurement_Characteristic->findDescriptor(*k_Descriptor_EDA_Measurement_UUID);
	if (m_EDAMeasurement_Descriptor == nullptr)
		return false;

	m_EDAMeasurement_DescriptorValue = m_EDAMeasurement_Descriptor->getDescriptorValue();
	if (m_EDAMeasurement_DescriptorValue == nullptr)
		return false;


	m_EDAPeriod_Characteristic = m_EDA_Service->findCharacteristic(*k_Characteristic_EDA_Period_UUID);
	if (m_EDAPeriod_Characteristic == nullptr)
		return false;

	m_EDAPeriod_CharacteristicValue = m_EDAPeriod_Characteristic->getCharacteristicValue();
	if (m_EDAPeriod_CharacteristicValue == nullptr)
		return false;

	fetchStreamCapabilities();

	m_bIsRunning = true;

	return true;
}

void AdafruitPacketProcessor::stop()
{
	if (!m_bIsRunning)
		return;

	if (HSL_BITMASK_GET_FLAG(m_streamActiveBitmask, HSLCapability_ElectrodermalActivity))
		stopElectrodermalActivityStream();

	m_streamActiveBitmask = 0;
	m_bIsRunning = false;
}

// Called from main thread
void AdafruitPacketProcessor::setActiveSensorDataStreams(t_hsl_caps_bitmask data_stream_flags)
{
	// Keep track of the streams being listened for
	m_streamListenerBitmask = data_stream_flags;

	// Process stream activation/deactivation requests
	if (m_streamListenerBitmask != m_streamActiveBitmask)
	{
		for (int stream_index = 0; stream_index < HSLCapability_COUNT; ++stream_index)
		{
			HSLSensorCapabilityType stream_flag = (HSLSensorCapabilityType)stream_index;
			bool wants_active = HSL_BITMASK_GET_FLAG(m_streamListenerBitmask, stream_flag);
			bool is_active = HSL_BITMASK_GET_FLAG(m_streamActiveBitmask, stream_flag);

			if (wants_active && !is_active)
			{
				bool can_activate = HSL_BITMASK_GET_FLAG(m_streamCapabilitiesBitmask, stream_flag);

				if (can_activate)
				{
					switch (stream_flag)
					{
					case HSLCapability_ElectrodermalActivity:
						startElectrodermalActivityStream();
						break;
					}
				}
			}
			else if (!wants_active && is_active)
			{
				switch (stream_flag)
				{
				case HSLCapability_ElectrodermalActivity:
					stopElectrodermalActivityStream();
					break;
				}
			}
		}
	}
}

t_hsl_caps_bitmask AdafruitPacketProcessor::getActiveSensorDataStreams() const
{
	return m_streamListenerBitmask;
}

// Callable from either main thread or worker thread
bool AdafruitPacketProcessor::getStreamCapabilities(t_hsl_caps_bitmask& outStreamCapabilitiesBitmask)
{
	if (m_bIsRunning)
	{
		outStreamCapabilitiesBitmask = m_streamCapabilitiesBitmask;
		return true;
	}

	return false;
}

void AdafruitPacketProcessor::fetchStreamCapabilities()
{
	// Reset the stream caps
	m_streamCapabilitiesBitmask = 0;

	if (m_config.deviceName.find("Bluefruit52") == 0)
	{
		HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLCapability_ElectrodermalActivity);
	}
}

void AdafruitPacketProcessor::startElectrodermalActivityStream()
{
	if (m_bIsEDANotificationEnabled)
		return;

	if (!m_EDAMeasurement_Characteristic->getIsReadable())
		return;

	if (!m_EDAMeasurement_Characteristic->getIsNotifiable())
		return;

	if (!m_EDAPeriod_Characteristic->getIsWritable())
		return;

	if (!m_EDAMeasurement_DescriptorValue->readDescriptorValue())
		return;

	BLEGattDescriptorValue::ClientCharacteristicConfiguration clientConfig;
	if (!m_EDAMeasurement_DescriptorValue->getClientCharacteristicConfiguration(clientConfig))
		return;

	clientConfig.IsSubscribeToNotification = true;
	if (!m_EDAMeasurement_DescriptorValue->setClientCharacteristicConfiguration(clientConfig))
		return;

	if (!m_EDAMeasurement_DescriptorValue->writeDescriptorValue())
		return;

	m_EDACallbackHandle =
		m_EDAMeasurement_Characteristic->registerChangeEvent(
			std::bind(
				&AdafruitPacketProcessor::OnReceivedEDADataPacket, this,
				std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	// Set the refresh period
	int periodMs = 1000 / m_config.edaSampleRate;
	m_EDAPeriod_CharacteristicValue->setData((const uint8_t*)&periodMs, sizeof(int));

	// Time on Heart Rate samples are relative to stream start
	m_edaStreamStartTimestamp = std::chrono::high_resolution_clock::now();

	m_bIsEDANotificationEnabled = true;
}

void AdafruitPacketProcessor::stopElectrodermalActivityStream()
{
	if (!m_bIsEDANotificationEnabled)
		return;

	if (!m_EDAMeasurement_DescriptorValue->readDescriptorValue())
		return;

	BLEGattDescriptorValue::ClientCharacteristicConfiguration clientConfig;
	clientConfig.IsSubscribeToNotification = false;

	if (!m_EDAMeasurement_DescriptorValue->setClientCharacteristicConfiguration(clientConfig))
		return;

	if (!m_EDAMeasurement_DescriptorValue->writeDescriptorValue())
		return;

	if (m_EDACallbackHandle != k_invalid_ble_gatt_event_handle)
	{
		m_EDAMeasurement_Characteristic->unregisterChangeEvent(m_EDACallbackHandle);
		m_EDACallbackHandle = k_invalid_ble_gatt_event_handle;
	}

	m_bIsEDANotificationEnabled = false;
}

void AdafruitPacketProcessor::OnReceivedEDADataPacket(BluetoothGattHandle attributeHandle, uint8_t* data, size_t data_size)
{
	StackBuffer<64> packet_data(data, data_size);

	ISensorListener::SensorPacket packet;
	memset(&packet, 0, sizeof(ISensorListener::SensorPacket));

	packet.payloadType = ISensorListener::SensorPacketPayloadType::EDAFrame;

	auto packet_time = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time_in_stream = packet_time - m_edaStreamStartTimestamp;
	packet.payload.edaFrame.timeInSeconds = time_in_stream.count();

	while (packet_data.canRead())
	{
		const uint16_t raw_adc_value= packet_data.readShort();

		// Formula provided by Grove-GSR_Sensor docs to convert raw ADC measurement [0,1023]
		// to resistence measurement in ohms
		// https://wiki.seeedstudio.com/Grove-GSR_Sensor/
		const double resistence= 
			((1024.0 + 2.0*(double)raw_adc_value) * 10000.0) 
			/ (512.0 - (double)raw_adc_value);

		// Converting resistance in Ohms to conductance in microSiemens
		// Conductance is the reciprocal of resistance: https://en.wikipedia.org/wiki/Siemens_(unit)
		// Multiply that reciprocal by 1x10^6 to get microSiemens, 
		// a standard Electrodermal Activity measurement (a.k.a. Galvanic Skin Response) 
		const double conductance= 1000000.0 / resistence;

		packet.payload.edaFrame.adcValue= raw_adc_value;
		packet.payload.edaFrame.resistanceOhms= resistence;
		packet.payload.edaFrame.conductanceMicroSiemens= conductance;

		m_sensorListener->notifySensorDataReceived(&packet);
	}
}