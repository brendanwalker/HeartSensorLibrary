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
	, m_bIsLastFilterUpdateTimestampValid(false)
	, heartRateBuffer(nullptr)
	, heartECGBuffer(nullptr)
	, heartPPGBuffer(nullptr)
	, heartPPIBuffer(nullptr)
	, heartAccBuffer(nullptr)
{
	for (int hrv_filter_index = 0; hrv_filter_index < HRVFilter_COUNT; ++hrv_filter_index)
	{
		hrvFilters[hrv_filter_index].hrvBuffer= nullptr;
	}
}

ServerSensorView::~ServerSensorView()
{
}

bool ServerSensorView::allocate_device_interface(
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

void ServerSensorView::free_device_interface()
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

		// Allocate data buffers based on device capabilities
		float sample_history_duration = m_device->getSampleHistoryDuration();
		t_hsl_stream_bitmask caps_bitmask= m_device->getSensorCapabilities();

		if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_HRData))
		{
			int sample_rate = m_device->getCapabilitySampleRate(HSLStreamFlags_HRData);
			int samples_needed = compute_samples_needed(sample_rate, sample_history_duration);

			heartRateBuffer = new CircularBuffer<HSLHeartRateFrame>(samples_needed);
		}

		if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_ECGData))
		{
			int sample_rate = m_device->getCapabilitySampleRate(HSLStreamFlags_ECGData);
			int samples_needed = compute_samples_needed(sample_rate, sample_history_duration);

			heartECGBuffer = new CircularBuffer<HSLHeartECGFrame>(samples_needed);
		}

		if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_PPGData))
		{
			int sample_rate = m_device->getCapabilitySampleRate(HSLStreamFlags_PPGData);
			int samples_needed = compute_samples_needed(sample_rate, sample_history_duration);

			heartPPGBuffer = new CircularBuffer<HSLHeartPPGFrame>(samples_needed);
		}

		if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_PPIData))
		{
			int sample_rate = m_device->getCapabilitySampleRate(HSLStreamFlags_PPIData);
			int samples_needed = compute_samples_needed(sample_rate, sample_history_duration);

			heartPPIBuffer = new CircularBuffer<HSLHeartPPIFrame>(samples_needed);
		}

		if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_AccData))
		{
			int sample_rate = m_device->getCapabilitySampleRate(HSLStreamFlags_AccData);
			int samples_needed = compute_samples_needed(sample_rate, sample_history_duration);

			heartAccBuffer = new CircularBuffer<HSLAccelerometerFrame>(samples_needed);
		}

		// We can compute HRV statistics if we either have ECG data or PPI data
		if (HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_ECGData) ||
			HSL_BITMASK_GET_FLAG(caps_bitmask, HSLStreamFlags_PPIData))
		{
			int hrv_samples_needed = m_device->getHeartRateVariabliyHistorySize();

			for (int filter_index = 0; filter_index < HRVFilter_COUNT; ++filter_index)
			{
				hrvFilters[filter_index].hrvBuffer = new CircularBuffer<HSLHeartVariabilityFrame>(hrv_samples_needed);
			}
		}
	}

	return bSuccess;
}

void ServerSensorView::close()
{
	if (heartRateBuffer != nullptr)
	{
		delete heartRateBuffer;
		heartRateBuffer = nullptr;
	}

	if (heartECGBuffer != nullptr)
	{
		delete heartECGBuffer;
		heartECGBuffer = nullptr;
	}

	if (heartPPGBuffer != nullptr)
	{
		delete heartPPGBuffer;
		heartPPGBuffer = nullptr;
	}

	if (heartPPIBuffer != nullptr)
	{
		delete heartPPIBuffer;
		heartPPIBuffer = nullptr;
	}

	if (heartAccBuffer != nullptr)
	{
		delete heartAccBuffer;
		heartAccBuffer = nullptr;
	}

	for (int filter_index = 0; filter_index < HRVFilter_COUNT; ++filter_index)
	{
		if (hrvFilters[filter_index].hrvBuffer != nullptr)
		{
			delete hrvFilters[filter_index].hrvBuffer;
			hrvFilters[filter_index].hrvBuffer = nullptr;
		}
	}

	ServerDeviceView::close();
}

bool ServerSensorView::setActiveSensorDataStreams(
	t_hsl_stream_bitmask data_stream_flags,
	t_hrv_filter_bitmask filter_stream_bitmask)
{
	if (m_device != nullptr)
	{
		if (m_device->setActiveSensorDataStreams(data_stream_flags))
		{
			m_activeFilterBitmask = filter_stream_bitmask;

			return true;
		}
	}

	return false;
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

	m_sensorPacketQueue.enqueue(*sensor_packet);
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
			heartAccBuffer->pushHead(packet.payload.accFrame);
			break;
		case ISensorListener::SensorPacketPayloadType::ECGFrame:
			heartECGBuffer->pushHead(packet.payload.ecgFrame);
			break;
		case ISensorListener::SensorPacketPayloadType::HRFrame:
			heartRateBuffer->pushHead(packet.payload.hrFrame);
			break;
		case ISensorListener::SensorPacketPayloadType::PPGFrame:
			heartPPGBuffer->pushHead(packet.payload.ppgFrame);
			break;
		case ISensorListener::SensorPacketPayloadType::PPIFrame:
			heartPPIBuffer->pushHead(packet.payload.ppiFrame);
			break;
		}
	}

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
const std::string &ServerSensorView::getDevicePath() const
{
	return m_devicePath;
}

const std::string &ServerSensorView::getFriendlyName() const
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

// Fill out the HSLSensor info struct
void ServerSensorView::fetchSensorInfo(HSLSensor *outSensorInfo) const
{
    if (m_device != nullptr)
    {
		outSensorInfo->sensorID = this->getDeviceID();

		outSensorInfo->capabilities = m_device->getSensorCapabilities();
		Utility::copyCString(
			m_friendlyName.c_str(), outSensorInfo->deviceFriendlyName,
			sizeof(outSensorInfo->deviceFriendlyName));
		Utility::copyCString(
			m_devicePath.c_str(), outSensorInfo->devicePath,
			sizeof(outSensorInfo->devicePath));

		m_device->getDeviceInformation(&outSensorInfo->deviceInformation);

		//TODO?
		// Dynamic Data
		//t_hsl_stream_bitmask	active_streams;
		//t_hrv_filter_bitmask	active_filters;
		//int						outputSequenceNum;
		//int						inputSequenceNum;
		//long long				dataFrameLastReceivedTime;
		//float					dataFrameAverageFPS;
		//int						listenerCount;
		//bool					isValid;
		//bool					isConnected;
    }
}
