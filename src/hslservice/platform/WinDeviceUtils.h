#ifndef WIN_DEVICE_ITERATOR_H
#define WIN_DEVICE_ITERATOR_H

#include <windows.h>  // Required for data types
#include <guiddef.h>
#include <setupapi.h>
#include <string>

class Win32DeviceInfoIterator
{
public:
	Win32DeviceInfoIterator(const GUID& deviceClassGUID, DWORD device_flags);
	virtual ~Win32DeviceInfoIterator();

	bool isValid() const;
	void next();

	inline const GUID& getDeviceClassGUID() const
	{
		return m_DeviceClassGUID;
	}

	inline HDEVINFO getDeviceInfoSetHandle() const
	{
		return m_DeviceInfoSetHandle;
	}

	inline SP_DEVINFO_DATA& getDeviceInfo()
	{
		return m_DeviceInfoData;
	}

private:
	const GUID& m_DeviceClassGUID;
	HDEVINFO m_DeviceInfoSetHandle;
	SP_DEVINFO_DATA m_DeviceInfoData;
	int m_MemberIndex;
	bool m_bNoMoreItems;
};

class Win32DeviceInterfaceIterator
{
public:
	Win32DeviceInterfaceIterator(Win32DeviceInfoIterator &dev_info_iterator);
	virtual ~Win32DeviceInterfaceIterator();

	bool isValid() const;
	void next();

	inline const SP_DEVICE_INTERFACE_DATA& getInterfaceData() const 
	{
		return m_interfaceData;
	}

	inline const SP_DEVICE_INTERFACE_DETAIL_DATA* getInterfaceDetailData() const
	{
		return m_interfaceDetailData;
	}

private:
	Win32DeviceInfoIterator &m_devInfoIter;
	SP_DEVICE_INTERFACE_DATA m_interfaceData;
	SP_DEVICE_INTERFACE_DETAIL_DATA *m_interfaceDetailData;
	int m_deviceInterfaceIndex;
	bool m_bNoMoreItems;
};

// -- utility methods ----
bool win32_device_fetch_device_instance_id(
	const HDEVINFO& deviceInfoSetHandle,
	const SP_DEVINFO_DATA& deviceInfoData,
	std::string& out_device_instance_id);

std::string win32_device_interface_get_symbolic_path(
	const SP_DEVICE_INTERFACE_DETAIL_DATA& interfaceDetailData);

bool win32_device_fetch_device_registry_string_property(
	const HDEVINFO& device_info_set_handle,
	const SP_DEVINFO_DATA& device_info_data,
	DWORD property_id,
	std::string& out_string);

bool win32_device_fetch_friendly_Name(
	const HDEVINFO& device_info_set_handle,
	const SP_DEVINFO_DATA& device_info_data,
	std::string& out_friendly_name);

std::string win32_get_last_error_as_string();

#endif // WIN_DEVICE_ITERATOR_H