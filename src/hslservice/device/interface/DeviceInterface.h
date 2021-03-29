#ifndef DEVICE_INTERFACE_H
#define DEVICE_INTERFACE_H

// -- includes -----
#include <string>
#include <tuple>
#include <vector>

#include "HSLClient_CAPI.h"

// -- definitions -----

/// Interface base class for any device interface. Further defined in specific device abstractions.
class IDeviceInterface
{
public:
	 
	virtual ~IDeviceInterface() {};

	// Return true if device path matches
	virtual bool matchesDeviceEnumerator(const class DeviceEnumerator *enumerator) const = 0;

    // Returns the friendly name of the device
    virtual const std::string getFriendlyName() const = 0;
	
	// Opens the HID device for the device at the given enumerator
	virtual bool open(const class DeviceEnumerator *enumerator) = 0;
	
	// Returns true if hidapi opened successfully
	virtual bool getIsOpen() const  = 0;
		 
	// Closes the HID device for the device
	virtual void close() = 0;			
};

/// Interface class for HMD events. Implemented HMD Server View
class ISensorListener
{
public:
	enum class SensorPacketPayloadType : int
	{
		HRFrame,
		ECGFrame,
		PPGFrame,
		PPIFrame,
		ACCFrame,
		EDAFrame
	};

	struct SensorPacket
	{
		union {
			HSLHeartRateFrame hrFrame;
			HSLHeartECGFrame ecgFrame;
			HSLHeartPPGFrame ppgFrame;
			HSLHeartPPIFrame ppiFrame;
			HSLAccelerometerFrame accFrame;
			HSLElectrodermalActivityFrame edaFrame;
		} payload;
		SensorPacketPayloadType payloadType;
	};

	// Called when new sensor state has been read from the sensor
	virtual void notifySensorDataReceived(const SensorPacket *sensor_state) = 0;
};

/// Abstract class for sensor interface. 
class ISensorInterface : public IDeviceInterface
{
public:
	// -- Mutators
	// Set the active sensor data streams
	virtual bool setActiveSensorDataStreams(t_hsl_caps_bitmask data_stream_flags) = 0;

	// Assign an HMD listener to send HMD events to
	virtual void setSensorListener(ISensorListener *listener) = 0;

	// -- Getters
	// Returns the full device path for the sensor
	virtual const std::string getDevicePath() const = 0;

	// Returns the serial number for the sensor
	virtual const std::string getBluetoothAddress() const = 0;

	// Returns a bitmask of flags from HSLSensorDataStreamFlags
	virtual t_hsl_caps_bitmask getSensorCapabilities() const = 0;

	// Returns a bitmask of flags from HSLSensorDataStreamFlags
	virtual t_hsl_caps_bitmask getActiveSensorDataStreams() const = 0;

	// Fills in device info struct
	virtual bool getDeviceInformation(HSLDeviceInformation *out_device_info) const = 0;

	// Get the sampling rate (in samples/sec) of the given capability type
	virtual bool getCapabilitySamplingRate(HSLSensorCapabilityType cap_type, int& out_sampling_rate) const = 0;

	// Get the sampling resolution (in bits) of the given capability type
	virtual bool getCapabilityBitResolution(HSLSensorCapabilityType cap_type, int& out_resolution) const = 0;

	// Returns all possible sample rate of the given capability
	virtual void getAvailableCapabilitySampleRates(HSLSensorCapabilityType flag, const int **out_rates, int *out_rate_count) const = 0;

	// Set the sample rate for the given capability
	virtual void setCapabilitySampleRate(HSLSensorCapabilityType flag, int sample_rate) = 0;

	// Returns the sample recording history time (in seconds)
	virtual float getSampleHistoryDuration() const = 0;

	// Sets the sample recording history time (in seconds)
	virtual void setSampleHistoryDuration(float duration) = 0;

	// Returns the HRV recording history size (in samples)
	virtual int getHeartRateVariabliyHistorySize() const = 0;

	// Sets the HRV recording history time (in samples)
	virtual void setHeartRateVariabliyHistorySize(int sample_count) = 0;
};

#endif // DEVICE_INTERFACE_H
