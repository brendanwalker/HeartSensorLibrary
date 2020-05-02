#include "HSLClient_CAPI.h"
#include "ClientConstants.h"
#include <iostream>
#include <iomanip>
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
				std::cerr << "Failed to startup the HSL Client" << std::endl;
			}
		}
		catch (std::exception& e) 
		{
			std::cerr << e.what() << std::endl;
		}

		// Attempt to shutdown the client
		try 
		{
		   shutdown();
		}
		catch (std::exception& e) 
		{
			std::cerr << e.what() << std::endl;
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
			if (HSL_Initialize(HSLLogSeverityLevel_info) == HSLResult_Success)
			{
				char version_string[32];

				HSL_GetVersionString(version_string, sizeof(version_string));
				std::cout << "HSLConsoleClient::startup() - Initialized client version - " << version_string << std::endl;
			}
			else
			{
				std::cout << "HSLConsoleClient::startup() - Failed to initialize the client manager" << std::endl;
				success= false;
			}

			if (success)
			{
				rebuildSensorList();
			
				// Register as listener and start stream for each Sensor
				t_hsl_stream_bitmask data_stream_flags = 0;
				HSL_BITMASK_SET_FLAG(data_stream_flags, HSLStreamFlags_HRData);
				HSL_BITMASK_SET_FLAG(data_stream_flags, HSLStreamFlags_ECGData);
				HSL_BITMASK_SET_FLAG(data_stream_flags, HSLStreamFlags_AccData);

				t_hrv_filter_bitmask filter_stream_bitmask= 0;
				
				if (SensorList.count > 0) 
				{
					if (HSL_SetActiveSensorDataStreams(SensorList.Sensors[0].sensorID, data_stream_flags, filter_stream_bitmask) != HSLResult_Success) 
					{
						success= false;
					}
				}
				else 
				{
					std::cout << "HSLConsoleClient::startup() - No Sensors found. Waiting..." << std::endl;
				}
			}
		}

		if (success)
		{
			last_report_fps_timestamp= 
				std::chrono::duration_cast< std::chrono::milliseconds >( 
					std::chrono::system_clock::now().time_since_epoch() );
		}

		return success;
	}

	void update()
	{
		std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
		std::chrono::milliseconds diff= now - last_report_fps_timestamp;
		
		// Polls events and updates Sensor state
		if (HSL_Update() != HSLResult_Success)
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

			t_hrv_filter_bitmask filter_stream_bitmask = 0;

			// Stop all Sensor streams
			HSL_StopAllSensorDataStreams(SensorList.Sensors[0].sensorID);

			// Get the current Sensor list
			rebuildSensorList();

			// Restart the Sensor streams
			if (SensorList.count > 0) 
			{
				if (HSL_SetActiveSensorDataStreams(SensorList.Sensors[0].sensorID, data_stream_flags, filter_stream_bitmask) != HSLResult_Success)
				{
					m_keepRunning = false;
				}
			}
			else 
			{
				std::cout << "HSLConsoleClient::startup() - No Sensors found. Waiting..." << std::endl;
			}
		}

		// Get the Sensor data for the first Sensor
		if (m_keepRunning && SensorList.count > 0)
		{
			HSLSensorID sensorID= SensorList.Sensors[0].sensorID;      
			HSLBufferIterator iter;

			std::cout << "Sensor 0 (HR ECG AccXYZ):  ";

			if (HSL_GetHeartRateBuffer(sensorID, &iter) == HSLResult_Success)
			{
				HSLHeartRateFrame* hrFrame= HSL_BufferIteratorGetHRData(&iter);

				std::cout << std::setw(12) << std::right << std::setprecision(6) << hrFrame->beatsPerMinute;
			}

			if (HSL_GetHeartECGBuffer(sensorID, &iter) == HSLResult_Success)
			{
				HSLHeartECGFrame* ecgFrame= HSL_BufferIteratorGetECGData(&iter);

				std::cout << std::setw(12) << std::right << std::setprecision(6) << ecgFrame->ecgValues[0];
			}

			if (HSL_GetHeartAccBuffer(sensorID, &iter) == HSLResult_Success)
			{
				HSLAccelerometerFrame* accFrame= HSL_BufferIteratorGetAccData(&iter);

				if (accFrame != nullptr)
				{
					HSLVector3f acc= accFrame->accSamples[0];

					std::cout << std::setw(12) << std::right << std::setprecision(6) << acc.x;
					std::cout << std::setw(12) << std::right << std::setprecision(6) << acc.y;
					std::cout << std::setw(12) << std::right << std::setprecision(6) << acc.z;
				}
			}
			   
			std::cout << std::endl;
		}
	}

	void shutdown()
	{
		if (SensorList.count > 0)
		{
			HSL_StopAllSensorDataStreams(SensorList.Sensors[0].sensorID);
			HSL_FreeSensorListener(SensorList.Sensors[0].sensorID);
		}
		// No tracker data streams started
		// No HMD data streams started

		HSL_Shutdown();
	}

	void rebuildSensorList()
	{
		memset(&SensorList, 0, sizeof(HSLSensorList));
		HSL_GetSensorList(&SensorList);

		std::cout << "Found " << SensorList.count << " Sensors." << std::endl;

		for (int sensor_index=0; sensor_index<SensorList.count; ++sensor_index) 
		{
			std::cout << "  Sensor ID: " << SensorList.Sensors[sensor_index].deviceFriendlyName << std::endl;
		}
	}

private:
	bool m_keepRunning;
	std::chrono::milliseconds last_report_fps_timestamp;
	HSLSensorList SensorList;
};

int main(int argc, char *argv[])
{   
	HSLConsoleClient app;

	// app instantiation
	return app.run();
}

