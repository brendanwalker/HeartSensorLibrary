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
	void setActiveSensorDataStreams(t_hsl_stream_bitmask data_stream_flags);
	t_hsl_stream_bitmask getActiveSensorDataStreams() const;

	bool getStreamCapabilities(t_hsl_stream_bitmask& outStreamCapabilitiesBitmask);

protected:

	void fetchStreamCapabilities();

	void startGalvanicSkinResponseStream();
	void stopGalvanicSkinResponseStream();

	void OnReceivedGSRDataPacket(BluetoothGattHandle attributeHandle, uint8_t* data, size_t data_size);

	// GATT Profile cached vars
	class BLEGattProfile* m_GATT_Profile;
	const class BLEGattService* m_GSR_Service;
	class BLEGattCharacteristic* m_GSRMeasurement_Characteristic;
	class BLEGattCharacteristicValue* m_GSRMeasurement_CharacteristicValue;
	class BLEGattCharacteristic* m_GSRPeriod_Characteristic;
	class BLEGattCharacteristicValue* m_GSRPeriod_CharacteristicValue;

	// Stream State
	t_bluetoothle_device_handle m_deviceHandle;
	ISensorListener* m_sensorListener;
	AdafruitSensorConfig m_config;
	t_hsl_stream_bitmask m_streamCapabilitiesBitmask;
	t_hsl_stream_bitmask m_streamListenerBitmask;
	bool m_bIsRunning;
	t_hsl_stream_bitmask m_streamActiveBitmask;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_gsrStreamStartTimestamp;
	bool m_bIsGSRNotificationEnabled;
	BluetoothEventHandle m_GSRCallbackHandle;
};

#endif // ADAFRUIT_PACKET_PROCESSOR_H