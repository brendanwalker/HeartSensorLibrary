//-- includes -----
#include "BluetoothLEApiInterface.h"
#include "BluetoothLEDeviceManager.h"
#include "Logger.h"
#include "Utility.h"

#include <atomic>
#include <thread>
#include <vector>
#include <map>

#include "WinBluetoothLEApi.h"

//-- typedefs -----
typedef std::map<t_bluetoothle_device_handle, BluetoothLEDeviceState *> t_ble_device_map;
typedef std::map<t_bluetoothle_device_handle, BluetoothLEDeviceState *>::iterator t_ble_device_map_iterator;
typedef std::pair<t_bluetoothle_device_handle, BluetoothLEDeviceState *> t_handle_ble_device_pair;

//-- constants -----
const char * k_winble_api_name = "winBLE_api";

//-- private implementation -----

//-- BLE Manager Config -----
const int BluetoothLEManagerConfig::CONFIG_VERSION = 1;

BluetoothLEManagerConfig::BluetoothLEManagerConfig(const std::string &fnamebase)
	: HSLConfig(fnamebase)
	, version(CONFIG_VERSION)
{
};

const configuru::Config
BluetoothLEManagerConfig::writeToJSON()
{
	configuru::Config pt{
		{"version", BluetoothLEManagerConfig::CONFIG_VERSION}
	};

	return pt;
}

void
BluetoothLEManagerConfig::readFromJSON(const configuru::Config &pt)
{
	version = pt.get_or<int>("version", 0);

	if (version == BluetoothLEManagerConfig::CONFIG_VERSION)
	{
	}
	else
	{
		HSL_LOG_WARNING("BluetoothLEManagerConfig") <<
			"Config version " << version << " does not match expected version " <<
			BluetoothLEManagerConfig::CONFIG_VERSION << ", Using defaults.";
	}
}

// -BLEAsyncRequestManagerImpl-
/// Internal implementation of the BLE async request manager.
class BluetoothLEDeviceManagerImpl
{
public:
	BluetoothLEDeviceManagerImpl()
		: m_next_ble_device_handle(0)
	{
		m_ble_apis = new IBluetoothLEApi*[_BLEApiType_COUNT];
#ifdef _WIN32
		m_ble_apis[_BLEApiType_WinBLE] = new WinBLEApi;
#else       
		m_ble_apis[_BLEApiType_WinBLE] = nullptr;
#endif _WIN32
	}

	virtual ~BluetoothLEDeviceManagerImpl()
	{
		for (int api = 0; api < _BLEApiType_COUNT; ++api)
		{
			delete m_ble_apis[api];
		}
		delete[] m_ble_apis;
	}

	// -- System ----
	bool startup(BluetoothLEManagerConfig &m_config)
	{
		bool bSuccess = true;

		for (int api = 0; api < _BLEApiType_COUNT; ++api)
		{
			if (m_ble_apis[api]->startup())
			{
				HSL_LOG_INFO("BLEAsyncRequestManager::startup") << "Initialized BLE API";
			}
			else
			{
				HSL_LOG_ERROR("BLEAsyncRequestManager::startup") << "Failed to initialize BLE API";
				bSuccess = false;
			}
		}

		return bSuccess;
	}

	void shutdown()
	{
		// Unref any BluetoothLE devices
		freeDeviceStateList();
	}

	// -- Device Actions ----
	t_bluetoothle_device_handle openBLEDevice(const class BluetoothLEDeviceEnumerator* enumerator, BLEGattProfile **out_gatt_profile)
	{
		t_bluetoothle_device_handle handle = k_invalid_ble_device_handle;

		BluetoothLEDeviceState *state = m_ble_apis[enumerator->api_type]->openBluetoothLEDevice(enumerator, out_gatt_profile);

		if (state != nullptr)
		{
			handle = { enumerator->api_type, m_next_ble_device_handle };
			state->assignPublicHandle(handle);
			++m_next_ble_device_handle;

			m_device_state_map.insert(t_handle_ble_device_pair(state->getPublicHandle(), state));
		}

		return handle;
	}

	void closeBLEDevice(t_bluetoothle_device_handle handle)
	{
		t_ble_device_map_iterator iter = m_device_state_map.find(handle);

		if (iter != m_device_state_map.end())
		{
			BluetoothLEDeviceState *ble_device_state = iter->second;

			m_device_state_map.erase(iter);
			m_ble_apis[handle.api_type]->closeBluetoothLEDevice(ble_device_state);
		}
	}

	bool canBLEDeviceBeOpened(const class BluetoothLEDeviceEnumerator* enumerator, char *outReason, size_t bufferSize)
	{
		return m_ble_apis[enumerator->api_type]->canBluetoothLEDeviceBeOpened(enumerator, outReason, bufferSize);
	}

