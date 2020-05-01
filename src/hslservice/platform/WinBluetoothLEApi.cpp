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

//-- public interface -----

//-- definitions -----
struct WinBLEDeviceInfo
{
	std::string DevicePath;
	std::string DeviceInstanceId;
    std::string FriendlyName;
    std::string BluetoothAddress;
	BluetoothUUIDSet ServiceUUIDSet;
    bool Visible;
    bool Authenticated;
    bool Connected;

	WinBLEDeviceInfo()
		: DevicePath()
		, DeviceInstanceId()
        , FriendlyName()
        , BluetoothAddress()
		, ServiceUUIDSet()
        , Visible(false)
        , Authenticated(false)
        , Connected(false)
	{
	}

	bool init(
		const HDEVINFO &deviceInfoSetHandle, 
		const SP_DEVINFO_DATA &deviceInfoData,
		const SP_DEVICE_INTERFACE_DETAIL_DATA &interfaceDetailData)
	{
		bool bSuccess = true;

		char raw_device_path[512];
#ifdef UNICODE
		wcstombs(raw_device_path, interfaceDetailData.DevicePath, (unsigned)_countof(raw_device_path));
#else
		strncpy_s(raw_device_path, interfaceDetailData.DevicePath, (unsigned)_countof(raw_device_path));
#endif
		DevicePath = raw_device_path;

		// Get the friendly name. If it fails it is OK to leave the
		// device_info_data.friendly_name as nullopt indicating the name not read.
		fetchBLEDeviceFriendlyName(deviceInfoSetHandle, deviceInfoData);

		if (!fetchBLEDeviceInstanceId(deviceInfoSetHandle, deviceInfoData))
		{
			return false;
		}

		if (!parseBLEDeviceAddressFromInstanceId(DeviceInstanceId, BluetoothAddress))
		{
			return false;
		}

		if (!fetchBLEDeviceStatus(deviceInfoSetHandle, deviceInfoData))
		{
			return false;
		}

		if (!fetchServiceUUIDs())
		{
			return false;
		}

		return bSuccess;
	}

private: 
	void fetchBLEDeviceFriendlyName(
		const HDEVINFO &deviceInfoSetHandle,
		const SP_DEVINFO_DATA &deviceInfoData)
	{
		char szFriendlyName[128];
		DWORD kFriendlyNameBufferSize = (DWORD)sizeof(szFriendlyName);
		DWORD propertyDataType;
		DWORD requiredBufferSize = 0;

		SP_DEVINFO_DATA deviceInfoDataCopy = deviceInfoData;
		BOOL success =
			SetupDiGetDeviceRegistryPropertyA(
				deviceInfoSetHandle,
				&deviceInfoDataCopy,
				SPDRP_FRIENDLYNAME,
				&propertyDataType,
				reinterpret_cast<PBYTE>(szFriendlyName),
				kFriendlyNameBufferSize,
				&requiredBufferSize);
		assert(requiredBufferSize <= kFriendlyNameBufferSize);

		if (success == TRUE)
		{
			FriendlyName = szFriendlyName;
		}
	}

	bool fetchBLEDeviceInstanceId(
		const HDEVINFO &deviceInfoSetHandle,
		const SP_DEVINFO_DATA &deviceInfoData)
	{
		char szInstanceId[512];
		DWORD kInstanceIdBufferSize = (DWORD)sizeof(szInstanceId);
		DWORD requiredBufferSize = 0;

		SP_DEVINFO_DATA deviceInfoDataCopy = deviceInfoData;
		BOOL success = 
			SetupDiGetDeviceInstanceIdA(
				deviceInfoSetHandle,
				&deviceInfoDataCopy,
				szInstanceId,
				kInstanceIdBufferSize,
				&requiredBufferSize);
		assert(requiredBufferSize <= kInstanceIdBufferSize);

		if (success == TRUE)
		{
			DeviceInstanceId = szInstanceId;
		}

		return true;
	}

