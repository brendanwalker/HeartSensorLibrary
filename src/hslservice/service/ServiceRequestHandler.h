#ifndef SERVICE_REQUEST_HANDLER_H
#define SERVICE_REQUEST_HANDLER_H

// -- includes -----
#include "HSLClient_CAPI.h"
#include "HSLServiceInterface.h"
#include <bitset>

// -- pre-declarations -----
class DeviceManager;

// -- definitions -----
class ServiceRequestHandler 
{
public:
	ServiceRequestHandler();
	virtual ~ServiceRequestHandler();

	static ServiceRequestHandler *get_instance() { return m_instance; }

	bool startup(
		class DeviceManager *deviceManager,
		class INotificationListener *notification_listener);
	void shutdown();
	
	/// Send a event to the client
	void publish_notification(const HSLEventMessage &message);

	// -- sensor requests -----
	class ServerSensorView *get_sensor_view_or_null(HSLSensorID hmd_id);
	HSLResult get_sensor_list(HSLSensorList *out_sensor_list);
	HSLResult setActiveSensorDataStreams(
		HSLSensorID sensor_id, 
		t_hsl_stream_bitmask data_stream_flags, 
		t_hrv_filter_bitmask filter_stream_bitmask);
	HSLResult stopAllActiveSensorDataStreams(HSLSensorID sensor_id);

	// -- general requests -----
	HSLResult get_service_version(char *out_version_string, size_t max_version_string);		

private:
	class DeviceManager *m_deviceManager;
	class INotificationListener *m_notificationListener;

	// Singleton instance of the class
	// Assigned in startup, cleared in teardown
	static ServiceRequestHandler *m_instance;
};

#endif	// SERVICE_REQUEST_HANDLER_H
