//-- includes -----
#include "ServerSensorView.h"

#include "AtomicPrimitives.h"
#include "SensorManager.h"
#include "DeviceManager.h"
#include "Logger.h"
#include "ServiceRequestHandler.h"
#include "MathUtility.h"
#include "Utility.h"

//-- typedefs ----
using t_high_resolution_timepoint= std::chrono::time_point<std::chrono::high_resolution_clock>;
using t_high_resolution_duration= t_high_resolution_timepoint::duration;

//-- constants -----
static const float k_min_time_delta_seconds = 1 / 2500.f;
static const float k_max_time_delta_seconds = 1 / 30.f;

//-- public implementation -----
ServerSensorView::ServerSensorView(const int device_id)
	: ServerDeviceView(device_id)
	, m_device(nullptr)
	, m_sensorPacketQueue(1000)
	, m_activeFilterBitmask(0)
	, m_bIsLastSensorDataTimestampValid(false)
	, heartRateBuffer(new CircularBuffer<HSLHeartRateFrame>(10))
	, heartECGBuffer(new CircularBuffer<HSLHeartECGFrame>(10))
	, heartPPGBuffer(new CircularBuffer<HSLHeartPPGFrame>(10))
	, heartPPIBuffer(new CircularBuffer<HSLHeartPPIFrame>(10))
	, heartAccBuffer(new CircularBuffer<HSLAccelerometerFrame>(10))
	, m_lastValidHRTimestamp(std::chrono::high_resolution_clock::now())
	, m_lastValidHR(0)
{
	for (int filter_index = 0; filter_index < HRVFilter_COUNT; ++filter_index)
	{
		hrvFilters[filter_index].hrvBuffer = new CircularBuffer<HSLHeartVariabilityFrame>(10);
	}
}

ServerSensorView::~ServerSensorView()
{
	delete heartRateBuffer;
	delete heartECGBuffer;
	delete heartPPGBuffer;
	delete heartPPIBuffer;
	delete heartAccBuffer;

	for (int filter_index = 0; filter_index < HRVFilter_COUNT; ++filter_index)
	{
		delete hrvFilters[filter_index].hrvBuffer;
	}
}

bool ServerSensorView::allocateDeviceInterface(
		const class DeviceEnumerator *enumerator)
{
	std::string friendlyName= enumerator->getFriendlyName();
	DeviceManager::DeviceFactoryFunction factoryFunc= DeviceManager::getInstance()->getFactoryFunction(friendlyName);

	if (factoryFunc)
	{
		IDeviceInterface *deviceInterface= factoryFunc();

		m_device = static_cast<ISensorInterface *>(deviceInterface);
		m_device->setSensorListener(this);
	}

	return m_device != nullptr;
}

void ServerSensorView::freeDeviceInterface()
{

	if (m_device != nullptr)
	{
		delete m_device;
		m_device = nullptr;
	}
}

inline int compute_samples_needed(int sample_rate, float sample_history_duration)
{
	return std::max((int)ceilf((float)sample_rate*sample_history_duration), 1);
}

bool ServerSensorView::open(const class DeviceEnumerator *enumerator)
{
	// Attempt to open the sensor
	bool bSuccess = ServerDeviceView::open(enumerator);

	if (bSuccess)
	{
		m_friendlyName= m_device->getFriendlyName();

		// Resize buffers to match requested sample frequency and history duration
		adjustSampleBufferCapacities();
	}

	return bSuccess;
}

