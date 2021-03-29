#include "HSLClient_CAPI.h"
#include "ClientConstants.h"

#include "stdio.h"

#include <chrono>
#include <cstring>

#if defined(__linux) || defined (__APPLE__)
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#define FPS_REPORT_DURATION 500 // ms

class HSLConsoleClient
{
public:
	HSLConsoleClient() 
		: m_keepRunning(true)
	{
		memset(&SensorList, 0, sizeof(HSLSensorList));
	}

	int run()
	{
		// Attempt to start and run the client
		try 
		{
			if (startup())
			{
				while (m_keepRunning)
				{
					update();

					HSL_SLEEP(10);
				}
			}
			else
			{
				fprintf( stderr, "ERROR: Failed to startup the HSL Client\n");
			}
		}
		catch (std::exception& e) 
		{
			fprintf( stderr, "ERROR: %s\n", e.what());
		}

		// Attempt to shutdown the client
		try 
		{
		   shutdown();
		}
		catch (std::exception& e) 
		{
			fprintf( stderr, "ERROR: %s\n", e.what());
		}
	  
		return 0;
   }

private:
	// HSLConsoleClient
	bool startup()
	{
		bool success= true;

		// Attempt to connect to the server
		if (success)
		{
			if (HSL_Initialize(HSLLogSeverityLevel_info))
			{
				char version_string[32];

				HSL_GetVersionString(version_string, sizeof(version_string));
				printf("HSLConsoleClient::startup() - Initialized client version - %s\n", version_string);
			}
			else
			{
				fprintf(stderr, "HSLConsoleClient::startup() - Failed to initialize the client manager\n");
				success= false;
			}
		}

		if (success)
		{
			lastHRDataTimestamp= 0.0;
			lastPPGDataTimestamp= 0.0;
			lastAccDataTimestamp= 0.0;
			lastEDADataTimestamp= 0.0;
		}

		return success;
	}

