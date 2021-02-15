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

const BluetoothUUID k_Service_GSR_UUID("B9C80E00-5875-4884-A84B-E3EDF3598BF3");
const BluetoothUUID k_Characteristic_GSR_Measurement_UUID("B9C80E01-5875-4884-A84B-E3EDF3598BF3");
const BluetoothUUID k_Characteristic_GSR_Period_UUID("ADAF0001-C332-42A8-93BD-25E905756cB8");

// -- AdafruitPacketProcessor
AdafruitPacketProcessor::AdafruitPacketProcessor(const AdafruitSensorConfig& config)
	: m_GATT_Profile(nullptr)
	, m_GSR_Service(nullptr)
	, m_GSRMeasurement_Characteristic(nullptr)
	, m_GSRMeasurement_CharacteristicValue(nullptr)
	, m_GSRPeriod_Characteristic(nullptr)
	, m_GSRPeriod_CharacteristicValue(nullptr)
	, m_deviceHandle(k_invalid_ble_device_handle)
	, m_sensorListener(nullptr)
	, m_streamCapabilitiesBitmask(0)
	, m_streamListenerBitmask(0)
	, m_bIsRunning(false)
	, m_streamActiveBitmask(0)
	, m_bIsGSRNotificationEnabled(false)
	, m_GSRCallbackHandle(k_invalid_ble_gatt_event_handle)
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

	// Galvanic Skin Response Service
	m_GSR_Service = m_GATT_Profile->findService(k_Service_GSR_UUID);
	if (m_GSR_Service == nullptr)
		return false;

	m_GSRMeasurement_Characteristic = m_GSR_Service->findCharacteristic(k_Characteristic_GSR_Measurement_UUID);
	if (m_GSRMeasurement_Characteristic == nullptr)
		return false;

	m_GSRMeasurement_CharacteristicValue = m_GSRMeasurement_Characteristic->getCharacteristicValue();
	if (m_GSRMeasurement_CharacteristicValue == nullptr)
		return false;

	m_GSRPeriod_Characteristic = m_GSR_Service->findCharacteristic(k_Characteristic_GSR_Period_UUID);
	if (m_GSRPeriod_Characteristic == nullptr)
		return false;

	m_GSRPeriod_CharacteristicValue = m_GSRPeriod_Characteristic->getCharacteristicValue();
	if (m_GSRPeriod_CharacteristicValue == nullptr)
		return false;

	fetchStreamCapabilities();

	m_bIsRunning = true;

	return true;
}

void AdafruitPacketProcessor::stop()
{
	if (!m_bIsRunning)
		return;

	if (HSL_BITMASK_GET_FLAG(m_streamActiveBitmask, HSLStreamFlags_GSRData))
		stopGalvanicSkinResponseStream();

	m_streamActiveBitmask = 0;
	m_bIsRunning = false;
}

// Called from main thread
void AdafruitPacketProcessor::setActiveSensorDataStreams(t_hsl_stream_bitmask data_stream_flags)
{
	// Keep track of the streams being listened for
	m_streamListenerBitmask = data_stream_flags;

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
					case HSLStreamFlags_GSRData:
						startGalvanicSkinResponseStream();
						break;
					}
				}
			}
			else if (!wants_active && is_active)
			{
				switch (stream_flag)
				{
				case HSLStreamFlags_GSRData:
					stopGalvanicSkinResponseStream();
					break;
				}
			}
		}
	}
}

t_hsl_stream_bitmask AdafruitPacketProcessor::getActiveSensorDataStreams() const
{
	return m_streamListenerBitmask;
}

// Callable from either main thread or worker thread
bool AdafruitPacketProcessor::getStreamCapabilities(t_hsl_stream_bitmask& outStreamCapabilitiesBitmask)
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

	if (m_config.deviceName.find("Bluefruit Feather52") == 0)
	{
		HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_GSRData);
	}
}

void AdafruitPacketProcessor::startGalvanicSkinResponseStream()
{
	if (m_bIsGSRNotificationEnabled)
		return;

	if (!m_GSRMeasurement_Characteristic->getIsReadable())
		return;

	if (!m_GSRMeasurement_Characteristic->getIsNotifiable())
		return;

	if (!m_GSRPeriod_Characteristic->getIsWritable())
		return;

	int periodMs= 1000 / m_config.gsrSampleRate;
	m_GSRPeriod_CharacteristicValue->setData((const uint8_t *)&periodMs, sizeof(int));

	m_GSRCallbackHandle =
		m_GSRMeasurement_Characteristic->registerChangeEvent(
			std::bind(
				&AdafruitPacketProcessor::OnReceivedGSRDataPacket, this,
				std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	// Time on Heart Rate samples are relative to stream start
	m_gsrStreamStartTimestamp = std::chrono::high_resolution_clock::now();

	m_bIsGSRNotificationEnabled = true;
}

void AdafruitPacketProcessor::stopGalvanicSkinResponseStream()
{
	if (!m_bIsGSRNotificationEnabled)
		return;

	if (m_GSRCallbackHandle != k_invalid_ble_gatt_event_handle)
	{
		m_GSRMeasurement_Characteristic->unregisterChangeEvent(m_GSRCallbackHandle);
		m_GSRCallbackHandle = k_invalid_ble_gatt_event_handle;
	}

	m_bIsGSRNotificationEnabled = false;
}

void AdafruitPacketProcessor::OnReceivedGSRDataPacket(BluetoothGattHandle attributeHandle, uint8_t* data, size_t data_size)
{
	StackBuffer<64> packet_data(data, data_size);

	ISensorListener::SensorPacket packet;
	memset(&packet, 0, sizeof(ISensorListener::SensorPacket));

	packet.payloadType = ISensorListener::SensorPacketPayloadType::GSRFrame;

	auto packet_time = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time_in_stream = packet_time - m_gsrStreamStartTimestamp;
	packet.payload.gsrFrame.timeInSeconds = time_in_stream.count();

	while (packet_data.canRead())
	{
		packet.payload.gsrFrame.gsrValue= packet_data.readShort();
		m_sensorListener->notifySensorDataReceived(&packet);
	}
}