	static bool parseBLEDeviceAddressFromInstanceId(
		const std::string &DeviceInstanceId,
		std::string &BluetoothAddress)
	{
		// Extract the bluetooth device address from the device instance ID string, 
		// A Bluetooth device instance ID often has the following format (under Win8+):
		// BTHLE\DEV_BC6A29AB5FB0\8&31038925&0&BC6A29AB5FB0
		// However, they have also been seen with the following, more expanded format:
		// BTHLEDEVICE\{0000180F-0000-1000-8000-00805F9B34FB}_DEV_VID&01000A_PID&
		// 014C_REV&0100_818B4B0BACE6\8&4C387F7&0&0020

		std::regex address_regex("(_([0-9A-F]{12})\\)", std::regex_constants::ECMAScript | std::regex_constants::icase);
		std::smatch match_results;
		if (std::regex_search(DeviceInstanceId, match_results, address_regex))
		{
			BluetoothAddress = match_results[0];
		}

		return false;
	}

	bool fetchBLEDeviceStatus(
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

		Connected = !(PropertyValue & DN_DEVICE_DISCONNECTED);
		// Windows 8 exposes BLE devices only if they are visible and paired. This
		// might change in the future if Windows offers a public API for discovering
		// and pairing BLE devices.
		Visible = true;
		Authenticated = true;

		return true;
	}

