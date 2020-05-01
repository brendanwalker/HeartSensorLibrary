#ifndef SERVER_SENSOR_VIEW_H
#define SERVER_SENSOR_VIEW_H

//-- includes -----
#include "ServerDeviceView.h"
#include "HSLClient_CAPI.h"
#include "HSLServiceInterface.h"
#include "CircularBuffer.h"

#include "readerwriterqueue.h" // lockfree queue

#include <array>

// -- declarations -----
class ServerSensorView : public ServerDeviceView, public ISensorListener
{
public:
	ServerSensorView(const int device_id);
	virtual ~ServerSensorView();

	bool open(const class DeviceEnumerator *enumerator) override;
	void close() override;

	// Sets which data streams from the device are active
	// and which filter streams on the ServerSensorView are active
	bool setActiveSensorDataStreams(
		t_hsl_stream_bitmask data_stream_flags,
		t_hrv_filter_bitmask filter_stream_bitmask);

	// Update Pose Filter using update packets from the tracker and IMU threads
	void processDevicePacketQueues();

	IDeviceInterface* getDevice() const override {return m_device;}

    const std::string &getFriendlyName() const;

	// Returns the full usb device path for the sensor
	const std::string &getDevicePath() const;

	// Returns the "sensor_" + serial number for the sensor
	std::string getConfigIdentifier() const;

    // Fill out the HSLSensor info struct
    void fetchSensorInfo(HSLSensor *outSensorInfo) const;

	inline CircularBuffer<HSLHeartRateFrame> *getHeartRateBuffer() const { return heartRateBuffer; }
	inline CircularBuffer<HSLHeartECGFrame> *getHeartECGBuffer() const { return heartECGBuffer; }
	inline CircularBuffer<HSLHeartPPGFrame> *getHeartPPGBuffer() const { return heartPPGBuffer; }
	inline CircularBuffer<HSLHeartPPIFrame> *getHeartPPIBuffer() const { return heartPPIBuffer; }
	inline CircularBuffer<HSLAccelerometerFrame> *getHeartAccBuffer() const { return heartAccBuffer; }
	inline CircularBuffer<HSLHeartVariabilityFrame> *getHeartHrvBuffer(HSLHeartRateVariabityFilterType filter) const
	{
		return hrvFilters[filter].hrvBuffer;
	}

	// Incoming device data callbacks
	void notifySensorDataReceived(const ISensorListener::SensorPacket *sensorPacket) override;

protected:
	bool allocate_device_interface(const class DeviceEnumerator *enumerator) override;
	void free_device_interface() override;

private:
	// Device State
	ISensorInterface *m_device;
	std::string m_friendlyName;
	std::string m_devicePath;
	 
	// Filter State (IMU Thread)
	std::chrono::time_point<std::chrono::high_resolution_clock> m_lastSensorDataTimestamp;
	bool m_bIsLastSensorDataTimestampValid;

	// Filter State (Shared)
	moodycamel::ReaderWriterQueue<ISensorListener::SensorPacket> m_sensorPacketQueue;

	// Filter State (Main Thread)
	CircularBuffer<HSLHeartRateFrame> *heartRateBuffer;
	CircularBuffer<HSLHeartECGFrame> *heartECGBuffer;
	CircularBuffer<HSLHeartPPGFrame> *heartPPGBuffer;
	CircularBuffer<HSLHeartPPIFrame> *heartPPIBuffer;
	CircularBuffer<HSLAccelerometerFrame> *heartAccBuffer;

	struct HRVFilterState
	{
		CircularBuffer<HSLHeartVariabilityFrame> *hrvBuffer;
	};
	std::array<HRVFilterState, HRVFilter_COUNT> hrvFilters;
	t_hrv_filter_bitmask m_activeFilterBitmask;

	std::chrono::time_point<std::chrono::high_resolution_clock> m_lastFilterUpdateTimestamp;
	bool m_bIsLastFilterUpdateTimestampValid;
};

#endif // SERVER_SENSOR_VIEW_H
