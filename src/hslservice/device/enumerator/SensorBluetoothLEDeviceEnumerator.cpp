// -- includes -----
#include "BluetoothLEDeviceManager.h"
#include "SensorBluetoothLEDeviceEnumerator.h"
#include "Utility.h"
#include "Logger.h"
#include "BluetoothLEServiceIDs.h"

// -- private definitions -----
#ifdef _MSC_VER
#pragma warning (disable: 4996) // 'This function or variable may be unsafe': snprintf
#define snprintf _snprintf
#endif

// -- methods -----
SensorBluetoothLEDeviceEnumerator::SensorBluetoothLEDeviceEnumerator()
	: DeviceEnumerator()
	, m_ble_enumerator(nullptr)
{
	BluetoothLEDeviceManager *bleRequestMgr = BluetoothLEDeviceManager::getInstance();

	m_ble_enumerator = bluetoothle_device_enumerator_allocate();

	// If the first BLE device handle isn't a heart sensor, move on to the next sensor
	if (!testBLEEnumerator())
	{
		next();
	}
}

SensorBluetoothLEDeviceEnumerator::SensorBluetoothLEDeviceEnumerator(const std::string &ble_path)
	: DeviceEnumerator()
	, m_ble_enumerator(nullptr)
{
	BluetoothLEDeviceManager *bleRequestMgr = BluetoothLEDeviceManager::getInstance();

	m_ble_enumerator = bluetoothle_device_enumerator_allocate();

	// If the first BLE device handle isn't a tracker, move on to the next device
	while (isValid())
	{
		// Find the device with the matching ble path
		if (testBLEEnumerator() && m_currentBLEPath == ble_path)
		{
            break;
		}
		else
		{
			next();
		}
    }
}

SensorBluetoothLEDeviceEnumerator::~SensorBluetoothLEDeviceEnumerator()
{
	if (m_ble_enumerator != nullptr)
	{
		bluetoothle_device_enumerator_free(m_ble_enumerator);
	}
}

const std::string &SensorBluetoothLEDeviceEnumerator::getFriendlyName() const
{
    return m_currentFriendlyName;
}

const std::string &SensorBluetoothLEDeviceEnumerator::getPath() const
{
	// Return a pointer to our member variable that has the path cached
	return m_currentBLEPath;
}

const std::string &SensorBluetoothLEDeviceEnumerator::getUniqueIdentifier() const
{
    return m_currentBLEIdentifier;
}

bool SensorBluetoothLEDeviceEnumerator::isValid() const
{
	return m_ble_enumerator != nullptr && bluetoothle_device_enumerator_is_valid(m_ble_enumerator);
}

bool SensorBluetoothLEDeviceEnumerator::next()
{
	BluetoothLEDeviceManager *bleRequestMgr = BluetoothLEDeviceManager::getInstance();
	bool foundValid = false;

	while (isValid() && !foundValid)
	{
		bluetoothle_device_enumerator_next(m_ble_enumerator);

		if (testBLEEnumerator())
		{
			foundValid= true;
		}
	}

	return foundValid;
}

bool SensorBluetoothLEDeviceEnumerator::testBLEEnumerator()
{
	bool found_valid= false;

	if (isValid())
	{
        BluetoothUUIDSet service_ids;
        bluetoothle_device_enumerator_get_service_ids(m_ble_enumerator, service_ids);

		if (service_ids.containsUUID(*k_Service_HeartRate_UUID))
		{
            char BLEPath[256];
            char BLEIdentifier[256];

            bluetoothle_device_enumerator_get_path(m_ble_enumerator, BLEPath, sizeof(BLEPath));
            bluetoothle_device_enumerator_get_unique_id(m_ble_enumerator, BLEIdentifier, sizeof(BLEIdentifier));

            // Remember the last BluetoothLE device path
            m_currentBLEPath= BLEPath;
            m_currentBLEIdentifier= BLEIdentifier;

            m_currentDriverType = bluetoothle_device_enumerator_get_driver_type(m_ble_enumerator);

            found_valid = true;
		}
	}

	return found_valid;
}