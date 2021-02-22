#include "WinDeviceUtils.h"
#include <assert.h>

// -- Win32DeviceInfoIterator ----
Win32DeviceInfoIterator::Win32DeviceInfoIterator(const GUID& device_class_guid, DWORD device_flags)
	: m_DeviceClassGUID(device_class_guid)
	, m_DeviceInfoSetHandle(INVALID_HANDLE_VALUE)
	, m_MemberIndex(-1)
	, m_bNoMoreItems(false)
{
	m_DeviceInfoSetHandle = SetupDiGetClassDevs((LPGUID)&m_DeviceClassGUID, 0, 0, device_flags);

	ZeroMemory(&m_DeviceInfoData, sizeof(SP_DEVINFO_DATA));
	m_DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	if (isValid())
	{
		next();
	}
}

Win32DeviceInfoIterator::~Win32DeviceInfoIterator()
{
	if (m_DeviceInfoSetHandle != INVALID_HANDLE_VALUE)
	{
		SetupDiDestroyDeviceInfoList(m_DeviceInfoSetHandle);
	}
}

bool Win32DeviceInfoIterator::isValid() const
{
	return m_DeviceInfoSetHandle != INVALID_HANDLE_VALUE && !m_bNoMoreItems;
}

void Win32DeviceInfoIterator::next()
{
	if (isValid())
	{
		++m_MemberIndex;

		if (SetupDiEnumDeviceInfo(m_DeviceInfoSetHandle, m_MemberIndex, &m_DeviceInfoData) == FALSE)
		{
			m_bNoMoreItems = true;
		}
	}
}

// -- Win32DeviceInterfaceIterator ----
Win32DeviceInterfaceIterator::Win32DeviceInterfaceIterator(Win32DeviceInfoIterator &dev_info_iterator)
	: m_devInfoIter(dev_info_iterator)
	, m_interfaceDetailData(nullptr)
	, m_deviceInterfaceIndex(-1)
	, m_bNoMoreItems(false)
{
	ZeroMemory(&m_interfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
	m_interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	if (isValid())
	{
		next();
	}
}

Win32DeviceInterfaceIterator::~Win32DeviceInterfaceIterator()
{
	if (m_interfaceDetailData != nullptr)
	{
		LocalFree(m_interfaceDetailData);
		m_interfaceDetailData = NULL;
	}
}

bool Win32DeviceInterfaceIterator::isValid() const
{
	return m_devInfoIter.getDeviceInfoSetHandle() != INVALID_HANDLE_VALUE && !m_bNoMoreItems;
}

void Win32DeviceInterfaceIterator::next()
{
	if (isValid())
	{
		HDEVINFO device_info_set_handle= m_devInfoIter.getDeviceInfoSetHandle();
		SP_DEVINFO_DATA& device_info_data= m_devInfoIter.getDeviceInfo();

		++m_deviceInterfaceIndex;

		if (SetupDiEnumDeviceInterfaces(
				device_info_set_handle,
				&device_info_data,
				&m_devInfoIter.getDeviceClassGUID(),
				m_deviceInterfaceIndex,
				&m_interfaceData) == TRUE)
		{
			ULONG requiredLength = 0;
			SetupDiGetDeviceInterfaceDetail(device_info_set_handle, &m_interfaceData, NULL, 0, &requiredLength, NULL);

			if (m_interfaceDetailData != nullptr)
			{
				LocalFree(m_interfaceDetailData);
				m_interfaceDetailData = NULL;
			}

			m_interfaceDetailData =(PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LMEM_FIXED, requiredLength);
			ZeroMemory(m_interfaceDetailData, sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA));
			m_interfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

			ULONG length = requiredLength;
			if (SetupDiGetDeviceInterfaceDetail(
				device_info_set_handle,
				&m_interfaceData,
				m_interfaceDetailData,
				length,
				&requiredLength,
				NULL) == FALSE)
			{
				LocalFree(m_interfaceDetailData);
				m_interfaceDetailData = NULL;
			}
		}
		else
		{
			m_bNoMoreItems = true;
		}
	}
}

// -- utility methods ----
bool win32_device_fetch_device_instance_id(
	const HDEVINFO& deviceInfoSetHandle,
	const SP_DEVINFO_DATA& deviceInfoData,
	std::string& out_device_instance_id)
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
		out_device_instance_id = szInstanceId;
	}

	return true;
}

std::string win32_device_interface_get_symbolic_path(const SP_DEVICE_INTERFACE_DETAIL_DATA& interfaceDetailData)
{
#ifdef UNICODE
	return convert_wstring_to_string(std::wstring(interfaceDetailData.DevicePath));
#else
	return interfaceDetailData.DevicePath;
#endif
}

bool win32_device_fetch_device_registry_string_property(
	const HDEVINFO& device_info_set_handle,
	const SP_DEVINFO_DATA& device_info_data,
	DWORD property_id,
	std::string& out_string)
{
	ULONG required_length = 0;
	SP_DEVINFO_DATA deviceInfoDataCopy = device_info_data;
	SetupDiGetDeviceRegistryPropertyA(
		device_info_set_handle,
		const_cast<SP_DEVINFO_DATA*>(&device_info_data),
		property_id,
		NULL,
		NULL,
		0,
		&required_length);
	if (required_length == 0)
	{
		std::string errorString = win32_get_last_error_as_string();
		return false;
	}

	uint8_t* property_value(new uint8_t[required_length]);
	ULONG actual_length = required_length;
	DWORD property_type;
	BOOL success = SetupDiGetDeviceRegistryPropertyA(
		device_info_set_handle,
		const_cast<SP_DEVINFO_DATA*>(&device_info_data),
		property_id,
		&property_type,
		property_value,
		actual_length,
		&required_length);

	if (success)
	{
		if (property_type == REG_SZ)
		{
			out_string = (char*)property_value;
		}
		else
		{
			success = FALSE;
		}
	}

	delete[] property_value;

	return success == TRUE;
}

bool win32_device_fetch_friendly_Name(
	const HDEVINFO& device_info_set_handle,
	const SP_DEVINFO_DATA& device_info_data,
	std::string& out_friendly_name)
{
	if (win32_device_fetch_device_registry_string_property(
		device_info_set_handle,
		device_info_data,
		SPDRP_FRIENDLYNAME,
		out_friendly_name))
	{
		return true;
	}

	if (win32_device_fetch_device_registry_string_property(
		device_info_set_handle,
		device_info_data,
		SPDRP_DEVICEDESC,
		out_friendly_name))
	{
		return true;
	}

	return false;
}

std::string win32_get_last_error_as_string()
{
	DWORD errorMessageID = GetLastError();

	if (errorMessageID == 0)
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