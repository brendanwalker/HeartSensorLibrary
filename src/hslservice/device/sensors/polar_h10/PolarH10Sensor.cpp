//-- includes -----
#include "AtomicPrimitives.h"
#include "BluetoothLEApiInterface.h"
#include "BluetoothLEDeviceManager.h"
#include "BluetoothLEServiceIDs.h"
#include "PolarH10Sensor.h"
#include "SensorDeviceEnumerator.h"
#include "SensorBluetoothLEDeviceEnumerator.h"
#include "StackBuffer.h"
#include "Logger.h"
#include "Utility.h"
#include "WorkerThread.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <chrono>
#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <thread>

#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif
#include <math.h>

const BluetoothUUID k_Service_PMD_UUID("FB005C80-02E7-F387-1CAD-8ACD2D8DF0C8");
const BluetoothUUID k_Characteristic_PMD_ControlPoint_UUID("FB005C81-02E7-F387-1CAD-8ACD2D8DF0C8");
const BluetoothUUID k_Descriptor_PMD_ControlPoint_UUID("2902");
const BluetoothUUID k_Characteristic_PMD_DataMTU_UUID("FB005C82-02E7-F387-1CAD-8ACD2D8DF0C8");
const BluetoothUUID k_Descriptor_PMD_DataMTU_UUID("2902");

// -- private definitions -----
class PolarH10PacketProcessor : public WorkerThread
{
public:
	PolarH10PacketProcessor(const PolarH10SensorConfig &m_config) 
		: WorkerThread("PolarH10SensorProcessor")
        , m_GATT_Profile(nullptr)
		, m_PMD_Service(nullptr)
		, m_PMDCtrlPoint_Characteristic(nullptr)
		, m_PMDCtrlPoint_CharacteristicValue(nullptr)
		, m_PMDCtrlPoint_Descriptor(nullptr)
		, m_PMDCtrlPoint_DescriptorValue(nullptr)
		, m_PMDDataMTU_Characteristic(nullptr)
		, m_PMDDataMTU_Descriptor(nullptr)
		, m_PMDDataMTU_DescriptorValue(nullptr)
		, m_HeartRate_Service(nullptr)
		, m_HeartRateMeasurement_Characteristic(nullptr)
		, m_HeartRateMeasurement_Descriptor(nullptr)
		, m_HeartRateMeasurement_DescriptorValue(nullptr)
		, m_deviceHandle(k_invalid_ble_device_handle)
		, m_sensorListener(nullptr)
		, m_streamCapabilitiesBitmask(0)
		, m_streamListenerBitmask(0)
        , m_accStreamStartTimestamp(0)
		, m_ecgStreamStartTimestamp(0)
        , m_bIsPMDControlPointIndicationEnabled(false)
		, m_bIsPMDDataMTUNotificationEnabled(false)
		, m_bIsHeartRateNotificationEnabled(false)
		, m_PMDDataMTUCallbackHandle(k_invalid_ble_gatt_event_handle)
		, m_HeartRateCallbackHandle(k_invalid_ble_gatt_event_handle)
	{
		setConfig(m_config);
	}

	void setConfig(const PolarH10SensorConfig &m_config)
	{
		m_cfg.storeValue(m_config);
	}

    bool start(t_bluetoothle_device_handle deviceHandle, BLEGattProfile *gattProfile, ISensorListener *sensorListener)
    {
		if (hasThreadStarted())
			return true;

		m_deviceHandle= deviceHandle;
        m_GATT_Profile = gattProfile;
        m_sensorListener = sensorListener;

		// PMD Service
        m_PMD_Service = m_GATT_Profile->findService(k_Service_PMD_UUID);
		if (m_PMD_Service == nullptr)
			return false;

		// PMD Control Point
        m_PMDCtrlPoint_Characteristic = m_PMD_Service->findCharacteristic(k_Characteristic_PMD_ControlPoint_UUID);
		if (m_PMDCtrlPoint_Characteristic == nullptr)
			return false;

		m_PMDCtrlPoint_CharacteristicValue= m_PMDCtrlPoint_Characteristic->getCharacteristicValue();
        if (m_PMDCtrlPoint_CharacteristicValue == nullptr)
            return false;

        m_PMDCtrlPoint_Descriptor = m_PMDCtrlPoint_Characteristic->findDescriptor(k_Descriptor_PMD_ControlPoint_UUID);
		if (m_PMDCtrlPoint_Descriptor == nullptr)
			return false;

        m_PMDCtrlPoint_DescriptorValue = m_PMDCtrlPoint_Descriptor->getDescriptorValue();
		if (m_PMDCtrlPoint_DescriptorValue == nullptr)
			return false;

		// PMD Data MTU
        m_PMDDataMTU_Characteristic = m_PMD_Service->findCharacteristic(k_Characteristic_PMD_DataMTU_UUID);
		if (m_PMDDataMTU_Characteristic == nullptr)
			return false;

        m_PMDDataMTU_Descriptor = m_PMDDataMTU_Characteristic->findDescriptor(k_Descriptor_PMD_DataMTU_UUID);
		if (m_PMDDataMTU_Descriptor == nullptr)
			return false;

        m_PMDDataMTU_DescriptorValue = m_PMDDataMTU_Descriptor->getDescriptorValue();
		if (m_PMDDataMTU_DescriptorValue == nullptr)
			return false;

		// Heart Rate Service
        m_HeartRate_Service = m_GATT_Profile->findService(*k_Service_HeartRate_UUID);
        if (m_HeartRate_Service == nullptr)
            return false;

        m_HeartRateMeasurement_Characteristic = m_PMD_Service->findCharacteristic(*k_Characteristic_HeartRateMeasurement_UUID);
        if (m_HeartRateMeasurement_Characteristic == nullptr)
            return false;

        m_HeartRateMeasurement_Descriptor = m_PMDDataMTU_Characteristic->findDescriptor(*k_Descriptor_HeartRateMeasurement_UUID);
        if (m_HeartRateMeasurement_Descriptor == nullptr)
            return false;

		m_HeartRateMeasurement_DescriptorValue= m_HeartRateMeasurement_Descriptor->getDescriptorValue();
		if (m_HeartRateMeasurement_DescriptorValue == nullptr)
			return false;

		// Fire up the worker thread
		WorkerThread::startThread();

		return true;
    }

