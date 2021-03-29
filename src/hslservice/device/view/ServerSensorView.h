#ifndef SERVER_SENSOR_VIEW_H
#define SERVER_SENSOR_VIEW_H

//-- includes -----
#include "ServerDeviceView.h"
#include "HSLClient_CAPI.h"
#include "HSLServiceInterface.h"
#include "CircularBuffer.h"

#include "readerwriterqueue.h" // lockfree queue

#include <array>
#include <mutex>

// -- declarations -----
class ServerSensorView : public ServerDeviceView, public ISensorListener
{
public:
	ServerSensorView(const int device_id);
	virtual ~ServerSensorView();

	bool open(const class DeviceEnumerator *enumerator) override;
	void close() override;

	// Sets which data streams from the device are active
	bool setActiveSensorDataStreams(t_hsl_caps_bitmask data_stream_flags);

	// Gets which data streams from the device are active
	t_hsl_caps_bitmask getActiveSensorDataStreams() const;

	// Sets which filter streams on the ServerSensorView are active
	bool setActiveSensorFilterStreams(t_hrv_filter_bitmask filter_stream_bitmask);

	// Gets which filter streams on the ServerSensorView are active
	t_hrv_filter_bitmask getActiveSensorFilterStreams();

	// Get the sampling rate (in samples/sec) of the given capability type
	bool getCapabilitySamplingRate(HSLSensorCapabilityType cap_type, int& out_sampling_rate);

	// Get the sampling resolution (in bits) of the given capability type
	bool getCapabilityBitResolution(HSLSensorCapabilityType cap_type, int& out_resolution);

	// Update Pose Filter using update packets from the tracker and IMU threads
	void processDevicePacketQueues();

	IDeviceInterface* getDevice() const override {return m_device;}

    const std::string getFriendlyName() const;

	// Returns the full usb device path for the sensor
	const std::string getDevicePath() const;

	// Returns the "sensor_" + serial number for the sensor
	std::string getConfigIdentifier() const;

    // Fill out the HSLDeviceInformation struct
    bool fetchDeviceInformation(HSLDeviceInformation* out_device_info) const;

	// Get the current heart rate value in beats per minute. All sensors support this feature.
	uint16_t getHeartRateBPM() const;

	// Accessors for the various history buffers for heart data and filter streams
	inline CircularBuffer<HSLHeartRateFrame> *getHeartRateBuffer() const { return heartRateBuffer; }
	inline CircularBuffer<HSLHeartECGFrame> *getHeartECGBuffer() const { return heartECGBuffer; }
	inline CircularBuffer<HSLHeartPPGFrame> *getHeartPPGBuffer() const { return heartPPGBuffer; }
	inline CircularBuffer<HSLHeartPPIFrame> *getHeartPPIBuffer() const { return heartPPIBuffer; }
	inline CircularBuffer<HSLAccelerometerFrame> *getHeartAccBuffer() const { return heartAccBuffer; }
	inline CircularBuffer<HSLElectrodermalActivityFrame>* getSkinEDABuffer() const { return skinEDABuffer; }
	inline CircularBuffer<HSLHeartVariabilityFrame> *getHeartHrvBuffer(HSLHeartRateVariabityFilterType filter) const
	{
		return hrvFilters[filter].hrvBuffer;
	}

	// Incoming device data callbacks
	void notifySensorDataReceived(const ISensorListener::SensorPacket *sensorPacket) override;

protected:
	bool allocateDeviceInterface(const class DeviceEnumerator *enumerator) override;
	void freeDeviceInterface() override;

	void adjustSampleBufferCapacities();
	void recomputeHeartRateBPM();

private:
	// Device State
	ISensorInterface *m_device;
	std::string m_friendlyName;
	std::string m_devicePath;
	 
	// Filter State (IMU Thread)
	std::chrono::time_point<std::chrono::high_resolution_clock> m_lastSensorDataTimestamp;
	bool m_bIsLastSensorDataTimestampValid;

	// Filter State (Shared)
	mutable std::mutex m_sensorPacketWriteMutex;
	moodycamel::ReaderWriterQueue<ISensorListener::SensorPacket> m_sensorPacketQueue;

	// Filter State (Main Thread)
	CircularBuffer<HSLHeartRateFrame> *heartRateBuffer;
	CircularBuffer<HSLHeartECGFrame> *heartECGBuffer;
	CircularBuffer<HSLHeartPPGFrame> *heartPPGBuffer;
	CircularBuffer<HSLHeartPPIFrame> *heartPPIBuffer;
	CircularBuffer<HSLAccelerometerFrame> *heartAccBuffer;
	CircularBuffer<HSLElectrodermalActivityFrame>* skinEDABuffer;

	struct HRVFilterState
	{
		CircularBuffer<HSLHeartVariabilityFrame> *hrvBuffer;
	};
	std::array<HRVFilterState, HRVFilter_COUNT> hrvFilters;
	t_hrv_filter_bitmask m_activeFilterBitmask;

	std::chrono::time_point<std::chrono::high_resolution_clock> m_lastValidHRTimestamp;
	uint16_t m_lastValidHR;
};

#endif // SERVER_SENSOR_VIEW_H
