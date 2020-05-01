#ifndef HSL_SERVICE_H
#define HSL_SERVICE_H

//-- includes -----
#include "ClientConstants.h"
#include "Logger.h"
#include <string>

//-- definitions -----
class HSLService
{
public:
		HSLService();
		virtual ~HSLService();

		static HSLService *getInstance()
		{ return m_instance; }

	bool startup(
		HSLLogSeverityLevel log_level, 
		class INotificationListener *notification_listener);
	void update();
	void shutdown();
	
	inline bool getIsInitialized() const { return m_isInitialized; }
	inline class ServiceRequestHandler * getRequestHandler() const { return m_request_handler; }

private:
	// Singleton instance of the class
	static HSLService *m_instance;

	// Manages all BluetoothLE device communication
	class BluetoothLEDeviceManager *m_ble_device_manager;

	// Keep track of currently connected devices (Heart Sensors)
	class DeviceManager *m_device_manager;

	// Generates responses from incoming requests sent from the client API
	class ServiceRequestHandler *m_request_handler;	
	
	bool m_isInitialized;
};

#endif // HSL_SERVICE_H