	void stop()
	{
		WorkerThread::stopThread();
	}

	// Called from main thread
	void setActiveSensorDataStreams(t_hsl_stream_bitmask data_stream_flags)
	{
		m_streamListenerBitmask.store(data_stream_flags);
	}

protected:
	virtual void onThreadStarted() override
	{
		setPMDControlPointIndicationEnabled(true);
		setPMDDataMTUNotificationEnabled(true);

		fetchPMDServiceCapabilities();
	}

	void setPMDControlPointIndicationEnabled(bool bIsEnabled)
	{
		if (m_bIsPMDControlPointIndicationEnabled == bIsEnabled)
			return;

        BLEGattDescriptorValue::ClientCharacteristicConfiguration clientConfig;
        if (!m_PMDCtrlPoint_DescriptorValue->getClientCharacteristicConfiguration(clientConfig))
			return;

        clientConfig.IsSubscribeToIndication = bIsEnabled;
        if (!m_PMDCtrlPoint_DescriptorValue->setClientCharacteristicConfiguration(clientConfig))
			return;

		m_bIsPMDControlPointIndicationEnabled= bIsEnabled;
	}

    void setPMDDataMTUNotificationEnabled(bool bIsEnabled)
    {
		if (m_bIsPMDDataMTUNotificationEnabled == bIsEnabled)
			return;

        if (!m_PMDDataMTU_DescriptorValue->readDescriptorValue())
            return;

        BLEGattDescriptorValue::ClientCharacteristicConfiguration clientConfig;
        if (!m_PMDDataMTU_DescriptorValue->getClientCharacteristicConfiguration(clientConfig))
            return;

        clientConfig.IsSubscribeToNotification = bIsEnabled;
        if (!m_PMDDataMTU_DescriptorValue->setClientCharacteristicConfiguration(clientConfig))
            return;

		if (bIsEnabled)
		{
			m_PMDDataMTUCallbackHandle =
				m_PMDDataMTU_Characteristic->registerChangeEvent(
					std::bind(
						&PolarH10PacketProcessor::OnReceivedPMDDataMTUPacket, this,
						std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		}
		else
		{
			if (m_PMDDataMTUCallbackHandle != k_invalid_ble_gatt_event_handle)
			{
				m_PMDDataMTU_Characteristic->unregisterChangeEvent(m_PMDDataMTUCallbackHandle);
				m_PMDDataMTUCallbackHandle= k_invalid_ble_gatt_event_handle;
			}
		}

        m_bIsPMDDataMTUNotificationEnabled= bIsEnabled;
    }

	void fetchPMDServiceCapabilities()
	{
		if (!m_PMDCtrlPoint_Characteristic->getIsReadable())
			return;

		BLEGattCharacteristicValue *value= m_PMDCtrlPoint_Characteristic->getCharacteristicValue();

		uint8_t *feature_buffer= nullptr;
		size_t feature_buffer_size= 0;
		if (!value->getData(&feature_buffer, &feature_buffer_size))
			return;

		if (feature_buffer_size == 0)
			return;

		// 0x0F = control point feature read response
		if (feature_buffer[0] != 0x0F)
			return;

		const int feature_bitmask= feature_buffer[1];
		const bool ecg_supported= HSL_BITMASK_GET_FLAG(feature_bitmask, 0);
		const bool ppg_supported= HSL_BITMASK_GET_FLAG(feature_bitmask, 1);
		const bool acc_supported= HSL_BITMASK_GET_FLAG(feature_bitmask, 2);
		const bool ppi_supported= HSL_BITMASK_GET_FLAG(feature_bitmask, 3);

		m_streamCapabilitiesBitmask = 0;
		if (ecg_supported)
			HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_ECGData);
        if (ppg_supported)
            HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_PPGData);
        if (acc_supported)
            HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_AccData);
        if (ppi_supported)
            HSL_BITMASK_SET_FLAG(m_streamCapabilitiesBitmask, HSLStreamFlags_PPIData);
	}

	bool startAccStream(const PolarH10SensorConfig &config)
	{
		StackBuffer<14> stream_settings;

		stream_settings.writeByte(0x02); // Start Measurement
		stream_settings.writeByte(0x02); // Accelerometer Stream

		stream_settings.writeByte(0x00); // Setting Type: 0 = SAMPLE_RATE
		stream_settings.writeByte(0x01); // 1 = array_count(1) 
		stream_settings.writeShort((uint16_t)config.accSampleRate);

        stream_settings.writeByte(0x01); // Setting Type: 1 = RESOLUTION
        stream_settings.writeByte(0x01); // 1 = array_count(1) 
        stream_settings.writeShort(0x0001); // 1 = 16-bit

        stream_settings.writeByte(0x02); // Setting Type: 2 = RANGE
        stream_settings.writeByte(0x01); // 1 = array_count(1) 
        stream_settings.writeShort(0x0008); //  2 = 2G , 4 = 4G , 8 = 8G

		if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getSize()))
			return false;

		uint8_t *response_buffer= nullptr;
		size_t response_buffer_size= 0;
		if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
			return false;

		if (response_buffer_size < 4)
			return false;

		uint8_t expected_response_prefix[4]= {
			0xF0, // control point response
			0x02, // op_code(Start Measurement)
			0x02, // measurement_type(ACC)
			0x00 // 00 = error_code(success)
		};
		if (memcmp(response_buffer, expected_response_prefix, 4) != 0) 
			return false;

		// Reset the stream start timestamp
		m_accStreamStartTimestamp= 0;

		return true;
	}

    bool stopAccStream()
    {
        StackBuffer<2> stream_settings;

        stream_settings.writeByte(0x03); // Stop Measurement
        stream_settings.writeByte(0x02); // Accelerometer Stream

        if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getSize()))
            return false;

		uint8_t* response_buffer = nullptr;
		size_t response_buffer_size = 0;
		if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
			return false;

		if (response_buffer_size < 4)
			return false;

        uint8_t expected_response_prefix[4] = {
            0xF0, // control point response
            0x03, // op_code(Stop Measurement)
            0x02, // measurement_type(ACC)
            0x00 // 00 = error_code(success)
        };
        if (memcmp(response_buffer, expected_response_prefix, 4) != 0)
            return false;

        return true;
    }

    bool startECGStream(const PolarH10SensorConfig& config)
    {
        StackBuffer<14> stream_settings;

        stream_settings.writeByte(0x02); // Start Measurement
        stream_settings.writeByte(0x00); // ECG Stream

        stream_settings.writeByte(0x00); // Setting Type: 0 = SAMPLE_RATE
        stream_settings.writeByte(0x01); // 1 = array_count(1) 
        stream_settings.writeShort((uint16_t)config.ecgSampleRate);

        stream_settings.writeByte(0x01); // Setting Type: 1 = RESOLUTION
        stream_settings.writeByte(0x01); // 1 = array_count(1) 
        stream_settings.writeShort(0x000E); // 1 = 14-bit

		if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getSize()))
			return false;

		uint8_t* response_buffer = nullptr;
		size_t response_buffer_size = 0;
		if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
			return false;

		if (response_buffer_size < 4)
			return false;

        uint8_t expected_response_prefix[4] = {
            0xF0, // control point response
            0x02, // op_code(Start Measurement)
            0x00, // measurement_type(ECG)
            0x00 // 00 = error_code(success)
        };
        if (memcmp(response_buffer, expected_response_prefix, 4) != 0)
            return false;

        // Reset the stream start timestamp
        m_ecgStreamStartTimestamp = 0;

        return true;
    }

    bool stopECGStream()
    {
        StackBuffer<2> stream_settings;

        stream_settings.writeByte(0x03); // Stop Measurement
        stream_settings.writeByte(0x00); // ECG Stream

		if (!m_PMDCtrlPoint_CharacteristicValue->setData(stream_settings.getBuffer(), stream_settings.getSize()))
			return false;

		uint8_t* response_buffer = nullptr;
		size_t response_buffer_size = 0;
		if (!m_PMDCtrlPoint_CharacteristicValue->getData(&response_buffer, &response_buffer_size))
			return false;

		if (response_buffer_size < 4)
			return false;

        uint8_t expected_response_prefix[4] = {
            0xF0, // control point response
            0x03, // op_code(Stop Measurement)
            0x00, // measurement_type(ECG)
            0x00 // 00 = error_code(success)
        };
        if (memcmp(response_buffer, expected_response_prefix, 4) != 0)
            return false;

        return true;
    }

	void startHeartRateStream()
	{
		if (m_bIsHeartRateNotificationEnabled)
			return;

        if (!m_HeartRateMeasurement_DescriptorValue->readDescriptorValue())
            return;

        BLEGattDescriptorValue::ClientCharacteristicConfiguration clientConfig;
        if (!m_HeartRateMeasurement_DescriptorValue->getClientCharacteristicConfiguration(clientConfig))
            return;

        clientConfig.IsSubscribeToNotification = true;
        if (!m_PMDDataMTU_DescriptorValue->setClientCharacteristicConfiguration(clientConfig))
            return;

        m_HeartRateCallbackHandle =
            m_HeartRateMeasurement_Characteristic->registerChangeEvent(
                std::bind(
                    &PolarH10PacketProcessor::OnReceivedHRDataPacket, this,
                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

		// Time on Heart Rate samples are relative to stream start
		m_hrStreamStartTimestamp= std::chrono::high_resolution_clock::now();

        // From: https://www.bluetooth.com/xml-viewer/?src=https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.heart_rate_control_point.xml
        // resets the value of the Energy Expended field in the Heart Rate Measurement characteristic to 0
		BLEGattCharacteristic *heartRateControlPoint_Characteristic = 
			m_HeartRate_Service->findCharacteristic(*k_Characteristic_HeartRateControlPoint_UUID);
        if (heartRateControlPoint_Characteristic != nullptr)
		{
			BLEGattCharacteristicValue *value= heartRateControlPoint_Characteristic->getCharacteristicValue();

			value->setByte(0x01);
		}

        m_bIsHeartRateNotificationEnabled = true;
	}

	void stopHeartRateStream()
	{
        if (!m_bIsHeartRateNotificationEnabled)
            return;

        if (m_HeartRateCallbackHandle != k_invalid_ble_gatt_event_handle)
        {
            m_HeartRateMeasurement_Characteristic->unregisterChangeEvent(m_HeartRateCallbackHandle);
            m_HeartRateCallbackHandle = k_invalid_ble_gatt_event_handle;
        }

		m_bIsHeartRateNotificationEnabled= false;
	}

	virtual bool doWork() override
    {
		PolarH10SensorConfig config;
		m_cfg.fetchValue(config);
		//TODO: Handle changes to the the sampling frequencies

		// Process stream activation/deactivation requests
		t_hsl_stream_bitmask listener_bitmask= m_streamListenerBitmask.load();
		if (listener_bitmask != m_streamActiveBitmask)
		{
			for (int stream_index = 0; stream_index < HSLStreamFlags_COUNT; ++stream_index)
			{
				HSLSensorDataStreamFlags stream_flag = (HSLSensorDataStreamFlags)stream_index;
				bool wants_active = HSL_BITMASK_GET_FLAG(listener_bitmask, stream_flag);
				bool is_active = HSL_BITMASK_GET_FLAG(m_streamActiveBitmask, stream_flag);
				
				if (wants_active && !is_active)
				{
					bool can_activate = HSL_BITMASK_GET_FLAG(m_streamCapabilitiesBitmask, stream_flag);

					if (can_activate)
					{
						switch(stream_flag)
						{
						case HSLStreamFlags_AccData:
							startAccStream(config);
							break;
						case HSLStreamFlags_ECGData:
							startECGStream(config);
							break;
						case HSLStreamFlags_HRData:
							startHeartRateStream();
							break;
						}
					}
				}
				else if (!wants_active && is_active)
				{
                    switch (stream_flag)
                    {
                    case HSLStreamFlags_AccData:
                        stopAccStream();
						break;
                    case HSLStreamFlags_ECGData:
                        stopECGStream();
                        break;
                    case HSLStreamFlags_HRData:
                        stopHeartRateStream();
                        break;
                    }
				}
			}
		}

		Utility::sleep_ms(10);

		return true;
    }

	void OnReceivedPMDDataMTUPacket(BluetoothGattHandle attributeHandle, uint8_t *data, size_t data_size)
	{
		// Send the sensor data for processing by filter
		if (m_sensorListener != nullptr)
		{
			StackBuffer<1024> packet_data(data, data_size);

            ISensorListener::SensorPacket packet;
            memset(&packet, 0, sizeof(ISensorListener::SensorPacket));

			uint8_t frame_type= packet_data.readByte();
			switch (frame_type)
			{
			case 0x00: // ECG
				{
					uint64_t timestamp = packet_data.readLong();
					if (m_ecgStreamStartTimestamp == 0)
					{
						m_ecgStreamStartTimestamp = timestamp;
					}

					const std::chrono::nanoseconds nanoseconds(timestamp - m_ecgStreamStartTimestamp);
					const auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(nanoseconds);

					if (packet_data.readByte() == 0x00) // ECG frame type
					{
						int ecg_value_capacity= ARRAY_SIZE(packet.payload.ecgFrame.ecgValues);

						while (packet_data.bytesRemaining() > 0)
						{
							uint8_t raw_microvolt_value[3]= {0x00, 0x00, 0x00};
							packet_data.readBytes(raw_microvolt_value, 3);
							uint16_t *microvolt_value= (uint16_t *)raw_microvolt_value;

							packet.payloadType = ISensorListener::SensorPacketPayloadType::ECGFrame;
							packet.payload.ecgFrame.timeInSeconds = seconds.count();
							packet.payload.ecgFrame.ecgValues[packet.payload.ecgFrame.ecgValueCount]= (*microvolt_value);
							packet.payload.ecgFrame.ecgValueCount++;

							if (packet.payload.ecgFrame.ecgValueCount >= ecg_value_capacity)
							{
								m_sensorListener->notifySensorDataReceived(&packet);
								packet.payload.ecgFrame.ecgValueCount= 0;
							}
						}

						if (packet.payload.ecgFrame.ecgValueCount > 0)
						{
                            m_sensorListener->notifySensorDataReceived(&packet);
						}
					}
				}
				break;
			case 0x02: // Acc
				{
					uint64_t timestamp= packet_data.readLong();
					if (m_accStreamStartTimestamp == 0)
					{
						m_accStreamStartTimestamp= timestamp;
					}

					const std::chrono::nanoseconds nanoseconds(timestamp - m_accStreamStartTimestamp);
					const auto seconds= std::chrono::duration_cast<std::chrono::duration<double>>(nanoseconds);

					if (packet_data.readByte() == 0x01) // 16-bit ACC frame type
					{
						int acc_value_capacity= ARRAY_SIZE(packet.payload.accFrame.accSamples);

						while (packet_data.bytesRemaining() > 0)
						{
							uint16_t x_milli_g = packet_data.readShort();
							uint16_t y_milli_g = packet_data.readShort();
							uint16_t z_milli_g = packet_data.readShort();

							packet.payloadType = ISensorListener::SensorPacketPayloadType::ACCFrame;
							packet.payload.accFrame.timeInSeconds= seconds.count();

							HSLVector3f &sample= packet.payload.accFrame.accSamples[packet.payload.accFrame.accSampleCount];
							sample.x= (float)x_milli_g / 1000.f;
							sample.y= (float)y_milli_g / 1000.f;
							sample.z= (float)z_milli_g / 1000.f;
							packet.payload.accFrame.accSampleCount++;

							if (packet.payload.accFrame.accSampleCount >= acc_value_capacity)
							{
								m_sensorListener->notifySensorDataReceived(&packet);
								packet.payload.accFrame.accSampleCount= 0;
							}
						}

                        if (packet.payload.accFrame.accSampleCount >= 0)
                        {
                            m_sensorListener->notifySensorDataReceived(&packet);
                        }
					}
				}
                break;
			default:
				break;
			}			
		}
	}

	void OnReceivedHRDataPacket(BluetoothGattHandle attributeHandle, uint8_t* data, size_t data_size)
	{
        StackBuffer<64> packet_data(data, data_size);

        ISensorListener::SensorPacket packet;
		memset(&packet, 0, sizeof(ISensorListener::SensorPacket));

        packet.payloadType = ISensorListener::SensorPacketPayloadType::HRFrame;

		auto packet_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> time_in_stream = packet_time - m_hrStreamStartTimestamp;
		packet.payload.hrFrame.timeInSeconds= time_in_stream.count();

        // See Heart Rate Service spec: https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=239866
		uint8_t flags_field= packet_data.readByte();
		bool heart_rate_format_flag= HSL_BITMASK_GET_FLAG(flags_field, 0);
        bool contact_status_flag = HSL_BITMASK_GET_FLAG(flags_field, 1);
		bool contact_status_supported_flag= HSL_BITMASK_GET_FLAG(flags_field, 2);
		bool energy_expended_supported_flag= HSL_BITMASK_GET_FLAG(flags_field, 3);
		bool RR_interval_supported_flag= HSL_BITMASK_GET_FLAG(flags_field, 4);

        if (heart_rate_format_flag)
        {
            packet.payload.hrFrame.beatsPerMinute= packet_data.readShort();
        }
        else
        {
            packet.payload.hrFrame.beatsPerMinute= (uint16_t)packet_data.readByte();
        }

		if (contact_status_supported_flag)
		{
			packet.payload.hrFrame.contactStatus= 
				contact_status_flag ? HSLContactStatus_Contact : HSLContactStatus_NoContact; 
		}
		else
		{
            packet.payload.hrFrame.contactStatus = HSLContactStatus_Invalid;
		}

		if (energy_expended_supported_flag)
		{
			packet.payload.hrFrame.energyExpended= packet_data.readShort();
		}

		if (RR_interval_supported_flag)
		{
			int rr_value_capacity= ARRAY_SIZE(packet.payload.hrFrame.RRIntervals);

			while (packet_data.bytesRemaining() > 0)
			{
				packet.payload.hrFrame.RRIntervals[packet.payload.hrFrame.RRIntervalCount]= packet_data.readShort();
				packet.payload.hrFrame.RRIntervalCount++;

                if (packet.payload.hrFrame.RRIntervalCount >= rr_value_capacity)
                {
                    m_sensorListener->notifySensorDataReceived(&packet);
                    packet.payload.hrFrame.RRIntervalCount = 0;
                }
			}

            if (packet.payload.hrFrame.RRIntervalCount >= 0)
            {
                m_sensorListener->notifySensorDataReceived(&packet);
            }
		}

		m_sensorListener->notifySensorDataReceived(&packet);
	}

	virtual void onThreadHaltBegin() override
	{
        setPMDControlPointIndicationEnabled(false);
        setPMDDataMTUNotificationEnabled(false);
	}

	// GATT Profile cached vars
    BLEGattProfile* m_GATT_Profile;
    const BLEGattService* m_PMD_Service;
    BLEGattCharacteristic* m_PMDCtrlPoint_Characteristic;
	BLEGattCharacteristicValue* m_PMDCtrlPoint_CharacteristicValue;
    BLEGattDescriptor* m_PMDCtrlPoint_Descriptor;
    BLEGattDescriptorValue* m_PMDCtrlPoint_DescriptorValue;
    BLEGattCharacteristic* m_PMDDataMTU_Characteristic;
    BLEGattDescriptor* m_PMDDataMTU_Descriptor;
    BLEGattDescriptorValue* m_PMDDataMTU_DescriptorValue;
	const BLEGattService* m_HeartRate_Service;
	BLEGattCharacteristic* m_HeartRateMeasurement_Characteristic;
	BLEGattDescriptor* m_HeartRateMeasurement_Descriptor;
	BLEGattDescriptorValue* m_HeartRateMeasurement_DescriptorValue;

    // Multi-threaded state
	t_bluetoothle_device_handle m_deviceHandle;
	ISensorListener *m_sensorListener;
	AtomicObject<PolarH10SensorConfig> m_cfg;
	std::atomic_uint32_t m_streamListenerBitmask;

    // Worker thread state
	t_hsl_stream_bitmask m_streamCapabilitiesBitmask;
	t_hsl_stream_bitmask m_streamActiveBitmask;
	uint64_t m_accStreamStartTimestamp;
	uint64_t m_ecgStreamStartTimestamp;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_hrStreamStartTimestamp;
	bool m_bIsPMDControlPointIndicationEnabled;
	bool m_bIsPMDDataMTUNotificationEnabled;
	bool m_bIsHeartRateNotificationEnabled;
	BluetoothEventHandle m_PMDDataMTUCallbackHandle;
	BluetoothEventHandle m_HeartRateCallbackHandle;
};

// -- public methods

// -- PolarH10BluetoothLEDetails
void PolarH10BluetoothLEDetails::reset()
{
	friendlyName= "";
	devicePath= "";
	deviceHandle= k_invalid_ble_device_handle;
	gattProfile= nullptr;
	bluetoothAddress= "";
	memset(&deviceInfo, 0, sizeof(HSLDeviceInformation));
	bodyLocation= "";
}

// -- PSMove Controller Config
// Bump this version when you are making a breaking config change.
// Simply adding or removing a field is ok and doesn't require a version bump.
const int PolarH10SensorConfig::CONFIG_VERSION= 1;

const int k_available_acc_sample_rates[] = { 25, 50, 100, 200 };
const int k_available_ecg_sample_rates[] = { 130 };
const int k_available_hr_sample_rates[] = { 10 };

int PolarH10SensorConfig::sanitizeSampleRate(int test_sample_rate, const int *sample_rate_array)
{
	const int* begin = sample_rate_array;
	const int* end = sample_rate_array + ARRAY_SIZE(sample_rate_array);
	const int* it = std::find(begin, end, test_sample_rate);

	return (it != end) ? test_sample_rate : sample_rate_array[0];
}

PolarH10SensorConfig::PolarH10SensorConfig(const std::string &fnamebase)
	: HSLConfig(fnamebase)
	, isValid(false)
	, version(CONFIG_VERSION)
	, sampleHistoryDuration(1.f)
	, hrvHistorySize(100)
{
	accSampleRate = k_available_acc_sample_rates[0];
	ecgSampleRate = k_available_ecg_sample_rates[0];
	hrSampleRate = k_available_hr_sample_rates[0];
};

const configuru::Config PolarH10SensorConfig::writeToJSON()
{
	configuru::Config pt{

		{"is_valid", isValid},
		{"version", PolarH10SensorConfig::CONFIG_VERSION},
		{"sample_history_duration", sampleHistoryDuration},
		{"hrv_history_size", hrvHistorySize},
		{"hr_sample_rate", hrSampleRate},
		{"ecg_sample_rate", ecgSampleRate},
		{"acc_sample_rate", accSampleRate}
	};

    return pt;
}

void 
PolarH10SensorConfig::readFromJSON(const configuru::Config &pt)
{
    version = pt.get_or<int>("version", 0);

    if (version == PolarH10SensorConfig::CONFIG_VERSION)
    {
        isValid = pt.get_or<bool>("is_valid", false);

		sampleHistoryDuration = pt.get_or<float>("sample_history_duration", sampleHistoryDuration);
		hrvHistorySize = pt.get_or<int>("hrv_history_size", hrvHistorySize);

		hrSampleRate =
			sanitizeSampleRate(
				pt.get_or<int>("hr_sample_rate", hrSampleRate),
				k_available_hr_sample_rates);
		ecgSampleRate =
			sanitizeSampleRate(
				pt.get_or<int>("ecg_sample_rate", ecgSampleRate),
				k_available_ecg_sample_rates);
		accSampleRate =
			sanitizeSampleRate(
				pt.get_or<int>("acc_sample_rate", accSampleRate), 
				k_available_acc_sample_rates);
    }
    else
    {
        HSL_LOG_WARNING("PolarH10SensorConfig") << 
            "Config version " << version << " does not match expected version " << 
            PolarH10SensorConfig::CONFIG_VERSION << ", Using defaults.";
    }
}

// -- PolarH10 Sensor -----
const char *PolarH10Sensor::k_szFriendlyName = "Polar H10";

IDeviceInterface *PolarH10Sensor::PolarH10SensorFactory()
{
	return new PolarH10Sensor();
}

PolarH10Sensor::PolarH10Sensor()
    : m_packetProcessor(nullptr)
	, m_sensorListener(nullptr)
{	
    m_bluetoothLEDetails.reset();
}

PolarH10Sensor::~PolarH10Sensor()
{
    if (getIsOpen())
    {
        HSL_LOG_ERROR("~PolarH10Sensor") << "Controller deleted without calling close() first!";
    }

	if (m_packetProcessor)
	{
		delete m_packetProcessor;
	}
}

bool PolarH10Sensor::open()
{
	SensorDeviceEnumerator enumerator(SensorDeviceEnumerator::CommunicationType_BLE);
	while (enumerator.isValid())
    {
		if (open(&enumerator))
		{
			return true;
		}

		enumerator.next();
    }

    return false;
}

bool PolarH10Sensor::open(
	const DeviceEnumerator *deviceEnum)
{
	const SensorDeviceEnumerator *sensorEnum = static_cast<const SensorDeviceEnumerator *>(deviceEnum);
	const SensorBluetoothLEDeviceEnumerator *sensorBluetoothLEEnum = sensorEnum->getBluetoothLESensorEnumerator();
	const BluetoothLEDeviceEnumerator *deviceBluetoothLEEnum = sensorBluetoothLEEnum->getBluetoothLEDeviceEnumerator();
	const std::string &cur_dev_path= sensorBluetoothLEEnum->getPath();

    bool success= false;

    if (getIsOpen())
    {
        HSL_LOG_WARNING("PolarH10Sensor::open") << "PolarH10Sensor(" << cur_dev_path << ") already open. Ignoring request.";
        success= true;
    }
    else
    {
        bool bSuccess = true;

		// Remember the friendly name of the device
		m_bluetoothLEDetails.friendlyName= sensorBluetoothLEEnum->getFriendlyName();

		// Attempt to open device
        HSL_LOG_INFO("PolarH10Sensor::open") << "Opening PolarH10Sensor(" << cur_dev_path << ")";
        m_bluetoothLEDetails.devicePath = cur_dev_path;
		m_bluetoothLEDetails.deviceHandle= bluetoothle_device_open(deviceBluetoothLEEnum, &m_bluetoothLEDetails.gattProfile);		
        if (!getIsOpen())
        {
			HSL_LOG_ERROR("PolarH10Sensor::open") << "Failed to open PolarH10Sensor(" << cur_dev_path << ")";
			success = false;
        }
        
		// Get the bluetooth address of the sensor.
		// This served as a unique id for the config file.
        char szBluetoothAddress[256];
		if (bluetoothle_device_get_bluetooth_address(
				m_bluetoothLEDetails.deviceHandle,
				szBluetoothAddress, sizeof(szBluetoothAddress)))
		{
			m_bluetoothLEDetails.bluetoothAddress= szBluetoothAddress;
		}
		else
		{
			HSL_LOG_WARNING("PolarH10Sensor::open") << "  EMPTY Bluetooth Address!";
			bSuccess= false;
		}

		// Load the config file (if it exists yet)
		std::string config_suffix = m_bluetoothLEDetails.friendlyName;
		config_suffix.erase(std::remove(config_suffix.begin(), config_suffix.end(), ' '), config_suffix.end());
		m_config = PolarH10SensorConfig(config_suffix);
		m_config.load();


		if (!fetchDeviceInformation())
		{
            HSL_LOG_WARNING("PolarH10Sensor::open") << "Failed to fetch device information";
            bSuccess = false;
		}

		if (!fetchBodySensorLocation())
		{
            HSL_LOG_WARNING("PolarH10Sensor::open") << "Failed to fetch sensor info";
            bSuccess = false;
		}

		// Create the sensor processor thread
		if (bSuccess)
		{
            m_packetProcessor = new PolarH10PacketProcessor(m_config);
            bSuccess= m_packetProcessor->start(
                m_bluetoothLEDetails.deviceHandle,
                m_bluetoothLEDetails.gattProfile,
                m_sensorListener);
		}

		// Write back out the config if we got a valid bluetooth address
		if (bSuccess)
		{
			m_config.save();
		}
    }

    return success;
}

bool PolarH10Sensor::fetchBodySensorLocation()
{
    const BLEGattService* heartRate_Service = m_bluetoothLEDetails.gattProfile->findService(*k_Service_HeartRate_UUID);
    if (heartRate_Service == nullptr)
        return false;

    BLEGattCharacteristic* bodySensorLocation_Characteristic = heartRate_Service->findCharacteristic(*k_Characteristic_BodySensorLocation_UUID);
    if (bodySensorLocation_Characteristic == nullptr)
        return false;

	uint8_t location_enum= 0;
	BLEGattCharacteristicValue* value = bodySensorLocation_Characteristic->getCharacteristicValue();
	if (!value->getByte(location_enum))
		return false;

	switch (location_enum)
	{
	case 0:
		m_bluetoothLEDetails.bodyLocation= "Other";
		break;
    case 1:
        m_bluetoothLEDetails.bodyLocation = "Chest";
        break;
    case 2:
        m_bluetoothLEDetails.bodyLocation = "Wrist";
        break;
    case 3:
        m_bluetoothLEDetails.bodyLocation = "Finger";
        break;
    case 4:
        m_bluetoothLEDetails.bodyLocation = "Hand";
        break;
    case 5:
        m_bluetoothLEDetails.bodyLocation = "Ear Lobe";
        break;
    case 6:
        m_bluetoothLEDetails.bodyLocation = "Foot";
        break;
	default:
		m_bluetoothLEDetails.bodyLocation = "Unknown";
		break;
	}
	return true;
}

static void fetchDeviceInfoString(
	const BLEGattService* deviceInfo_Service,
	const BluetoothUUID* characteristicUUID, 
	size_t max_string_length,
	char *out_string)
{
    BLEGattCharacteristic* deviceInfo_Characteristic = deviceInfo_Service->findCharacteristic(*characteristicUUID);
    if (deviceInfo_Characteristic == nullptr)
        return;

	BLEGattCharacteristicValue *value= deviceInfo_Characteristic->getCharacteristicValue();

	uint8_t *result_buffer= nullptr;
	size_t result_buffer_size = 0;
	if (value->getData(&result_buffer, &result_buffer_size))
	{
		size_t copy_size= std::min(result_buffer_size, max_string_length);

		memcpy(out_string, result_buffer, copy_size);
		out_string[max_string_length-1]= '\0';
	}
}

bool PolarH10Sensor::fetchDeviceInformation()
{
    const BLEGattService* deviceInfo_Service = m_bluetoothLEDetails.gattProfile->findService(*k_Service_DeviceInformation_UUID);
    if (deviceInfo_Service == nullptr)
        return false;

	HSLDeviceInformation &devInfo= m_bluetoothLEDetails.deviceInfo;
    fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_SystemID_UUID, 
		sizeof(devInfo.systemID), devInfo.systemID);
    fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_ModelNumberString_UUID, 
		sizeof(devInfo.modelNumberString), devInfo.modelNumberString);
    fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_SerialNumberString_UUID, 
		sizeof(devInfo.serialNumberString), devInfo.serialNumberString);
    fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_FirmwareRevisionString_UUID, 
		sizeof(devInfo.firmwareRevisionString), devInfo.firmwareRevisionString);
    fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_HardwareRevisionString_UUID, 
		sizeof(devInfo.hardwareRevisionString), devInfo.hardwareRevisionString);
    fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_SoftwareRevisionString_UUID,
		sizeof(devInfo.softwareRevisionString), devInfo.softwareRevisionString);
    fetchDeviceInfoString(
		deviceInfo_Service, k_Characteristic_ManufacturerNameString_UUID, 
		sizeof(devInfo.manufacturerNameString), devInfo.manufacturerNameString);

	return true;
}

