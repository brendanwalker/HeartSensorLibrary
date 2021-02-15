#ifndef ADAFRUIT_SENSOR_H
#define ADAFRUIT_SENSOR_H

#include "HSLClient_CAPI.h"
#include "BluetoothLEApiInterface.h"
#include "DeviceEnumerator.h"
#include "DeviceInterface.h"
#include "MathUtility.h"
#include "AdafruitSensorConfig.h"

#include <string>
#include <array>
#include <deque>
#include <chrono>

struct AdafruitBluetoothInfo
{
	t_bluetoothle_device_handle deviceHandle;
	BLEGattProfile* gattProfile;
	std::string bluetoothAddress;      // The bluetooth address of the sensor
	HSLDeviceInformation deviceInfo;

	void reset();
};

class AdafruitSensor : public ISensorInterface {
public:
	static const char* k_szFriendlyName;
	static IDeviceInterface* AdafruitSensorFactory();

	AdafruitSensor();
	virtual ~AdafruitSensor();

	// AdafruitSensor
	bool open(); // Opens the first BluetoothLE device for the sensor

	// -- IDeviceInterface
	virtual bool matchesDeviceEnumerator(const DeviceEnumerator* enumerator) const override;
	virtual bool open(const DeviceEnumerator* enumerator) override;
	virtual bool getIsOpen() const override;
	virtual const std::string getFriendlyName() const override;
	virtual void close() override;

	// -- ISensorInterface
	virtual bool setActiveSensorDataStreams(t_hsl_stream_bitmask data_stream_flags) override;
	virtual t_hsl_stream_bitmask getActiveSensorDataStreams() const override;
	virtual const std::string getDevicePath() const override;
	virtual const std::string getBluetoothAddress() const override;
	virtual t_hsl_stream_bitmask getSensorCapabilities() const override;
	virtual bool getDeviceInformation(HSLDeviceInformation* out_device_info) const override;
	virtual int getCapabilitySampleRate(HSLSensorDataStreamFlags flag) const;
	virtual void getAvailableCapabilitySampleRates(HSLSensorDataStreamFlags flag, const int** out_rates, int* out_rate_count) const;
	virtual void setCapabilitySampleRate(HSLSensorDataStreamFlags flag, int sample_rate);
	virtual float getSampleHistoryDuration() const override;
	virtual void setSampleHistoryDuration(float duration) override;
	virtual int getHeartRateVariabliyHistorySize() const override;
	virtual void setHeartRateVariabliyHistorySize(int sample_count) override;

	// -- Getters
	inline const AdafruitSensorConfig* getConfig() const
	{
		return &m_config;
	}

	// -- Setters
	void setConfig(const AdafruitSensorConfig* config);
	void setSensorListener(ISensorListener* listener) override;

private:
	bool fetchDeviceInformation();

	// Constant while a sensor is open
	AdafruitSensorConfig m_config;
	AdafruitBluetoothInfo m_bluetoothLEDetails;

	// Bluetooth LE Packet Processing
	class AdafruitPacketProcessor* m_packetProcessor;
	ISensorListener* m_sensorListener;
};
#endif // ADAFRUIT_SENSOR_H
