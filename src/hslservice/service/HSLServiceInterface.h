#ifndef HSL_SERVICE_INTERFACE_H
#define HSL_SERVICE_INTERFACE_H

//-- includes -----
#include "HSLClient_CAPI.h"

//-- interface -----
class INotificationListener
{
public:
	virtual void handle_notification(const HSLEventMessage &response) = 0;
};

#endif  // PSVRPROTOCOL_INTERFACE_H
