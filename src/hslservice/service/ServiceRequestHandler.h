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
	void publishNotification(const HSLEventMessage &message);

	// -- sensor requests -----
	class ServerSensorView *getServerSensorView(HSLSensorID sensor_id);
	bool getSensorList(HSLSensorList *out_sensor_list) const;
	bool setActiveSensorDataStreams(HSLSensorID sensor_id, t_hsl_stream_bitmask data_stream_flags);
	t_hsl_stream_bitmask getActiveSensorDataStreams(HSLSensorID sensor_id) const;
	bool setActiveSensorFilterStreams(HSLSensorID sensor_id, t_hrv_filter_bitmask filter_stream_bitmask);
	t_hrv_filter_bitmask getActiveSensorFilterStreams(HSLSensorID sensor_id) const;
	bool stopAllActiveSensorStreams(HSLSensorID sensor_id);

	// -- general requests -----
	bool getServiceVersion(char *out_version_string, size_t max_version_string) const;		

private:
	class DeviceManager *m_deviceManager;
	class INotificationListener *m_notificationListener;

	// Singleton instance of the class
	// Assigned in startup, cleared in teardown
	static ServiceRequestHandler *m_instance;
};

#endif	// SERVICE_REQUEST_HANDLER_H