void PolarH10Sensor::close()
{
    if (getIsOpen())
    {
        HSL_LOG_INFO("PolarH10Sensor::close") << "Closing PolarH10Sensor(" << m_bluetoothLEDetails.devicePath << ")";

		if (m_packetProcessor != nullptr)
		{
			// halt the HID packet processing thread
			m_packetProcessor->stop();
			delete m_packetProcessor;
			m_packetProcessor= nullptr;
		}

        if (m_bluetoothLEDetails.deviceHandle != k_invalid_ble_device_handle)
        {
			bluetoothle_device_close(m_bluetoothLEDetails.deviceHandle);
			m_bluetoothLEDetails.gattProfile = nullptr;
            m_bluetoothLEDetails.deviceHandle = k_invalid_ble_device_handle;
        }
    }
    else
    {
        HSL_LOG_INFO("PolarH10Sensor::close") << "PolarH10Sensor(" << m_bluetoothLEDetails.devicePath << ") already closed. Ignoring request.";
    }
}

bool PolarH10Sensor::setActiveSensorDataStreams(t_hsl_stream_bitmask data_stream_flags)
{
	if (m_packetProcessor != nullptr)
	{
		m_packetProcessor->setActiveSensorDataStreams(data_stream_flags);
		return true;
	}

	return false;
}

