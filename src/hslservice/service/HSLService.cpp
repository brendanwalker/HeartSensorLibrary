//-- includes -----
#include "HSLService.h"
#include "BluetoothLEDeviceManager.h"
#include "ServiceRequestHandler.h"
#include "DeviceManager.h"
#include "Logger.h"
#include "HSLServiceInterface.h"
#include "ServiceVersion.h"

#include <fstream>
#include <cstdio>
#include <string>
#include <chrono>

//-- statics -----
HSLService *HSLService::m_instance= nullptr;

//-- definitions -----
HSLService::HSLService()
	: m_device_manager(nullptr)
	, m_request_handler(nullptr)
	, m_isInitialized(false)
{
	HSLService::m_instance= this;

    m_ble_device_manager = new BluetoothLEDeviceManager();

	// Keep track of currently connected devices (HSL controllers, cameras, HMDs)
	m_device_manager= new DeviceManager();

	// Generates responses from incoming requests sent to the network manager
	m_request_handler= new ServiceRequestHandler();
	
}

HSLService::~HSLService()
{
	delete m_ble_device_manager;
	delete m_device_manager;
	delete m_request_handler;
	
	HSLService::m_instance= nullptr;
}

bool HSLService::startup(
		HSLLogSeverityLevel log_level,
		class INotificationListener *notification_listener)
{
	bool success= true;
	 
	// initialize logging system
	log_init(log_level, "HSLSERVICE.log");

	// Start the service app
	HSL_LOG_INFO("main") << "Starting HSLService v" << HSL_SERVICE_VERSION_STRING;	   
	 
	/** Setup the bluetooth LE subsystem first before we initialize sensors */
	 if (success)
	 {
		 if (!m_ble_device_manager->startup())
		 {
			 HSL_LOG_FATAL("HSLService") << "Failed to initialize the BluetoothLE manager";
			 success = false;
		 }
	 }

	/** Setup the device manager */
	if (success)
	{
		if (!m_device_manager->startup())
		{
			HSL_LOG_FATAL("HSLService") << "Failed to initialize the device manager";
			success= false;
		}
	}

	/** Setup the request handler */
	if (success)
	{
		if (!m_request_handler->startup(m_device_manager, notification_listener))
		{
			HSL_LOG_FATAL("HSLService") << "Failed to initialize the service request handler";
			success= false;
		}
	}

	return success;
}

void HSLService::update()
{
	// Update the list of active tracked devices
	// Send device updates to the client
	m_device_manager->update();
}

void HSLService::shutdown()
{
	HSL_LOG_INFO("main") << "Shutting down HSLService";
	
	// Kill any pending request state
	m_request_handler->shutdown();

	// Disconnect any actively connected controllers
	m_device_manager->shutdown();

	// Shutdown the Bluetooth device management last
	// Must be after device manager since devices can have an active BLE connection
	m_ble_device_manager->shutdown();
	
	log_dispose();
}