	bool getIsBLEDeviceOpen(t_bluetoothle_device_handle handle) const
	{
		bool bIsOpen = (m_device_state_map.find(handle) != m_device_state_map.end());

		return bIsOpen;
	}

	// -- accessors ----
	inline const IBluetoothLEApi *getBLEApiConst(eBluetoothLEApiType apiType) const { return m_ble_apis[apiType]; }
	inline IBluetoothLEApi *getBLEApi(eBluetoothLEApiType apiType) { return m_ble_apis[apiType]; }

	bool getDeviceFilter(t_bluetoothle_device_handle handle, BluetoothUUIDSet &out_service_ids)
	{
		t_ble_device_map_iterator iter = m_device_state_map.find(handle);
		bool bSuccess = false;

		if (iter != m_device_state_map.end())
		{
			bSuccess = m_ble_apis[handle.api_type]->getBluetoothLEDeviceServiceIDs(iter->second, &out_service_ids);
		}

		return bSuccess;
	}

	bool getDeviceFullPath(t_bluetoothle_device_handle handle, char *outBuffer, size_t bufferSize)
	{
		t_ble_device_map_iterator iter = m_device_state_map.find(handle);
		bool bSuccess = false;

		if (iter != m_device_state_map.end())
		{
			bSuccess = m_ble_apis[handle.api_type]->getBluetoothLEDevicePath(iter->second, outBuffer, bufferSize);
		}

		return bSuccess;
	}

	bool getBluetoothAddress(t_bluetoothle_device_handle handle, char *outBuffer, size_t bufferSize)
	{
		t_ble_device_map_iterator iter = m_device_state_map.find(handle);
		bool bSuccess = false;

		if (iter != m_device_state_map.end())
		{
			bSuccess = m_ble_apis[handle.api_type]->getBluetoothLEAddress(iter->second, outBuffer, bufferSize);
		}

		return bSuccess;
	}

	bool getGettProfile(t_bluetoothle_device_handle handle, BLEGattProfile **outGattProfile)
	{
		t_ble_device_map_iterator iter = m_device_state_map.find(handle);
		bool bSuccess = false;

		if (iter != m_device_state_map.end())
		{
			bSuccess = m_ble_apis[handle.api_type]->getBluetoothLEGattProfile(iter->second, outGattProfile);
		}

		return bSuccess;
	}

	bool getUsbDeviceIsOpen(t_bluetoothle_device_handle handle)
	{
		t_ble_device_map_iterator iter = m_device_state_map.find(handle);
		const bool bIsOpen = (iter != m_device_state_map.end());

		return bIsOpen;
	}

protected:
	void freeDeviceStateList()
	{
		for (auto it = m_device_state_map.begin(); it != m_device_state_map.end(); ++it)
		{
			m_ble_apis[it->first.api_type]->closeBluetoothLEDevice(it->second);
		}

		m_device_state_map.clear();
	}

private:
	IBluetoothLEApi **m_ble_apis;
	t_ble_device_map m_device_state_map;
	short m_next_ble_device_handle;
};

//-- public interface -----
BluetoothLEDeviceManager *BluetoothLEDeviceManager::m_instance = NULL;

BluetoothLEDeviceManager::BluetoothLEDeviceManager()
	: m_cfg()
	, m_implementation_ptr(new BluetoothLEDeviceManagerImpl())
{
	m_cfg.load();

	// Save the config back out in case it doesn't exist
	m_cfg.save();
}

BluetoothLEDeviceManager::~BluetoothLEDeviceManager()
{
	if (m_instance != NULL)
	{
		HSL_LOG_ERROR("~BLEAsyncRequestManager()") << "BLE Async Request Manager deleted without shutdown() getting called first";
	}

	if (m_implementation_ptr != nullptr)
	{
		delete m_implementation_ptr;
		m_implementation_ptr = nullptr;
	}
}

IBluetoothLEApi *BluetoothLEDeviceManager::getBLEApiInterface(eBluetoothLEApiType api)
{
	return BluetoothLEDeviceManager::getInstance()->m_implementation_ptr->getBLEApi(api);
}

bool BluetoothLEDeviceManager::startup()
{
	m_instance = this;
	return m_implementation_ptr->startup(m_cfg);
}

void BluetoothLEDeviceManager::shutdown()
{
	m_implementation_ptr->shutdown();
	m_instance = NULL;
}

// -- Device Enumeration ----
BluetoothLEDeviceEnumerator* bluetoothle_device_enumerator_allocate(eBluetoothLEApiType api)
{
	return BluetoothLEDeviceManager::getBLEApiInterface(api)->deviceEnumeratorCreate();
}

