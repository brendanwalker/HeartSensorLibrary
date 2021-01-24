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
	HSLBufferIterator getHeartRateBuffer(HSLSensorID sensor_id);
	HSLBufferIterator getHeartECGBuffer(HSLSensorID sensor_id);
	HSLBufferIterator getHeartPPGBuffer(HSLSensorID sensor_id);
	HSLBufferIterator getHeartPPIBuffer(HSLSensorID sensor_id);
	HSLBufferIterator getHeartAccBuffer(HSLSensorID sensor_id);
	HSLBufferIterator getHeartHrvBuffer(HSLSensorID sensor_id, HSLHeartRateVariabityFilterType filter);
		
protected:
	bool setup_client_sensor_state(HSLSensorID sensor_id);

	// INotificationListener
	virtual void handle_notification(const HSLEventMessage &response) override;

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