void ServerSensorView::adjustSampleBufferCapacities()
{
	if (m_device == nullptr)
		return;

	// Allocate data buffers based on device capabilities
	float sample_history_duration = m_device->getSampleHistoryDuration();
	t_hsl_stream_bitmask caps_bitmask = m_device->getSensorCapabilities();

	if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_HRData))
	{
		int sample_rate = m_device->getCapabilitySampleRate(HSLStreamFlags_HRData);
		int samples_needed = compute_samples_needed(sample_rate, sample_history_duration);

		heartRateBuffer->setCapacity(samples_needed);
	}

	if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_ECGData))
	{
		int sample_rate = m_device->getCapabilitySampleRate(HSLStreamFlags_ECGData);
		int samples_needed = compute_samples_needed(sample_rate, sample_history_duration);

		heartECGBuffer->setCapacity(samples_needed);
	}

	if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_PPGData))
	{
		int sample_rate = m_device->getCapabilitySampleRate(HSLStreamFlags_PPGData);
		int samples_needed = compute_samples_needed(sample_rate, sample_history_duration);

		heartPPGBuffer->setCapacity(samples_needed);
	}

	if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_PPIData))
	{
		int sample_rate = m_device->getCapabilitySampleRate(HSLStreamFlags_PPIData);
		int samples_needed = compute_samples_needed(sample_rate, sample_history_duration);

		heartPPIBuffer->setCapacity(samples_needed);
	}

	if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_AccData))
	{
		int sample_rate = m_device->getCapabilitySampleRate(HSLStreamFlags_AccData);
		int samples_needed = compute_samples_needed(sample_rate, sample_history_duration);

		heartAccBuffer->setCapacity(samples_needed);
	}

	// We can compute HRV statistics if we either have ECG data or PPI data
	if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_ECGData) ||
		HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_PPIData))
	{
		int hrv_samples_needed = m_device->getHeartRateVariabliyHistorySize();

		for (int filter_index = 0; filter_index < HRVFilter_COUNT; ++filter_index)
		{
			hrvFilters[filter_index].hrvBuffer->setCapacity(hrv_samples_needed);
		}
	}
}

void ServerSensorView::close()
{
	ServerDeviceView::close();
}

bool ServerSensorView::setActiveSensorDataStreams(t_hsl_stream_bitmask data_stream_flags)
{
	if (m_device != nullptr)
	{
		return m_device->setActiveSensorDataStreams(data_stream_flags);
	}

	return false;
}

t_hsl_stream_bitmask ServerSensorView::getActiveSensorDataStreams() const
{
	if (m_device != nullptr)
	{
		return m_device->getActiveSensorDataStreams();
	}

	return false;
}


bool ServerSensorView::setActiveSensorFilterStreams(t_hrv_filter_bitmask filter_stream_bitmask)
{
	if (m_device != nullptr)
	{
		m_activeFilterBitmask = filter_stream_bitmask;

		return true;
	}

	return false;
}

t_hrv_filter_bitmask ServerSensorView::getActiveSensorFilterStreams()
{
	if (m_device != nullptr)
	{
		return m_activeFilterBitmask;
	}

	return 0;
}

void ServerSensorView::notifySensorDataReceived(const ISensorListener::SensorPacket *sensor_packet)
{
	// Compute the time in seconds since the last update
	const t_high_resolution_timepoint now = std::chrono::high_resolution_clock::now();
	t_high_resolution_duration durationSinceLastUpdate= t_high_resolution_duration::zero();

	if (m_bIsLastSensorDataTimestampValid)
	{
		durationSinceLastUpdate = now - m_lastSensorDataTimestamp;
	}
	m_lastSensorDataTimestamp= now;
	m_bIsLastSensorDataTimestampValid= true;

	{
		std::lock_guard<std::mutex> write_lock(m_sensorPacketWriteMutex);

		m_sensorPacketQueue.enqueue(*sensor_packet);
	}
}

// Update Pose Filter using update packets from the tracker and IMU threads
void ServerSensorView::processDevicePacketQueues()
{
	// Drain the packet queues filled by the threads
	ISensorListener::SensorPacket packet;
	while (m_sensorPacketQueue.try_dequeue(packet))
	{
		switch (packet.payloadType)
		{
		case ISensorListener::SensorPacketPayloadType::ACCFrame:
			heartAccBuffer->writeItem(packet.payload.accFrame);
			break;
		case ISensorListener::SensorPacketPayloadType::ECGFrame:
			heartECGBuffer->writeItem(packet.payload.ecgFrame);
			break;
		case ISensorListener::SensorPacketPayloadType::HRFrame:
			heartRateBuffer->writeItem(packet.payload.hrFrame);
			break;
		case ISensorListener::SensorPacketPayloadType::PPGFrame:
			heartPPGBuffer->writeItem(packet.payload.ppgFrame);
			break;
		case ISensorListener::SensorPacketPayloadType::PPIFrame:
			heartPPIBuffer->writeItem(packet.payload.ppiFrame);
			break;
		}
	}

	// Find the latest valid heart rate valid from either the PPI buffer or the HR buffer
	recomputeHeartRateBPM();

	for (int filter_index = 0; filter_index < HRVFilter_COUNT; ++filter_index)
	{
		CircularBuffer<HSLHeartVariabilityFrame> *hrvBuffer= hrvFilters[filter_index].hrvBuffer;

		if (hrvBuffer != nullptr && HSL_BITMASK_GET_FLAG(m_activeFilterBitmask, filter_index))
		{
			// TODO: Update hrvBuffer
			switch (filter_index)
			{				 
			case HRVFilter_SDNN:
				break;
			case HRVFilter_RMSSD:
				break;
			case HRVFilter_SDSD:
				break;
			case HRVFilter_NN50:
				break;
			case HRVFilter_pNN50:
				break;
			case HRVFilter_NN20:
				break;
			case HRVFilter_pNN20:
				break;
			}
		}
	}
}