// Getters
bool PolarH10Sensor::matchesDeviceEnumerator(const DeviceEnumerator *enumerator) const
{
    // Down-cast the enumerator so we can use the correct get_path.
    const SensorDeviceEnumerator *pEnum = static_cast<const SensorDeviceEnumerator *>(enumerator);

	bool matches = false;

	if (pEnum->getApiType() == SensorDeviceEnumerator::CommunicationType_BLE)
	{
		const SensorBluetoothLEDeviceEnumerator *sensorBluetoothLEEnum = pEnum->getBluetoothLESensorEnumerator();
		const std::string enumerator_path = pEnum->getPath();

		matches = (enumerator_path == m_bluetoothLEDetails.devicePath);
	}

    return matches;
}

const std::string &PolarH10Sensor::getFriendlyName() const
{
	return m_bluetoothLEDetails.friendlyName;
}

const std::string &PolarH10Sensor::getDevicePath() const
{
    return m_bluetoothLEDetails.devicePath;
}

t_hsl_stream_bitmask PolarH10Sensor::getSensorCapabilities() const
{
	t_hsl_stream_bitmask bitmask = 0;

	HSL_BITMASK_SET_FLAG(bitmask, HSLStreamFlags_HRData);
	HSL_BITMASK_SET_FLAG(bitmask, HSLStreamFlags_ECGData);
	HSL_BITMASK_SET_FLAG(bitmask, HSLStreamFlags_AccData);

	return bitmask;
}