bool bluetoothle_device_enumerator_is_valid(const class BluetoothLEDeviceEnumerator* enumerator)
{
	return BluetoothLEDeviceManager::getBLEApiInterface(enumerator->api_type)->deviceEnumeratorIsValid(enumerator);
}

bool bluetoothle_device_enumerator_get_service_ids(const class BluetoothLEDeviceEnumerator* enumerator, BluetoothUUIDSet &out_service_ids)
{
	return BluetoothLEDeviceManager::getBLEApiInterface(enumerator->api_type)->deviceEnumeratorGetServiceIDs(enumerator, &out_service_ids);
}

void bluetoothle_device_enumerator_next(class BluetoothLEDeviceEnumerator* enumerator)
{
	BluetoothLEDeviceManager::getBLEApiInterface(enumerator->api_type)->deviceEnumeratorNext(enumerator);
}

void bluetoothle_device_enumerator_free(class BluetoothLEDeviceEnumerator* enumerator)
{
	BluetoothLEDeviceManager::getBLEApiInterface(enumerator->api_type)->deviceEnumeratorDispose(enumerator);
}

bool bluetoothle_device_enumerator_get_friendly_name(const class BluetoothLEDeviceEnumerator* enumerator, char* outBuffer, size_t bufferSize)
{
	return BluetoothLEDeviceManager::getBLEApiInterface(enumerator->api_type)->deviceEnumeratorGetFriendlyName(enumerator, outBuffer, bufferSize);
}

bool bluetoothle_device_enumerator_get_path(const class BluetoothLEDeviceEnumerator* enumerator, char *outBuffer, size_t bufferSize)
{
	return BluetoothLEDeviceManager::getBLEApiInterface(enumerator->api_type)->deviceEnumeratorGetPath(enumerator, outBuffer, bufferSize);
}

bool bluetoothle_device_enumerator_get_unique_id(const class BluetoothLEDeviceEnumerator* enumerator, char *outBuffer, size_t bufferSize)
{
	return BluetoothLEDeviceManager::getBLEApiInterface(enumerator->api_type)->deviceEnumeratorGetUniqueIdentifier(enumerator, outBuffer, bufferSize);
}

eBluetoothLEApiType bluetoothle_device_enumerator_get_driver_type(const class BluetoothLEDeviceEnumerator* enumerator)
{
	return BluetoothLEDeviceManager::getBLEApiInterface(enumerator->api_type)->getRuntimeBLEApiType();
}

// -- Device Actions ----
t_bluetoothle_device_handle bluetoothle_device_open(const class BluetoothLEDeviceEnumerator* enumerator, BLEGattProfile **out_gatt_profile)
{
	return BluetoothLEDeviceManager::getInstance()->getImplementation()->openBLEDevice(enumerator, out_gatt_profile);
}

void bluetoothle_device_close(t_bluetoothle_device_handle ble_device_handle)
{
	BluetoothLEDeviceManager::getInstance()->getImplementation()->closeBLEDevice(ble_device_handle);
}

bool bluetoothle_device_can_be_opened(const class BluetoothLEDeviceEnumerator* enumerator, char *outReason, size_t bufferSize)
{
	return BluetoothLEDeviceManager::getInstance()->getImplementation()->canBLEDeviceBeOpened(enumerator, outReason, bufferSize);
}

// -- Device Queries ----
bool bluetoothle_device_get_service_ids(t_bluetoothle_device_handle handle, BluetoothUUIDSet &out_service_ids)
{
	return BluetoothLEDeviceManager::getInstance()->getImplementation()->getDeviceFilter(handle, out_service_ids);
}

bool bluetoothle_device_get_full_path(t_bluetoothle_device_handle handle, char *outBuffer, size_t bufferSize)
{
	return BluetoothLEDeviceManager::getInstance()->getImplementation()->getDeviceFullPath(handle, outBuffer, bufferSize);
}

bool bluetoothle_device_get_bluetooth_address(t_bluetoothle_device_handle handle, char *outBuffer, size_t bufferSize)
{
	return BluetoothLEDeviceManager::getInstance()->getImplementation()->getBluetoothAddress(handle, outBuffer, bufferSize);
}

bool bluetoothle_device_get_gatt_profile(t_bluetoothle_device_handle handle, BLEGattProfile **outGattProfile)
{
	return BluetoothLEDeviceManager::getInstance()->getImplementation()->getGettProfile(handle, outGattProfile);
}

bool bluetoothle_device_get_is_open(t_bluetoothle_device_handle handle)
{
	return BluetoothLEDeviceManager::getInstance()->getImplementation()->getUsbDeviceIsOpen(handle);
}