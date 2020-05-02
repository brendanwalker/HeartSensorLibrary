//-- includes -----
#include "BluetoothUUID.h"
#include <ctype.h>

//-- constants -----
const char kCommonUuidPostfix[] = "-0000-1000-8000-00805f9b34fb";
const char kCommonUuidPrefix[] = "0000";

//-- public interface -----

// -- BluetoothUUID ----
BluetoothUUID::BluetoothUUID() 
	: uuid128_string()
{
}

BluetoothUUID::BluetoothUUID(const std::string& in_uuid)
	: uuid128_string()
{
	setUUID(in_uuid);
}

void BluetoothUUID::setUUID(const std::string& in_uuid)
{
	// Initialize the values for the failure case.
	if (in_uuid.empty())
	{
		return;
	}

	// Strip off "0x" prefix
	std::string uuid = in_uuid;
	if (uuid.size() >= 2 && uuid[0] == '0' && uuid[1] == 'x')
	{
		uuid = uuid.substr(2);
	}

	if (uuid.size() != 4 && uuid.size() != 8 && uuid.size() != 36)
	{
		return;
	}

	for (size_t i = 0; i < uuid.size(); ++i) 
	{
		if (i == 8 || i == 13 || i == 18 || i == 23) 
		{
			if (uuid[i] != '-')
			{
				return;
			}
		}
		else 
		{
			if (isxdigit(uuid[i]) == 0)
			{
				return;
			}

			uuid[i] = tolower(uuid[i]);
		}
	}

	if (uuid.size() == 4) 
	{
		uuid128_string.assign(kCommonUuidPrefix + uuid + kCommonUuidPostfix);
	}
	else if (uuid.size() == 8) 
	{
		uuid128_string.assign(uuid + kCommonUuidPostfix);
	}
	else 
	{
		uuid128_string.assign(uuid);
	}
}

bool BluetoothUUID::isValid() const 
{
	return uuid128_string.size() > 0;
}

bool BluetoothUUID::operator<(const BluetoothUUID& uuid) const 
{
	return uuid128_string < uuid.uuid128_string;
}

bool BluetoothUUID::operator==(const BluetoothUUID& uuid) const 
{
	return uuid128_string == uuid.uuid128_string;
}

bool BluetoothUUID::operator!=(const BluetoothUUID& uuid) const 
{
	return uuid128_string != uuid.uuid128_string;
}

// -- BluetoothUUIDSet ----
BluetoothUUIDSet::BluetoothUUIDSet() : uuid_set()
{
}

void BluetoothUUIDSet::addUUID(const BluetoothUUID &uuid)
{
	uuid_set.insert(uuid);
}

bool BluetoothUUIDSet::containsUUID(const BluetoothUUID& uuid)
{
	return uuid_set.find(uuid) != uuid_set.end();
}

bool BluetoothUUIDSet::operator == (const BluetoothUUIDSet& other_set) const
{
	return uuid_set == other_set.uuid_set;
}

bool BluetoothUUIDSet::operator != (const BluetoothUUIDSet& other_set) const
{
	return uuid_set != other_set.uuid_set;
}