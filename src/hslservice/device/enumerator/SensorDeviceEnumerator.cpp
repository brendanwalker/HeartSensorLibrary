// -- includes -----
#include "SensorDeviceEnumerator.h"
#include "SensorBluetoothLEDeviceEnumerator.h"
#include "assert.h"
#include "string.h"

// -- globals -----

// -- SensorDeviceEnumerator -----
SensorDeviceEnumerator::SensorDeviceEnumerator(
	eAPIType _apiType)
	: DeviceEnumerator()
	, api_type(_apiType)
	, enumerators(nullptr)
	, enumerator_count(0)
	, enumerator_index(0)
{
	switch (_apiType)
	{
	case eAPIType::CommunicationType_BLE:
		enumerators = new DeviceEnumerator *[1];
		enumerators[0] = new SensorBluetoothLEDeviceEnumerator;
		enumerator_count = 1;
		break;
	case eAPIType::CommunicationType_ALL:
		enumerators = new DeviceEnumerator *[1];
		enumerators[0] = new SensorBluetoothLEDeviceEnumerator;
		enumerator_count = 1;
		break;
	}

	if (isValid())
	{
		m_currentFriendlyName= enumerators[0]->getFriendlyName();
		m_currentPath=enumerators[0]->getPath();
	}
	else
	{
		next();
	}
}

SensorDeviceEnumerator::~SensorDeviceEnumerator()
{
	for (int index = 0; index < enumerator_count; ++index)
	{
		delete enumerators[index];
	}
	delete[] enumerators;
}

const std::string& SensorDeviceEnumerator::getFriendlyName() const
{
	return m_currentFriendlyName;
}

const std::string& SensorDeviceEnumerator::getPath() const
{
	return m_currentPath;
}


SensorDeviceEnumerator::eAPIType SensorDeviceEnumerator::getApiType() const
{
	SensorDeviceEnumerator::eAPIType result= SensorDeviceEnumerator::CommunicationType_INVALID;

	switch (api_type)
	{
	case eAPIType::CommunicationType_BLE:
		result = 
			(enumerator_index < enumerator_count) 
			? SensorDeviceEnumerator::CommunicationType_BLE 
			: SensorDeviceEnumerator::CommunicationType_INVALID;
		break;
	case eAPIType::CommunicationType_ALL:
		if (enumerator_index < enumerator_count)
		{
			switch (enumerator_index)
			{
			case 0:
				result = SensorDeviceEnumerator::CommunicationType_BLE;
				break;
			default:
				result = SensorDeviceEnumerator::CommunicationType_INVALID;
				break;
			}
		}
		else
		{
			result = SensorDeviceEnumerator::CommunicationType_INVALID;
		}
		break;
	}

	return result;
}

const SensorBluetoothLEDeviceEnumerator *SensorDeviceEnumerator::getBluetoothLESensorEnumerator() const
{
	SensorBluetoothLEDeviceEnumerator *enumerator = nullptr;

	switch (api_type)
	{
	case eAPIType::CommunicationType_BLE:
		enumerator = 
			(enumerator_index < enumerator_count) 
			? static_cast<SensorBluetoothLEDeviceEnumerator *>(enumerators[0]) 
			: nullptr;
		break;
	case eAPIType::CommunicationType_ALL:
		if (enumerator_index < enumerator_count)
		{
			enumerator = 
				(enumerator_index == 0) 
				? static_cast<SensorBluetoothLEDeviceEnumerator *>(enumerators[0]) 
				: nullptr;
		}
		else
		{
			enumerator = nullptr;
		}
		break;
	}

	return enumerator;
}

bool SensorDeviceEnumerator::isValid() const
{
	bool bIsValid = false;

	if (enumerator_index < enumerator_count)
	{
		bIsValid = enumerators[enumerator_index]->isValid();
	}

	return bIsValid;
}

bool SensorDeviceEnumerator::next()
{
	bool foundValid = false;

	while (!foundValid && enumerator_index < enumerator_count)
	{
		if (enumerators[enumerator_index]->isValid())
		{
			enumerators[enumerator_index]->next();
			foundValid = enumerators[enumerator_index]->isValid();
		}
		else
		{
			++enumerator_index;

			if (enumerator_index < enumerator_count)
			{
				foundValid = enumerators[enumerator_index]->isValid();
			}
		}
	}

	if (foundValid)
	{
        m_currentFriendlyName = enumerators[enumerator_index]->getFriendlyName();
        m_currentPath = enumerators[enumerator_index]->getPath();
	}
	else
	{
		m_currentFriendlyName.clear();
		m_currentPath.clear();
	}

	return foundValid;
}