bool PolarH10Sensor::getDeviceInformation(HSLDeviceInformation* out_device_info) const
{
	if (getIsOpen())
	{
        *out_device_info= m_bluetoothLEDetails.deviceInfo;
		return true;
	}

	return false;
}

int PolarH10Sensor::getCapabilitySampleRate(HSLSensorDataStreamFlags flag) const
{
	int sample_rate = 0;

	switch (flag)
	{
	case HSLStreamFlags_HRData:
		sample_rate = m_config.hrSampleRate;
		break;
	case HSLStreamFlags_ECGData:
		sample_rate = m_config.ecgSampleRate;
		break;
	case HSLStreamFlags_AccData:
		sample_rate = m_config.accSampleRate;
		break;
	}

	return sample_rate;
}

void PolarH10Sensor::getAvailableCapabilitySampleRates(
	HSLSensorDataStreamFlags flag,
	const int **out_rates, 
	int *out_rate_count) const
{
	assert(out_rates);
	assert(out_rate_count);

	switch (flag)
	{
	case HSLStreamFlags_HRData:
		*out_rates= k_available_hr_sample_rates;
		*out_rate_count = ARRAY_SIZE(k_available_hr_sample_rates);
		break;
	case HSLStreamFlags_ECGData:
		*out_rates = k_available_ecg_sample_rates;
		*out_rate_count = ARRAY_SIZE(k_available_ecg_sample_rates);
		break;
	case HSLStreamFlags_AccData:
		*out_rates = k_available_acc_sample_rates;
		*out_rate_count = ARRAY_SIZE(k_available_acc_sample_rates);
		break;
	}
}

