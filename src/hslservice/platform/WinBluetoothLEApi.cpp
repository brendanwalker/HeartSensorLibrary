//-- includes -----
#include "Logger.h"
#include "BluetoothLEDeviceManager.h"
#include "WinBluetoothLEApi.h"
#include "Utility.h"

#define ANSI
#define WIN32_LEAN_AND_MEAN

#include <initguid.h>   
#include <windows.h>
#include <bthdef.h>
#include <bthledef.h>
#include <bluetoothapis.h>
#include <bluetoothleapis.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include <devguid.h>
#include <regstr.h>
#include <setupapi.h>

#include <functional>
#include <regex>
#include <assert.h>
#include <stdio.h>
#include <strsafe.h>
#include <iomanip>
#include <combaseapi.h>

#ifdef _MSC_VER
#pragma warning(disable:4996) // disable warnings about strncpy
#endif

#include "WinBluetoothLEDeviceGatt.h"
#include "WinDeviceUtils.h"


//-- definitions -----
struct WinBLEDeviceInfo
{
	std::string DevicePath;
	std::string DeviceInstanceId;
	std::string FriendlyName;
	std::string BluetoothAddress;
	BluetoothUUIDSet ServiceUUIDSet;
	BluetoothUUIDDevicePathMap ServiceDevicePathMap;

	WinBLEDeviceInfo()
		: DevicePath()
		, DeviceInstanceId()
		, FriendlyName()
		, BluetoothAddress()
		, ServiceUUIDSet()
		, ServiceDevicePathMap()
	{
	}

	bool init(
		const HDEVINFO &deviceInfoSetHandle, 
		const SP_DEVINFO_DATA &deviceInfoData,
		const SP_DEVICE_INTERFACE_DETAIL_DATA &interfaceDetailData)
	{
		bool bSuccess = true;

		DevicePath= win32_device_interface_get_symbolic_path(interfaceDetailData);

		// Get the friendly name. If it fails it is OK to leave the
		// device_info_data.friendly_name as nullptr indicating the name not read.
		win32_device_fetch_friendly_Name(deviceInfoSetHandle, deviceInfoData, FriendlyName);

		if (!verifyBLEDeviceConnected(deviceInfoSetHandle, deviceInfoData))
		{
			return false;
		}

		if (!fetchServiceUUIDs())
		{
			return false;
		}

		if (!win32_device_fetch_device_instance_id(deviceInfoSetHandle, deviceInfoData, DeviceInstanceId))
		{
			return false;
		}

		if (!parseBLEDeviceAddressFromInstanceId(DeviceInstanceId, BluetoothAddress))
		{
			return false;
		}

		return bSuccess;
	}

private: 
	static bool parseBLEDeviceAddressFromInstanceId(
		const std::string &DeviceInstanceId,
		std::string &BluetoothAddress)
	{
		// Extract the bluetooth device address from the device instance ID string, 
		// bthledevice#{00001800-0000-1000-8000-00805f9b34fb}_dev_ma&polar_electro_oy_mo&h10_fw&5.0.0_cb37c3e454b5#9&82b4232&0&0001#{6e3bb679-4372-40c8-9eaa-4509df260cd8}
		std::regex address_regex(R"(_([0-9A-F]{12})[#\\])", std::regex_constants::ECMAScript | std::regex_constants::icase);
		std::smatch match_results;
		if (std::regex_search(DeviceInstanceId, match_results, address_regex) && match_results.size() == 2)
		{
			BluetoothAddress = match_results[1];
			return true;
		}

		return false;
	}

	bool verifyBLEDeviceConnected(
		const HDEVINFO &deviceInfoSetHandle,
		const SP_DEVINFO_DATA &deviceInfoData)
	{
		UINT32 PropertyValue = 0;
		DWORD propertyDataType;
		DWORD requiredBufferSize = 0;

		SP_DEVINFO_DATA deviceInfoDataCopy = deviceInfoData;
		BOOL success =
			SetupDiGetDevicePropertyW(
				deviceInfoSetHandle,
				&deviceInfoDataCopy,
				&DEVPKEY_Device_DevNodeStatus,
				&propertyDataType,
				reinterpret_cast<PBYTE>(&PropertyValue),
				sizeof(PropertyValue),
				&requiredBufferSize,
				0);

		if (success == FALSE || propertyDataType != DEVPROP_TYPE_UINT32)
		{
			return false;
		}

		bool bIsConnected = !(PropertyValue & DN_DEVICE_DISCONNECTED);

		return bIsConnected;
	}

