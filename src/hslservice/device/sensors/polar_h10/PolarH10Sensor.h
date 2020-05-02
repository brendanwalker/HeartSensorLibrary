#ifndef POLAR_H10_SENSOR_H
#define POLAR_H10_SENSOR_H

#include "HSLConfig.h"
#include "HSLClient_CAPI.h"
#include "BluetoothLEApiInterface.h"
#include "DeviceEnumerator.h"
#include "DeviceInterface.h"
#include "MathUtility.h"

#include <string>
#include <array>
#include <deque>
#include <chrono>

struct PolarH10BluetoothLEDetails 
{
	std::string friendlyName;
    std::string devicePath;
	t_bluetoothle_device_handle deviceHandle;
	BLEGattProfile *gattProfile;
    std::string bluetoothAddress;      // The bluetooth address of the sensor
	HSLDeviceInformation deviceInfo;
	std::string bodyLocation;

	void reset();
};

class PolarH10SensorConfig : public HSLConfig
{
public:
    static const int CONFIG_VERSION;

	PolarH10SensorConfig(const std::string &fnamebase = "PolarH10SensorConfig");

    virtual const configuru::Config writeToJSON();
    virtual void readFromJSON(const configuru::Config &pt);

	static int sanitizeSampleRate(int test_sample_rate, const int *sample_rate_array);

    bool isValid;
    long version;

	float sampleHistoryDuration;
	int hrvHistorySize;

	int accSampleRate;
	int ecgSampleRate;
	int hrSampleRate;
};

class PolarH10Sensor : public ISensorInterface {
public:
	static const char *k_szFriendlyName;
	static IDeviceInterface *PolarH10SensorFactory();

	PolarH10Sensor();
	virtual ~PolarH10Sensor();

	// PSMoveController
	bool open(); // Opens the first BluetoothLE device for the sensor

	// -- IDeviceInterface
	virtual bool matchesDeviceEnumerator(const DeviceEnumerator *enumerator) const override;
	virtual bool open(const DeviceEnumerator *enumerator) override;
	virtual bool getIsOpen() const override;
	virtual const std::string &getFriendlyName() const override;
	virtual void close() override;

	// -- ISensorInterface
	virtual bool setActiveSensorDataStreams(t_hsl_stream_bitmask data_stream_flags) override;
    virtual const std::string &getDevicePath() const override;
    virtual const std::string &getBluetoothAddress() const override;
	virtual t_hsl_stream_bitmask getSensorCapabilities() const override;
	virtual bool getDeviceInformation(HSLDeviceInformation *out_device_info) const override;
	virtual int getCapabilitySampleRate(HSLSensorDataStreamFlags flag) const;
	virtual void getAvailableCapabilitySampleRates(HSLSensorDataStreamFlags flag, const int **out_rates, int *out_rate_count) const;
	virtual void setCapabilitySampleRate(HSLSensorDataStreamFlags flag, int sample_rate);
	virtual float getSampleHistoryDuration() const override;
	virtual void setSampleHistoryDuration(float duration) override;
	virtual int getHeartRateVariabliyHistorySize() const override;
	virtual void setHeartRateVariabliyHistorySize(int sample_count) override;

    // -- Getters
    inline const PolarH10SensorConfig *getConfig() const
    { return &m_config; }    
    
    // -- Setters
	void setConfig(const PolarH10SensorConfig *config);
	void setSensorListener(ISensorListener *listener) override;

private:       
	bool fetchBodySensorLocation();
	bool fetchDeviceInformation();

	// Constant while a sensor is open
    PolarH10SensorConfig m_config;
    PolarH10BluetoothLEDetails m_bluetoothLEDetails;

    // HID Packet Processing
	class PolarH10PacketProcessor* m_packetProcessor;
	ISensorListener* m_sensorListener;
};
#endif // POLAR_H10_SENSOR_H