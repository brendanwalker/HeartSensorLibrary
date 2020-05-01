#ifndef DEVICE_ENUMERATOR_H
#define DEVICE_ENUMERATOR_H

// -- includes -----
#include "DeviceInterface.h"
#include <string>

// -- definitions -----
class DeviceEnumerator
{
public:
	DeviceEnumerator() {}
	virtual ~DeviceEnumerator() {}

	virtual bool isValid() const =0;
	virtual bool next()=0;
    virtual const std::string &getFriendlyName() const =0;
	virtual const std::string &getPath() const =0;
};

#endif // DEVICE_ENUMERATOR_H