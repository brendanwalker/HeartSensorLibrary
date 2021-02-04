//-- includes -----
#include "ServerDeviceView.h"
#include "Logger.h"

#include <chrono>

//-- private methods -----

//-- public implementation -----
ServerDeviceView::ServerDeviceView(
	const int device_id)
	: m_deviceID(device_id)
{
}

ServerDeviceView::~ServerDeviceView()
{
}

bool
ServerDeviceView::open(const DeviceEnumerator *enumerator)
{
	// Attempt to allocate the device 
	bool bSuccess= allocateDeviceInterface(enumerator);
	
	// Attempt to open the device
	if (bSuccess)
	{
		bSuccess= getDevice()->open(enumerator);
	}
	
	return bSuccess;
}

bool
ServerDeviceView::getIsOpen() const
{
	IDeviceInterface* device= getDevice();

	return (device != nullptr) ? device->getIsOpen() : false;
}

void
ServerDeviceView::close()
{
	if (getIsOpen())
	{
		getDevice()->close();
		freeDeviceInterface();
	}
}

bool
ServerDeviceView::matchesDeviceEnumerator(const DeviceEnumerator *enumerator) const
{
	return getIsOpen() && getDevice()->matchesDeviceEnumerator(enumerator);
}