	void update()
	{	
		// Polls events and updates Sensor state
		if (!HSL_Update())
		{
			m_keepRunning= false;
		}

		// See if we need to rebuild the Sensor list
		if (m_keepRunning && HSL_HasSensorListChanged())
		{
			// Get the current Sensor list
			fetchSensorList();

			// Make sure sensor stream is started
			if (SensorList.count > 0) 
			{
				const HSLSensorListEntry &listEntry= SensorList.sensors[0];

				t_hsl_caps_bitmask cap_flags = 0;

				if (HSL_BITMASK_GET_FLAG(listEntry.deviceInformation.capabilities, HSLCapability_Electrocardiography))
				{
					HSL_BITMASK_SET_FLAG(cap_flags, HSLCapability_Electrocardiography);
				}
				else if (HSL_BITMASK_GET_FLAG(listEntry.deviceInformation.capabilities, HSLCapability_Photoplethysmography))
				{
					HSL_BITMASK_SET_FLAG(cap_flags, HSLCapability_Photoplethysmography);
				}
				else if (HSL_BITMASK_GET_FLAG(listEntry.deviceInformation.capabilities, HSLCapability_HeartRate))
				{
					HSL_BITMASK_SET_FLAG(cap_flags, HSLCapability_HeartRate);
				}
				else if (HSL_BITMASK_GET_FLAG(listEntry.deviceInformation.capabilities, HSLCapability_ElectrodermalActivity))
				{
					HSL_BITMASK_SET_FLAG(cap_flags, HSLCapability_ElectrodermalActivity);
				}

				if (!HSL_SetActiveSensorCapabilityStreams(SensorList.sensors[0].sensorID, cap_flags))
				{
					m_keepRunning = false;
				}
			}
			else 
			{
				printf("HSLConsoleClient::startup() - No Sensors found. Waiting...");
			}
		}

		// Get the Sensor data for the first Sensor
		if (m_keepRunning && SensorList.count > 0)
		{
			const HSLSensorID sensorID= SensorList.sensors[0].sensorID;
			const HSLSensor *sensor= HSL_GetSensor(sensorID);

			if (HSL_BITMASK_GET_FLAG(sensor->activeSensorStreams, HSLCapability_HeartRate))
			{
				for (HSLBufferIterator iter= HSL_GetCapabilityBuffer(sensorID, HSLCapability_HeartRate);
					HSL_IsBufferIteratorValid(&iter);
					HSL_BufferIteratorNext(&iter))
				{
					HSLHeartRateFrame* hrFrame= HSL_BufferIteratorGetHRData(&iter);

					if (hrFrame->timeInSeconds > lastHRDataTimestamp)
					{
						printf("[HR:%fs] %dBPM\n", hrFrame->timeInSeconds, hrFrame->beatsPerMinute);
						lastHRDataTimestamp= hrFrame->timeInSeconds;
					}
				}

				HSL_FlushCapabilityBuffer(sensorID, HSLCapability_HeartRate);
			}

			if (HSL_BITMASK_GET_FLAG(sensor->activeSensorStreams, HSLCapability_Photoplethysmography))
			{
				for (HSLBufferIterator iter = HSL_GetCapabilityBuffer(sensorID, HSLCapability_Photoplethysmography);
					 HSL_IsBufferIteratorValid(&iter);
					 HSL_BufferIteratorNext(&iter))
				{
					HSLHeartPPGFrame* ppgFrame = HSL_BufferIteratorGetPPGData(&iter);

					if (ppgFrame->timeInSeconds > lastPPGDataTimestamp)
					{
						printf("[PPG: %fs]\n", ppgFrame->timeInSeconds);
						for (int sample_index = 0; sample_index < ppgFrame->ppgSampleCount; ++sample_index)
						{
							const HSLHeartPPGSample& ppgSample = ppgFrame->ppgSamples[sample_index];

							printf("    [0:%d, 1:%d, 2:%d], amb:%d\n",
								   ppgSample.ppgValue0, ppgSample.ppgValue1, ppgSample.ppgValue2, ppgSample.ambient);
						}

						lastPPGDataTimestamp= ppgFrame->timeInSeconds;
					}
				}

				HSL_FlushCapabilityBuffer(sensorID, HSLCapability_Photoplethysmography);
			}

			if (HSL_BITMASK_GET_FLAG(sensor->activeSensorStreams, HSLCapability_PulseInterval))
			{
				for (HSLBufferIterator iter = HSL_GetCapabilityBuffer(sensorID, HSLCapability_PulseInterval);
					 HSL_IsBufferIteratorValid(&iter);
					 HSL_BufferIteratorNext(&iter))
				{
					HSLHeartPPIFrame* ppiFrame = HSL_BufferIteratorGetPPIData(&iter);

					printf("[PPI: %fs]\n", ppiFrame->timeInSeconds);
					for (int sample_index = 0; sample_index < ppiFrame->ppiSampleCount; ++sample_index)
					{
						const HSLHeartPPISample& ppiSample = ppiFrame->ppiSamples[sample_index];

						printf("    BPM:%d, Dur:%d(ms), Err:%d(ms)\n",
							   ppiSample.beatsPerMinute, ppiSample.pulseDuration, ppiSample.pulseDurationErrorEst);
					}
				}

				HSL_FlushCapabilityBuffer(sensorID, HSLCapability_PulseInterval);
			}

			if (HSL_BITMASK_GET_FLAG(sensor->activeSensorStreams, HSLCapability_Accelerometer))
			{
				for (HSLBufferIterator iter = HSL_GetCapabilityBuffer(sensorID, HSLCapability_Accelerometer);
					 HSL_IsBufferIteratorValid(&iter);
					 HSL_BufferIteratorNext(&iter))
				{
					HSLAccelerometerFrame* accFrame = HSL_BufferIteratorGetAccData(&iter);

					if (accFrame->timeInSeconds > lastAccDataTimestamp)
					{
						printf("[ACC: %fs]\n", accFrame->timeInSeconds);
						for (int sample_index = 0; sample_index < accFrame->accSampleCount; ++sample_index)
						{
							const HSLVector3f& accSample = accFrame->accSamples[sample_index];

							printf("    [0:%.2f, 1:%.2f, 2:%.2f]\n", accSample.x, accSample.y, accSample.z);
						}

						lastAccDataTimestamp= accFrame->timeInSeconds;
					}
				}

				HSL_FlushCapabilityBuffer(sensorID, HSLCapability_Accelerometer);
			}

			if (HSL_BITMASK_GET_FLAG(sensor->activeSensorStreams, HSLCapability_ElectrodermalActivity))
			{
				for (HSLBufferIterator iter = HSL_GetCapabilityBuffer(sensorID, HSLCapability_ElectrodermalActivity);
					HSL_IsBufferIteratorValid(&iter);
					HSL_BufferIteratorNext(&iter))
				{
					HSLElectrodermalActivityFrame *edaData= HSL_BufferIteratorGetEDAData(&iter);

					if (edaData->timeInSeconds > lastEDADataTimestamp)
					{
						printf("[EDA: %fs] %f%cS\n", edaData->timeInSeconds, edaData->conductanceMicroSiemens, 230);
						lastEDADataTimestamp = edaData->timeInSeconds;
					}
				}

				HSL_FlushCapabilityBuffer(sensorID, HSLCapability_ElectrodermalActivity);
			}
		}
	}

	void shutdown()
	{
		if (SensorList.count > 0)
		{
			HSL_StopAllSensorStreams(SensorList.sensors[0].sensorID);
		}

		HSL_Shutdown();
	}

	void fetchSensorList()
	{
		memset(&SensorList, 0, sizeof(HSLSensorList));
		HSL_GetSensorList(&SensorList);

		printf("Found %d Sensors.\n", SensorList.count);
		for (int sensor_index=0; sensor_index<SensorList.count; ++sensor_index) 
		{
			printf("  Sensor ID: %s\n", SensorList.sensors[sensor_index].deviceInformation.deviceFriendlyName);
		}
	}

private:
	bool m_keepRunning;
	double lastHRDataTimestamp;
	double lastPPGDataTimestamp;
	double lastAccDataTimestamp;
	double lastEDADataTimestamp;
	HSLSensorList SensorList;
};

int main(int argc, char *argv[])
{   
	HSLConsoleClient app;

	// app instantiation
	return app.run();
}

