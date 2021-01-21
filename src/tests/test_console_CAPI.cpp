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
			t_hsl_stream_bitmask data_stream_flags = 0;
			HSL_BITMASK_SET_FLAG(data_stream_flags, HSLStreamFlags_HRData);
			HSL_BITMASK_SET_FLAG(data_stream_flags, HSLStreamFlags_ECGData);
			HSL_BITMASK_SET_FLAG(data_stream_flags, HSLStreamFlags_AccData);
			HSL_BITMASK_SET_FLAG(data_stream_flags, HSLStreamFlags_PPGData);
			HSL_BITMASK_SET_FLAG(data_stream_flags, HSLStreamFlags_PPIData);

			t_hrv_filter_bitmask filter_stream_bitmask = 0;

			// Get the current Sensor list
			rebuildSensorList();

			// Restart the Sensor streams
			if (SensorList.count > 0) 
			{
				if (!HSL_SetActiveSensorDataStreams(SensorList.Sensors[0].sensorID, data_stream_flags, filter_stream_bitmask))
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
			HSLSensorID sensorID= SensorList.Sensors[0].sensorID;      

			//for (HSLBufferIterator iter= HSL_GetHeartRateBuffer(sensorID); 
			//	HSL_IsBufferIteratorValid(&iter);
			//	HSL_BufferIteratorNext(&iter))
			//{
			//	HSLHeartRateFrame* hrFrame= HSL_BufferIteratorGetHRData(&iter);

			//	if (hrFrame->timeInSeconds > lastHRDataTimestamp)
			//	{
			//		printf("[HR:%fs] %dBPM\n", hrFrame->timeInSeconds, hrFrame->beatsPerMinute);
			//		lastHRDataTimestamp= hrFrame->timeInSeconds;
			//	}
			//}

			//for (HSLBufferIterator iter = HSL_GetHeartPPGBuffer(sensorID);
			//	 HSL_IsBufferIteratorValid(&iter);
			//	 HSL_BufferIteratorNext(&iter))
			//{
			//	HSLHeartPPGFrame* ppgFrame = HSL_BufferIteratorGetPPGData(&iter);

			//	if (ppgFrame->timeInSeconds > lastPPGDataTimestamp)
			//	{
			//		printf("[PPG: %fs]\n", ppgFrame->timeInSeconds);
			//		for (int sample_index = 0; sample_index < ppgFrame->ppgSampleCount; ++sample_index)
			//		{
			//			const HSLHeartPPGSample& ppgSample = ppgFrame->ppgSamples[sample_index];

			//			printf("    [0:%d, 1:%d, 2:%d], amb:%d\n",
			//				   ppgSample.ppgValue0, ppgSample.ppgValue1, ppgSample.ppgValue2, ppgSample.ambient);
			//		}

			//		lastPPGDataTimestamp= ppgFrame->timeInSeconds;
			//	}
			//}

			for (HSLBufferIterator iter = HSL_GetHeartPPIBuffer(sensorID);
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

			//for (HSLBufferIterator iter = HSL_GetHeartAccBuffer(sensorID);
			//	 HSL_IsBufferIteratorValid(&iter);
			//	 HSL_BufferIteratorNext(&iter))
			//{
			//	HSLAccelerometerFrame* accFrame = HSL_BufferIteratorGetAccData(&iter);

			//	if (accFrame->timeInSeconds > lastAccDataTimestamp)
			//	{
			//		printf("[ACC: %fs]\n", accFrame->timeInSeconds);
			//		for (int sample_index = 0; sample_index < accFrame->accSampleCount; ++sample_index)
			//		{
			//			const HSLVector3f& accSample = accFrame->accSamples[sample_index];

			//			printf("    [0:%.2f, 1:%.2f, 2:%.2f]\n", accSample.x, accSample.y, accSample.z);
			//		}

			//		lastAccDataTimestamp= accFrame->timeInSeconds;
			//	}
			//}
		}
	}

	void shutdown()
	{
		if (SensorList.count > 0)
		{
			HSL_StopAllSensorDataStreams(SensorList.Sensors[0].sensorID);
		}
		// No tracker data streams started
		// No HMD data streams started

		HSL_Shutdown();
	}

	void rebuildSensorList()
	{
		memset(&SensorList, 0, sizeof(HSLSensorList));
		HSL_GetSensorList(&SensorList);

		printf("Found %d Sensors.\n", SensorList.count);
		for (int sensor_index=0; sensor_index<SensorList.count; ++sensor_index) 
		{
			printf("  Sensor ID: %s\n", SensorList.Sensors[sensor_index].deviceFriendlyName);
		}
	}

private:
	bool m_keepRunning;
	double lastHRDataTimestamp;
	double lastPPGDataTimestamp;
	double lastAccDataTimestamp;
	HSLSensorList SensorList;
};

int main(int argc, char *argv[])
{   
	HSLConsoleClient app;

	// app instantiation
	return app.run();
}