void PolarH10Sensor::setCapabilitySampleRate(HSLSensorDataStreamFlags flag, int sample_rate)
{
	int new_config_value = 0;
	int *config_value_ptr = nullptr;

	switch (flag)
	{
	case HSLStreamFlags_HRData:
		config_value_ptr = &m_config.hrSampleRate;
		new_config_value = PolarH10SensorConfig::sanitizeSampleRate(sample_rate, k_available_hr_sample_rates);
		break;
	case HSLStreamFlags_ECGData:
		config_value_ptr = &m_config.ecgSampleRate;
		new_config_value = PolarH10SensorConfig::sanitizeSampleRate(sample_rate, k_available_ecg_sample_rates);
		break;
	case HSLStreamFlags_AccData:
		config_value_ptr = &m_config.accSampleRate;
		new_config_value = PolarH10SensorConfig::sanitizeSampleRate(sample_rate, k_available_acc_sample_rates);
		break;
	}

	if (config_value_ptr != nullptr && *config_value_ptr != new_config_value)
	{
		*config_value_ptr = new_config_value;

		// Push updated config to the packet processor thread
		if (m_packetProcessor != nullptr)
		{
			m_packetProcessor->setConfig(m_config);
		}

		// Save the updated config to disk
		m_config.save();
	}
}