	bool fetchServiceUUIDs()
	{
		bool bSuccess = true;

		HANDLE deviceHandle = CreateFile(
			DevicePath.c_str(),
			GENERIC_WRITE | GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);
		if (deviceHandle == INVALID_HANDLE_VALUE)
		{
			bSuccess = false;
		}

		USHORT serviceBufferRequiredCount = 0;
		if (bSuccess)
		{
			HRESULT hr = BluetoothGATTGetServices(
				deviceHandle,
				0,
				NULL,
				&serviceBufferRequiredCount,
				BLUETOOTH_GATT_FLAG_NONE);

			if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA))
			{
				bSuccess = false;
			}
		}

		// Fetch the service list from the windows BluetoothLE API.
		// Create a WinBLEGattService wrapper instance for each entry in the service list.
		if (bSuccess)
		{
			BTH_LE_GATT_SERVICE *serviceBufferWinAPI = 
				(BTH_LE_GATT_SERVICE *)malloc(sizeof(BTH_LE_GATT_SERVICE) * serviceBufferRequiredCount);
			assert(serviceBufferWinAPI != nullptr);
			memset(serviceBufferWinAPI, 0, sizeof(BTH_LE_GATT_SERVICE) * serviceBufferRequiredCount);

			USHORT servicesBufferActualCount = serviceBufferRequiredCount;
			HRESULT hr = BluetoothGATTGetServices(
				deviceHandle,
				servicesBufferActualCount,
				serviceBufferWinAPI,
				&serviceBufferRequiredCount,
				BLUETOOTH_GATT_FLAG_NONE);

			if (hr == S_OK)
			{
				for (USHORT serviceIndex = 0; serviceIndex < servicesBufferActualCount; ++serviceIndex)
				{
					const BTH_LE_GATT_SERVICE &serviceWinAPI = serviceBufferWinAPI[serviceIndex];
					
					ServiceUUIDSet.addUUID(WinBluetoothUUID(serviceWinAPI.ServiceUuid));
				}
			}
			else
			{
				bSuccess = false;
			}

			if (serviceBufferWinAPI != nullptr)
			{
				free(serviceBufferWinAPI);
				serviceBufferWinAPI = nullptr;
			}
		}

		if (deviceHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(deviceHandle);
			deviceHandle = INVALID_HANDLE_VALUE;
		}

		return bSuccess;
	}
};

class WinBLEDeviceEnumerator : public BluetoothLEDeviceEnumerator
{
public:
	WinBLEDeviceEnumerator()
	{
		buildBLEDeviceList();
		if (m_bleDeviceList.size() > 0)
		{
			buildBLEServiceInterfaceList();
		}

		api_type= _BLEApiType_WinBLE;
		BluetoothLEDeviceEnumerator::device_index= -1;
		next();
	}

	bool isValid() const
	{
		return BluetoothLEDeviceEnumerator::device_index < (int)m_bleDeviceList.size();
	}

	void next()
	{
		++BluetoothLEDeviceEnumerator::device_index;
	}

	inline std::string getDevicePath() const
	{
		return (isValid()) ? m_bleDeviceList[BluetoothLEDeviceEnumerator::device_index]->DevicePath : std::string();
	}

	inline std::string getDeviceInstanceId() const 
	{
		return (isValid()) ? m_bleDeviceList[BluetoothLEDeviceEnumerator::device_index]->DeviceInstanceId : std::string();
	}

	inline std::string getFriendlyName() const
	{
		return (isValid()) ? m_bleDeviceList[BluetoothLEDeviceEnumerator::device_index]->FriendlyName : std::string();
	}

	inline std::string getBluetoothAddress() const
	{
		return (isValid()) ? m_bleDeviceList[BluetoothLEDeviceEnumerator::device_index]->BluetoothAddress : std::string();
	}

	inline BluetoothUUIDSet getServiceUUIDSet() const
	{
		return isValid() ? m_bleDeviceList[BluetoothLEDeviceEnumerator::device_index]->ServiceUUIDSet : BluetoothUUIDSet();
	}

