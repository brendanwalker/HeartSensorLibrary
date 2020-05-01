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
	void process_messages();
	bool poll_next_message(HSLEventMessage *message);
	void shutdown();

	// -- Client HSL API Requests -----
	bool allocate_sensor_listener(HSLSensorID sensor_id);
	void free_sensor_listener(HSLSensorID sensor_id);   
	HSLSensor* get_sensor_view(HSLSensorID sensor_id);
	bool getHeartRateBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator);
	bool getHeartECGBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator);
	bool getHeartPPGBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator);
	bool getHeartPPIBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator);
	bool getHeartAccBuffer(HSLSensorID sensor_id, HSLBufferIterator *out_iterator);
	bool getHeartHrvBuffer(
		HSLSensorID sensor_id,
		HSLHeartRateVariabityFilterType filter,
		HSLBufferIterator *out_iterator);
		
protected:
	// INotificationListener
	virtual void handle_notification(const HSLEventMessage &response) override;

	// Message Helpers
	//-----------------
	void process_event_message(const HSLEventMessage *event_message);

private:
	//-- Request Handling -----
	class ServiceRequestHandler *m_requestHandler;

	//-- Sensor Views -----
	struct HSLClentSensorState *m_clientSensors;

	bool m_bHasSensorListChanged;

	//-- Messages -----
	// Queue of message received from the most recent call to update()
	// This queue will be emptied automatically at the next call to update().
	t_message_queue m_message_queue;
};


#endif // HSL_CLIENT_H