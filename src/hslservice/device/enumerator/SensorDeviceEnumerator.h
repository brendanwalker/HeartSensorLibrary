#ifndef SENSOR_DEVICE_ENUMERATOR_H
#define SENSOR_DEVICE_ENUMERATOR_H

#include "DeviceEnumerator.h"

class SensorDeviceEnumerator : public DeviceEnumerator
{
public:
	enum eAPIType
	{
		CommunicationType_INVALID= -1,
		CommunicationType_BLE,
		CommunicationType_ALL
	};

	SensorDeviceEnumerator(eAPIType api_type);
	~SensorDeviceEnumerator();

	bool isValid() const override;
	bool next() override;
    virtual const std::string& getFriendlyName() const override;
    virtual const std::string& getPath() const override;

	eAPIType getApiType() const;
	const class SensorBluetoothLEDeviceEnumerator *getBluetoothLESensorEnumerator() const;

private:
	eAPIType api_type;
	DeviceEnumerator **enumerators;
	int enumerator_count;
	int enumerator_index;
	std::string m_currentFriendlyName;
	std::string m_currentPath;
};

#endif // SENSOR_DEVICE_ENUMERATOR_H
