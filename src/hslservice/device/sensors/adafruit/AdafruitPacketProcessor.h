#ifndef ADAFRUIT_PACKET_PROCESSOR_H
#define ADAFRUIT_PACKET_PROCESSOR_H

//-- includes -----
#include "BluetoothLEApiInterface.h"
#include "AdafruitSensorConfig.h"

#include <chrono>

class AdafruitPacketProcessor
{
public:
	AdafruitPacketProcessor(const AdafruitSensorConfig& config);

	bool start(t_bluetoothle_device_handle deviceHandle, class BLEGattProfile* gattProfile, class ISensorListener* sensorListener);
	void stop();

	void setConfig(const AdafruitSensorConfig& config);
	void setActiveSensorDataStreams(t_hsl_caps_bitmask data_stream_flags);
	t_hsl_caps_bitmask getActiveSensorDataStreams() const;

	bool getStreamCapabilities(t_hsl_caps_bitmask& outStreamCapabilitiesBitmask);

protected:

	void fetchStreamCapabilities();

	void startElectrodermalActivityStream();
	void stopElectrodermalActivityStream();

	void OnReceivedEDADataPacket(BluetoothGattHandle attributeHandle, uint8_t* data, size_t data_size);

	// GATT Profile cached vars
	class BLEGattProfile* m_GATT_Profile;
	const class BLEGattService* m_EDA_Service;
	class BLEGattCharacteristic* m_EDAMeasurement_Characteristic;
	class BLEGattCharacteristicValue* m_EDAMeasurement_CharacteristicValue;
	class BLEGattDescriptor* m_EDAMeasurement_Descriptor;
	class BLEGattDescriptorValue* m_EDAMeasurement_DescriptorValue;
	class BLEGattCharacteristic* m_EDAPeriod_Characteristic;
	class BLEGattCharacteristicValue* m_EDAPeriod_CharacteristicValue;

	// Stream State
	t_bluetoothle_device_handle m_deviceHandle;
	ISensorListener* m_sensorListener;
	AdafruitSensorConfig m_config;
	t_hsl_caps_bitmask m_streamCapabilitiesBitmask;
	t_hsl_caps_bitmask m_streamListenerBitmask;
	bool m_bIsRunning;
	t_hsl_caps_bitmask m_streamActiveBitmask;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_edaStreamStartTimestamp;
	bool m_bIsEDANotificationEnabled;
	BluetoothEventHandle m_EDACallbackHandle;
};

#endif // ADAFRUIT_PACKET_PROCESSOR_H