	bool fetchServiceUUIDs()
	{
		bool bSuccess = false;

		HANDLE deviceHandle = CreateFile(
			DevicePath.c_str(),
			GENERIC_WRITE | GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if (deviceHandle != INVALID_HANDLE_VALUE)
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

		api_type= _BLEApiType_WinBLE;
		BluetoothLEDeviceEnumerator::device_index= -1;
		next();
	}

	bool isValid() const
	{
		return BluetoothLEDeviceEnumerator::device_index < (int)m_deviceInfoList.size();
	}

	void next()
	{
		++BluetoothLEDeviceEnumerator::device_index;
	}

	inline std::string getDevicePath() const
	{
		return (isValid()) ? m_deviceInfoList[BluetoothLEDeviceEnumerator::device_index].DevicePath : std::string();
	}

	inline std::string getDeviceInstanceId() const 
	{
		return (isValid()) ? m_deviceInfoList[BluetoothLEDeviceEnumerator::device_index].DeviceInstanceId : std::string();
	}

	inline std::string getFriendlyName() const
	{
		return (isValid()) ? m_deviceInfoList[BluetoothLEDeviceEnumerator::device_index].FriendlyName : std::string();
	}

	inline std::string getBluetoothAddress() const
	{
		return (isValid()) ? m_deviceInfoList[BluetoothLEDeviceEnumerator::device_index].BluetoothAddress : std::string();
	}

	inline BluetoothUUIDSet getServiceUUIDSet() const
	{
		return isValid() ? m_deviceInfoList[BluetoothLEDeviceEnumerator::device_index].ServiceUUIDSet : BluetoothUUIDSet();
	}

	inline bool getIsVisible() const
	{
		return (isValid()) ? m_deviceInfoList[BluetoothLEDeviceEnumerator::device_index].Visible : false;
	}

	inline bool getIsAuthenticated() const
	{
		return (isValid()) ? m_deviceInfoList[BluetoothLEDeviceEnumerator::device_index].Authenticated : false;
	}

	inline bool getIsConnected() const
	{
		return (isValid()) ? m_deviceInfoList[BluetoothLEDeviceEnumerator::device_index].Connected : false;
	}

protected:
	void buildBLEDeviceList()
	{
		HDEVINFO deviceInfoSetHandle = 
			SetupDiGetClassDevs(
				&GUID_BLUETOOTH_GATT_SERVICE_DEVICE_INTERFACE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

		SP_DEVINFO_DATA deviceInfoData;
		ZeroMemory(&deviceInfoData, sizeof(SP_DEVINFO_DATA));
		deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

		DWORD deviceInfoSetIndex = 0;
		while(SetupDiEnumDeviceInfo(deviceInfoSetHandle, deviceInfoSetIndex, &deviceInfoData))
		{
			DWORD deviceInterfaceIndex = 0;

			SP_DEVICE_INTERFACE_DATA interfaceData;
			ZeroMemory(&interfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
			interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

			while (SetupDiEnumDeviceInterfaces(
				deviceInfoSetHandle,
				NULL,
				&GUID_BLUETOOTH_GATT_SERVICE_DEVICE_INTERFACE,
				deviceInterfaceIndex,
				&interfaceData) == TRUE)
			{
				ULONG requiredLength = 0;
				SetupDiGetDeviceInterfaceDetail(deviceInfoSetHandle, &interfaceData, NULL, 0, &requiredLength, NULL);
				PSP_DEVICE_INTERFACE_DETAIL_DATA interfaceDetailData =
					(PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LMEM_FIXED, requiredLength);

				if (interfaceDetailData != NULL)
				{
					SP_DEVINFO_DATA deviceInfoData = { 0 };
					deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

					interfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
					ULONG length = requiredLength;

					if (SetupDiGetDeviceInterfaceDetail(
						deviceInfoSetHandle,
						&interfaceData,
						interfaceDetailData,
						length,
						&requiredLength,
						&deviceInfoData) == TRUE)
					{
						WinBLEDeviceInfo newDeviceInfo;

						if (newDeviceInfo.init(deviceInfoSetHandle, deviceInfoData, *interfaceDetailData))
						{
							m_deviceInfoList.push_back(newDeviceInfo);
						}
					}

					if (interfaceDetailData != NULL)
					{
						LocalFree(interfaceDetailData);
						interfaceDetailData = NULL;
					}
				}

				++deviceInterfaceIndex;
			}

			++deviceInfoSetIndex;
		}

		if (deviceInfoSetHandle != INVALID_HANDLE_VALUE)
		{
			SetupDiDestroyDeviceInfoList(deviceInfoSetHandle);
		}
	}

private:
	std::vector<WinBLEDeviceInfo> m_deviceInfoList;
};

//-- private methods -----
static std::string GetLastErrorAsString();

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
}

WinBLEDeviceState::~WinBLEDeviceState()
{
	closeDevice();
}

bool WinBLEDeviceState::openDevice(BLEGattProfile **out_gatt_profile)
{
	bool bSuccess = true;

	deviceHandle = CreateFile(
		devicePath.c_str(),
		GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		0, //FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		NULL);

	if (deviceHandle != INVALID_HANDLE_VALUE)
	{
		gattProfile = new WinBLEGattProfile(this);

		if (static_cast<WinBLEGattProfile *>(gattProfile)->fetchServices())
		{
			if (out_gatt_profile != nullptr)
			{
				*out_gatt_profile = gattProfile;
			}
		}
		else
		{
			closeDevice();
		}
	}
	else
	{
		bSuccess = false;
	}

	return bSuccess;
}

void WinBLEDeviceState::closeDevice()
{
	if (deviceHandle != INVALID_HANDLE_VALUE)
	{
		HSL_LOG_INFO("WinBLEApi::closeBluetoothLEDevice") << "Close BLE device handle " << deviceHandle;

		if (gattProfile != nullptr)
		{
			static_cast<WinBLEGattProfile *>(gattProfile)->freeServices();

			delete gattProfile;
			gattProfile = nullptr;
		}

		CloseHandle(deviceHandle);
		deviceHandle = INVALID_HANDLE_VALUE;
	}
}

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
				std::string error_string= GetLastErrorAsString();
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

static std::string GetLastErrorAsString()
{
	DWORD errorMessageID = GetLastError();

	if(errorMessageID == 0)
			return std::string(); //No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, 
			errorMessageID, 
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
			(LPSTR)&messageBuffer, 
			0, 
			NULL);
	std::string message(messageBuffer, size);

	LocalFree(messageBuffer);

	return message;
}