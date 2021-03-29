#ifndef HSL_CLIENT_H
#define HSL_CLIENT_H

//-- includes -----
#include "HSLClient_CAPI.h"
#include "HSLServiceInterface.h"
#include "Logger.h"
#include <deque>
#include <map>
#include <vector>

//-- typedefs -----
typedef std::deque<HSLEventMessage> t_message_queue;

//-- definitions -----
class HSLClient : public INotificationListener
{
public:
	HSLClient();
	virtual ~HSLClient();

	// -- State Queries ----
	bool pollHasSensorListChanged();

	// -- Client HSL API System -----
	bool startup(HSLLogSeverityLevel log_level, class ServiceRequestHandler * request_handler);
	void update();
	void fetchMessagesFromServer();
	bool fetchNextServerMessage(HSLEventMessage *message);
	void flushAllServerMessages();
	void shutdown();

	// -- Client HSL API Requests -----
	HSLSensor* getClientSensorView(HSLSensorID sensor_id);
	HSLBufferIterator getCapabilityBuffer(HSLSensorID sensor_id, HSLSensorCapabilityType cap_type);
	HSLBufferIterator getHeartRateVariabilityBuffer(HSLSensorID sensor_id, HSLHeartRateVariabityFilterType filter);
	bool flushCapabilityBuffer(HSLSensorID sensor_id, HSLSensorCapabilityType cap_type);
	bool flushHeartHrvBuffer(HSLSensorID sensor_id, HSLHeartRateVariabityFilterType filter);
		
protected:
	bool initClientSensorState(HSLSensorID sensor_id);
	void disposeClientSensorState(HSLSensorID sensor_id);
	void updateClientSensorState(HSLSensorID sensor_id, bool updateDeviceInformation);
	void updateAllClientSensorStates(bool updateDeviceInformation);

	// INotificationListener
	virtual void handleNotification(const HSLEventMessage &response) override;

	// Message Helpers
	//-----------------
	void processServerMessage(const HSLEventMessage *event_message);

private:
	//-- Request Handling -----
	class ServiceRequestHandler *m_requestHandler;

	//-- Sensor Views -----
	struct HSLClentSensorState *m_clientSensors;

	bool m_bHasSensorListChanged;

	//-- Messages -----
	// Queue of message received from the most recent call to update()
	// This queue will be emptied automatically at the next call to update().
	t_message_queue m_serverMessageQueue;
};


#endif // HSL_CLIENT_H