	inline BluetoothUUIDDevicePathMap getServiceDevicePathTable() const
	{
		return isValid() ? m_bleDeviceList[BluetoothLEDeviceEnumerator::device_index]->ServiceDevicePathMap : BluetoothUUIDDevicePathMap();
	}

protected:
	void buildBLEDeviceList()
	{
		for (Win32DeviceInfoIterator dev_iter(GUID_BLUETOOTHLE_DEVICE_INTERFACE, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
			dev_iter.isValid();
			dev_iter.next())
		{
			for (Win32DeviceInterfaceIterator iface_iter(dev_iter); iface_iter.isValid(); iface_iter.next())
			{
				const SP_DEVICE_INTERFACE_DETAIL_DATA* iface_detail_data= iface_iter.getInterfaceDetailData();

				if (iface_detail_data != nullptr)
				{
					auto newDeviceInfo = std::make_unique<WinBLEDeviceInfo>();
					const SP_DEVINFO_DATA &dev_info_data= dev_iter.getDeviceInfo();

					// Fetch the device ID string
					TCHAR dev_id[MAX_DEVICE_ID_LEN];
					if (CM_Get_Device_ID(dev_info_data.DevInst, dev_id, ARRAY_SIZE(dev_id), 0) != CR_SUCCESS)
					{
						continue;
					}
					
					// Skip any duplicate device entries
					if (m_bleDeviceInstTable.find(std::string(dev_id)) != m_bleDeviceInstTable.end())
					{
						continue;
					}

					if (newDeviceInfo->init(dev_iter.getDeviceInfoSetHandle(), dev_iter.getDeviceInfo(), *iface_detail_data))
					{
						const int ble_device_list_index= (int)m_bleDeviceList.size();

						m_bleDeviceList.push_back(std::move(newDeviceInfo));
						m_bleDeviceInstTable.insert(std::make_pair(std::string(dev_id), ble_device_list_index));
					}
				}
			}
		}
	}

	void buildBLEServiceInterfaceList()
	{
		for (Win32DeviceInfoIterator dev_iter(GUID_BLUETOOTH_GATT_SERVICE_DEVICE_INTERFACE, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
			dev_iter.isValid();
			dev_iter.next())
		{
			HDEVINFO dev_info_set_handle= dev_iter.getDeviceInfoSetHandle();

			for (Win32DeviceInterfaceIterator iface_iter(dev_iter); iface_iter.isValid(); iface_iter.next())
			{
				const SP_DEVICE_INTERFACE_DETAIL_DATA* iface_detail_data = iface_iter.getInterfaceDetailData();

				if (iface_detail_data != nullptr)
				{
					const SP_DEVINFO_DATA& dev_info_data = dev_iter.getDeviceInfo();

					// Get the symbolic path of the Service path
					const std::string interface_symbolic_path= win32_device_interface_get_symbolic_path(*iface_detail_data);

					// Extract the bluetooth device address from the device instance ID string, 
					// \\?\bthledevice#{00001800-0000-1000-8000-00805f9b34fb}_dev_ma&polar_electro_oy_mo&h10_fw&5.0.0_cb37c3e454b5#9&82b4232&1&0001#{6e3bb679-4372-40c8-9eaa-4509df260cd8}
					std::regex address_regex(R"(([0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}))", std::regex_constants::ECMAScript | std::regex_constants::icase);
					std::smatch match_results;
					if (std::regex_search(interface_symbolic_path, match_results, address_regex) && match_results.size() < 2)
					{
						continue;
					}

					std::string uuid_string = match_results[1];
					BluetoothUUID serviceUUID(uuid_string);

					// Get the parent device instance
					DEVINST parent_dev_instance;
					if (CM_Get_Parent(&parent_dev_instance, dev_info_data.DevInst, 0) != CR_SUCCESS)
					{
						continue;
					}

					TCHAR parent_dev_id[MAX_DEVICE_ID_LEN];
					if (CM_Get_Device_ID(parent_dev_instance, parent_dev_id, ARRAY_SIZE(parent_dev_id), 0) != CR_SUCCESS)
					{
						continue;
					}

					// Look up the parent device we found earlier
					auto win_ble_dev_iter = m_bleDeviceInstTable.find(std::string(parent_dev_id));
					if (win_ble_dev_iter != m_bleDeviceInstTable.end())
					{
						const int ble_device_list_index= win_ble_dev_iter->second;

						// Finally, make a mapping from service UUID to symbolic path for the device
						m_bleDeviceList[ble_device_list_index]->ServiceDevicePathMap.insert(
							std::make_pair(serviceUUID, interface_symbolic_path));
					}
				}
			}
		}
	}

private:
	std::vector<std::unique_ptr<WinBLEDeviceInfo>> m_bleDeviceList;
	std::map<std::string, int> m_bleDeviceInstTable;
};

//-- WinBLEDeviceState -----
WinBLEDeviceState::WinBLEDeviceState(const WinBLEDeviceEnumerator* enumerator)
	: BluetoothLEDeviceState()
{
	devicePath = enumerator->getDevicePath();
	deviceInstanceId = enumerator->getDeviceInstanceId();
	friendlyName = enumerator->getFriendlyName();
	bluetoothAddress = enumerator->getBluetoothAddress();
	deviceHandle = INVALID_HANDLE_VALUE;
	m_serviceUUIDSet = enumerator->getServiceUUIDSet();
	m_serviceDevicePathMap = enumerator->getServiceDevicePathTable();
}

WinBLEDeviceState::~WinBLEDeviceState()
{
	closeDevice();
}

bool WinBLEDeviceState::openDevice(BLEGattProfile **out_gatt_profile)
{
	// Open the main device handle
	deviceHandle = CreateFile(
		devicePath.c_str(),
		GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (deviceHandle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// Open each service device handle
	for (auto iter = m_serviceDevicePathMap.begin(); iter != m_serviceDevicePathMap.end(); ++iter)
	{		
		HANDLE serviceDeviceHandle = CreateFile(
			iter->second.c_str(),
			GENERIC_WRITE | GENERIC_READ,
			FILE_SHARE_WRITE | FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if (serviceDeviceHandle == INVALID_HANDLE_VALUE)
		{
			// Fall back to read only
			serviceDeviceHandle = CreateFile(
				iter->second.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ,
				NULL,
				OPEN_EXISTING,
				0,
				NULL);
		}

		if (serviceDeviceHandle != INVALID_HANDLE_VALUE)
		{
			m_serviceDeviceHandleMap.insert(std::make_pair(iter->first, serviceDeviceHandle));
		}
		//else
		//{
		//	return false;
		//}
	}

	gattProfile = new WinBLEGattProfile(this);
	if (!static_cast<WinBLEGattProfile *>(gattProfile)->fetchServices())
	{		
		closeDevice();
	}

	if (out_gatt_profile != nullptr)
	{
		*out_gatt_profile = gattProfile;
	}

	return true;
}

void WinBLEDeviceState::closeDevice()
{
	HSL_LOG_INFO("WinBLEApi::closeBluetoothLEDevice") << "Close BLE device handle " << deviceHandle;

	if (gattProfile != nullptr)
	{
		static_cast<WinBLEGattProfile*>(gattProfile)->freeServices();

		delete gattProfile;
		gattProfile = nullptr;
	}

	for (auto iter = m_serviceDeviceHandleMap.begin(); iter != m_serviceDeviceHandleMap.end(); ++iter)
	{
		if (iter->second != INVALID_HANDLE_VALUE)
		{
			CloseHandle(iter->second);
		}
	}
	m_serviceDeviceHandleMap.clear();

	if (deviceHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(deviceHandle);
		deviceHandle = INVALID_HANDLE_VALUE;
	}
}

bool WinBLEDeviceState::getServiceInterfaceDevicePath(
	const BluetoothUUID& service_uuid,
	std::string& out_symbolic_path) const
{
	auto iter = m_serviceDevicePathMap.find(service_uuid);
	if (iter != m_serviceDevicePathMap.end())
	{
		out_symbolic_path = iter->second;
		return true;
	}

	return false;
}

HANDLE WinBLEDeviceState::getServiceDeviceHandle(
	const BluetoothUUID& service_uuid) const
{
	auto iter = m_serviceDeviceHandleMap.find(service_uuid);
	if (iter != m_serviceDeviceHandleMap.end())
	{
		return iter->second;
	}

	return nullptr;
}

//-- public interface -----

//-- WinBLEApi -----
WinBLEApi::WinBLEApi() : IBluetoothLEApi()
{
}

WinBLEApi *WinBLEApi::getInterface()
{
	return BluetoothLEDeviceManager::getTypedBLEApiInterface<WinBLEApi>();
}

bool WinBLEApi::startup()
{
	return true;
}

void WinBLEApi::shutdown()
{
}

BluetoothLEDeviceEnumerator* WinBLEApi::deviceEnumeratorCreate()
{
	WinBLEDeviceEnumerator *enumerator= new WinBLEDeviceEnumerator();

	return enumerator;
}

bool WinBLEApi::deviceEnumeratorIsValid(const BluetoothLEDeviceEnumerator* enumerator)
{
	const WinBLEDeviceEnumerator *winble_enumerator = static_cast<const WinBLEDeviceEnumerator *>(enumerator);

	return winble_enumerator != nullptr && winble_enumerator->isValid();
}

bool WinBLEApi::deviceEnumeratorGetServiceIDs(const BluetoothLEDeviceEnumerator* enumerator, BluetoothUUIDSet *out_service_ids) const
{
	const WinBLEDeviceEnumerator *winble_enumerator = static_cast<const WinBLEDeviceEnumerator *>(enumerator);
	bool bSuccess = false;

	if (winble_enumerator != nullptr)
	{
		*out_service_ids = winble_enumerator->getServiceUUIDSet();
		bSuccess = true;
	}

	return bSuccess;
}

bool WinBLEApi::deviceEnumeratorGetFriendlyName(const BluetoothLEDeviceEnumerator* enumerator, char* outBuffer, size_t bufferSize) const
{
	const WinBLEDeviceEnumerator* winble_enumerator = static_cast<const WinBLEDeviceEnumerator*>(enumerator);
	bool bSuccess = false;

	if (winble_enumerator != nullptr)
	{
		std::string friendlyName = winble_enumerator->getFriendlyName();

		bSuccess = SUCCEEDED(StringCchCopy(outBuffer, bufferSize, friendlyName.c_str()));
	}

	return bSuccess;
}

bool WinBLEApi::deviceEnumeratorGetPath(const BluetoothLEDeviceEnumerator* enumerator, char *outBuffer, size_t bufferSize) const
{
	const WinBLEDeviceEnumerator *winble_enumerator = static_cast<const WinBLEDeviceEnumerator *>(enumerator);
	bool bSuccess = false;

	if (winble_enumerator != nullptr)
	{
		std::string devicePath= winble_enumerator->getDevicePath();

		bSuccess = SUCCEEDED(StringCchCopy(outBuffer, bufferSize, devicePath.c_str()));
	}

	return bSuccess;
}

bool WinBLEApi::deviceEnumeratorGetUniqueIdentifier(const BluetoothLEDeviceEnumerator* enumerator, char *outBuffer, size_t bufferSize) const
{
	const WinBLEDeviceEnumerator *winble_enumerator = static_cast<const WinBLEDeviceEnumerator *>(enumerator);
	bool bSuccess = false;

	if (winble_enumerator != nullptr)
	{
		std::string deviceIdentifier = winble_enumerator->getDeviceInstanceId();

		bSuccess = SUCCEEDED(StringCchCopy(outBuffer, bufferSize, deviceIdentifier.c_str()));
	}

	return bSuccess;
}

void WinBLEApi::deviceEnumeratorNext(BluetoothLEDeviceEnumerator* enumerator)
{
	WinBLEDeviceEnumerator *winble_enumerator = static_cast<WinBLEDeviceEnumerator *>(enumerator);

	if (deviceEnumeratorIsValid(winble_enumerator))
	{
		winble_enumerator->next();
	}
}

void WinBLEApi::deviceEnumeratorDispose(BluetoothLEDeviceEnumerator* enumerator)
{
	if (enumerator != nullptr)
	{
		delete enumerator;
	}
}

BluetoothLEDeviceState *WinBLEApi::openBluetoothLEDevice(
	const BluetoothLEDeviceEnumerator* enumerator,
	BLEGattProfile **out_gatt_profile)
{
	const WinBLEDeviceEnumerator *winble_enumerator = static_cast<const WinBLEDeviceEnumerator *>(enumerator);
	WinBLEDeviceState *winble_device_state = nullptr;

	if (deviceEnumeratorIsValid(enumerator))
	{
		bool bOpened = false;
		
		winble_device_state = new WinBLEDeviceState(winble_enumerator);

		if (!winble_device_state->openDevice(out_gatt_profile))
		{
			closeBluetoothLEDevice(winble_device_state);
			winble_device_state= nullptr;
		}
	}

	return winble_device_state;
}

void WinBLEApi::closeBluetoothLEDevice(BluetoothLEDeviceState* device_state)
{
	if (device_state != nullptr)
	{
		WinBLEDeviceState *winble_device_state = static_cast<WinBLEDeviceState *>(device_state);

		winble_device_state->closeDevice();

		delete winble_device_state;
	}
}

bool WinBLEApi::canBluetoothLEDeviceBeOpened(const BluetoothLEDeviceEnumerator* enumerator, char *outReason, size_t bufferSize)
{
	const WinBLEDeviceEnumerator *winble_enumerator = static_cast<const WinBLEDeviceEnumerator *>(enumerator);
	bool bCanBeOpened= false;

	if (deviceEnumeratorIsValid(enumerator))
	{	
		std::string device_path= winble_enumerator->getDevicePath();
		HANDLE device_handle = CreateFile(
				device_path.c_str(),
				GENERIC_WRITE | GENERIC_READ,
				FILE_SHARE_WRITE | FILE_SHARE_READ,
				NULL,
				OPEN_EXISTING,
				0, //FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
				NULL);

		// Can be opened if we can open the device now or it's already opened
		if (device_handle != INVALID_HANDLE_VALUE)
		{
			strncpy(outReason, "SUCCESS(can be opened)", bufferSize);
			CloseHandle(device_handle);
			bCanBeOpened= true;
		}
		else
		{
			if (GetLastError() == ERROR_SHARING_VIOLATION)
			{
				strncpy(outReason, "SUCCESS(already opened)", bufferSize);
				bCanBeOpened= true;
			}
			else
			{
				std::string error_string= win32_get_last_error_as_string();
				strncpy(outReason, error_string.c_str(), bufferSize);
			}
		}
	}

	return bCanBeOpened;
}

bool WinBLEApi::getBluetoothLEDeviceServiceIDs(const BluetoothLEDeviceState* device_state, BluetoothUUIDSet *out_service_ids) const
{
	bool bSuccess = false;

	if (device_state != nullptr)
	{
		const WinBLEDeviceState *winble_device_state = static_cast<const WinBLEDeviceState *>(device_state);

		*out_service_ids = winble_device_state->getServiceUUIDSet();
		bSuccess = true;
	}

	return bSuccess;
}

bool WinBLEApi::getBluetoothLEDevicePath(const BluetoothLEDeviceState* device_state, char *outBuffer, size_t bufferSize) const
{
	bool bSuccess = false;

	if (device_state != nullptr)
	{
		const WinBLEDeviceState *winble_device_state = static_cast<const WinBLEDeviceState *>(device_state);

		strncpy(outBuffer, winble_device_state->getDevicePath().c_str(), bufferSize);
		bSuccess = true;
	}

	return bSuccess;
}

bool WinBLEApi::getBluetoothLEAddress(const BluetoothLEDeviceState* device_state, char *outBuffer, size_t bufferSize) const
{
	bool bSuccess = false;

	if (device_state != nullptr)
	{
		const WinBLEDeviceState *winble_device_state = static_cast<const WinBLEDeviceState *>(device_state);

		strncpy(outBuffer, winble_device_state->getBluetoothAddress().c_str(), bufferSize);
		bSuccess = true;
	}

	return bSuccess;
}

bool WinBLEApi::getBluetoothLEGattProfile(const BluetoothLEDeviceState* device_state, BLEGattProfile **outGattProfile) const
{
	bool bSuccess = false;

	if (device_state != nullptr)
	{
		const WinBLEDeviceState *winble_device_state = static_cast<const WinBLEDeviceState *>(device_state);

		assert(outGattProfile != nullptr);
		*outGattProfile = winble_device_state->getGattProfile();
		bSuccess = true;
	}

	return bSuccess;
}