float PolarH10Sensor::getSampleHistoryDuration() const
{
	return m_config.sampleHistoryDuration;
}

void PolarH10Sensor::setSampleHistoryDuration(float duration)
{
	if (m_config.sampleHistoryDuration != duration)
	{
		m_config.sampleHistoryDuration = duration;

		// Packet processor thread doesn't care about sample history duration

		// Save the updated config to disk
		m_config.save();
	}
}

int PolarH10Sensor::getHeartRateVariabliyHistorySize() const
{
	return m_config.hrvHistorySize;
}

void PolarH10Sensor::setHeartRateVariabliyHistorySize(int sample_count)
{
	if (m_config.hrvHistorySize != sample_count)
	{
		m_config.hrvHistorySize = sample_count;

		// Packet processor thread doesn't care about hrv history size

		// Save the updated config to disk
		m_config.save();
	}
}

const std::string &PolarH10Sensor::getBluetoothAddress() const
{
    return m_bluetoothLEDetails.bluetoothAddress;
}

bool PolarH10Sensor::getIsOpen() const
{
    return (m_bluetoothLEDetails.deviceHandle != k_invalid_ble_device_handle);
}

void PolarH10Sensor::setSensorListener(ISensorListener *listener)
{
	m_sensorListener= listener;
}

// Setters
void PolarH10Sensor::setConfig(const PolarH10SensorConfig *config)
{
	m_config= *config;

	if (m_packetProcessor != nullptr)
	{
		m_packetProcessor->setConfig(*config);
	}

	m_config.save();
}