// Returns the full device path for the sensor
const std::string ServerSensorView::getDevicePath() const
{
	return m_devicePath;
}

const std::string ServerSensorView::getFriendlyName() const
{
    return m_friendlyName;
}

// Returns the "sensor_" + serial number for the sensor
std::string ServerSensorView::getConfigIdentifier() const
{
    std::string	identifier = "";

    if (m_device != nullptr)
    {
        std::string	prefix = "sensor_";

        identifier = prefix + m_device->getBluetoothAddress();
    }

    return identifier;
}

void ServerSensorView::recomputeHeartRateBPM()
{
	uint16_t newHeartRate = 0;

	if (m_device != nullptr)
	{
		const t_hsl_stream_bitmask data_stream_bitmask = m_device->getActiveSensorDataStreams();

		// First try to find the most recent Pulse-to-Pulse-Interval derived HeartRate (only Polar sensors)
		if (!heartPPIBuffer->isEmpty())
		{
			const size_t newestIndex = heartPPIBuffer->getWriteIndex();
			const HSLHeartPPIFrame* PPIFrame = &heartPPIBuffer->getBuffer()[newestIndex];

			for (int sampleIndex = 0; sampleIndex < PPIFrame->ppiSampleCount; ++sampleIndex)
			{
				const HSLHeartPPISample& PPISample = PPIFrame->ppiSamples[sampleIndex];

				if (PPISample.beatsPerMinute > 0)
				{
					newHeartRate = PPISample.beatsPerMinute;
				}
			}
		}

		// Fall back to most recent generic HeartRate packet (all HR sensors)
		if (newHeartRate == 0 && !heartRateBuffer->isEmpty())
		{
			const size_t newestIndex = heartRateBuffer->getWriteIndex();
			const HSLHeartRateFrame* HRFrame = &heartRateBuffer->getBuffer()[newestIndex];

			if (HRFrame->beatsPerMinute > 0)
			{
				newHeartRate = HRFrame->beatsPerMinute;
			}
		}
	}

	// Sometimes we get bubbles of 0 HR from the PPI data, 
	// so we try to paper over these by holding onto the most recent non-zero value.
	// If it has bee too long though, we throw out the preserved value.
	if (newHeartRate > 0)
	{
		m_lastValidHRTimestamp= std::chrono::high_resolution_clock::now();
		m_lastValidHR= newHeartRate;
	}
	else if (m_lastValidHR > 0)
	{
		const t_high_resolution_timepoint now = std::chrono::high_resolution_clock::now();
		const auto age_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastValidHRTimestamp);
		const int timeout= DeviceManager::getInstance()->getSensorManager()->getConfig().heartRateTimeoutMilliSeconds;

		if (age_milliseconds.count() > timeout)
		{
			m_lastValidHR= 0;
		}
	}
}

uint16_t ServerSensorView::getHeartRateBPM() const
{
	return m_lastValidHR;
}

// Fill out the HSLDeviceInformation info struct
bool ServerSensorView::fetchDeviceInformation(HSLDeviceInformation* out_device_info) const
{
    if (m_device != nullptr)
    {
		return m_device->getDeviceInformation(out_device_info);
    }

	return false;
}
