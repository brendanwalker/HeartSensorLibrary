#ifndef POLAR_PACKET_PROCESSOR_H
#define POLAR_PACKET_PROCESSOR_H

//-- includes -----
#include "BluetoothLEApiInterface.h"
#include "PolarSensorConfig.h"

#include <chrono>

class PolarPacketProcessor
{
public:
	PolarPacketProcessor(const PolarSensorConfig& config);

	bool start(t_bluetoothle_device_handle deviceHandle, class BLEGattProfile* gattProfile, class ISensorListener* sensorListener);
	void stop();

	void setConfig(const PolarSensorConfig& config);
	void setActiveSensorDataStreams(t_hsl_caps_bitmask data_stream_flags);
	t_hsl_caps_bitmask getActiveSensorDataStreams() const;

	bool getStreamCapabilities(t_hsl_caps_bitmask& outStreamCapabilitiesBitmask);

protected:

	void setPMDControlPointIndicationEnabled(bool bIsEnabled);
	void setPMDDataMTUNotificationEnabled(bool bIsEnabled);
	void fetchStreamCapabilities();

	bool startAccStream(const PolarSensorConfig& config);
	bool stopAccStream();
	bool startECGStream(const PolarSensorConfig& config);
	bool stopECGStream();
	bool startPPGStream(const PolarSensorConfig& config);
	bool stopPPGStream();
	bool startPPIStream(const PolarSensorConfig& config);
	bool stopPPIStream();
	void startHeartRateStream();
	void stopHeartRateStream();

	void OnReceivedPMDDataMTUPacket(BluetoothGattHandle attributeHandle, uint8_t* data, size_t data_size);
	void OnReceivedHRDataPacket(BluetoothGattHandle attributeHandle, uint8_t* data, size_t data_size);

	// GATT Profile cached vars
	class BLEGattProfile* m_GATT_Profile;
	const class BLEGattService* m_PMD_Service;
	class BLEGattCharacteristic* m_PMDCtrlPoint_Characteristic;
	class BLEGattCharacteristicValue* m_PMDCtrlPoint_CharacteristicValue;
	class BLEGattDescriptor* m_PMDCtrlPoint_Descriptor;
	class BLEGattDescriptorValue* m_PMDCtrlPoint_DescriptorValue;
	class BLEGattCharacteristic* m_PMDDataMTU_Characteristic;
	class BLEGattDescriptor* m_PMDDataMTU_Descriptor;
	class BLEGattDescriptorValue* m_PMDDataMTU_DescriptorValue;
	const class BLEGattService* m_HeartRate_Service;
	class BLEGattCharacteristic* m_HeartRateMeasurement_Characteristic;
	class BLEGattDescriptor* m_HeartRateMeasurement_Descriptor;
	class BLEGattDescriptorValue* m_HeartRateMeasurement_DescriptorValue;

	// Stream State
	t_bluetoothle_device_handle m_deviceHandle;
	ISensorListener* m_sensorListener;
	PolarSensorConfig m_config;
	t_hsl_caps_bitmask m_streamCapabilitiesBitmask;
	t_hsl_caps_bitmask m_streamListenerBitmask;
	bool m_bIsRunning;
	t_hsl_caps_bitmask m_streamActiveBitmask;
	uint64_t m_accStreamStartTimestamp;
	uint64_t m_ecgStreamStartTimestamp;
	uint64_t m_ppgStreamStartTimestamp;
	uint64_t m_ppiStreamStartTimestamp;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_hrStreamStartTimestamp;
	bool m_bIsPMDControlPointIndicationEnabled;
	bool m_bIsPMDDataMTUNotificationEnabled;
	bool m_bIsHeartRateNotificationEnabled;
	BluetoothEventHandle m_PMDDataMTUCallbackHandle;
	BluetoothEventHandle m_HeartRateCallbackHandle;
};

#endif // POLAR_PACKET_PROCESSOR_H