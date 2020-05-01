#ifndef BLE_DEVICE_MANAGER_H
#define BLE_DEVICE_MANAGER_H

//-- includes -----
#include "HSLConfig.h"
#include <functional>
#include "BluetoothLEApiInterface.h"

//-- definitions -----
class BluetoothLEManagerConfig : public HSLConfig
{
public:
	static const int CONFIG_VERSION;

	BluetoothLEManagerConfig(const std::string &fnamebase = "BluetoothLEManagerConfig");

	virtual const configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config &pt);

	long version;
};

/// Manages async control and bulk transfer requests to usb devices via selected usb api.
class BluetoothLEDeviceManager
{
public:
	BluetoothLEDeviceManager();
	virtual ~BluetoothLEDeviceManager();

	static inline BluetoothLEDeviceManager *getInstance()
	{
		return m_instance;
	}

	inline class BluetoothLEDeviceManagerImpl *getImplementation()
	{
		return m_implementation_ptr;
	}

	static IBluetoothLEApi *getBLEApiInterface(eBluetoothLEApiType api);

	template <class t_usb_api>
	static t_usb_api *getTypedBLEApiInterface()
	{
		IBluetoothLEApi *api= getBLEApiInterface(t_usb_api::getStaticBLEApiType());
		assert(api->getRuntimeBLEApiType() == t_usb_api::getStaticBLEApiType());

		return static_cast<t_usb_api *>(api);
	}

	// -- System ----
	bool startup();
	void shutdown();

private:
	/// Configuration settings used by the BLE manager
	BluetoothLEManagerConfig m_cfg;

	/// private implementation
	class BluetoothLEDeviceManagerImpl *m_implementation_ptr;

	/// Singleton instance of the class
	/// Assigned in startup, cleared in teardown
	static BluetoothLEDeviceManager *m_instance;
};

// -- Device Enumeration ----
class BluetoothLEDeviceEnumerator* bluetoothle_device_enumerator_allocate(eBluetoothLEApiType api=_BLEApiType_WinBLE);
bool bluetoothle_device_enumerator_is_valid(const class BluetoothLEDeviceEnumerator* enumerator);
bool bluetoothle_device_enumerator_get_service_ids(const class BluetoothLEDeviceEnumerator* enumerator, BluetoothUUIDSet &outDeviceInfo);
void bluetoothle_device_enumerator_next(class BluetoothLEDeviceEnumerator* enumerator);
void bluetoothle_device_enumerator_free(class BluetoothLEDeviceEnumerator* enumerator);
bool bluetoothle_device_enumerator_get_path(const class BluetoothLEDeviceEnumerator* enumerator, char *outBuffer, size_t bufferSize);
bool bluetoothle_device_enumerator_get_unique_id(const class BluetoothLEDeviceEnumerator* enumerator, char *outBuffer, size_t bufferSize);
eBluetoothLEApiType bluetoothle_device_enumerator_get_driver_type(const class BluetoothLEDeviceEnumerator* enumerator);

// -- Device Actions ----
t_bluetoothle_device_handle bluetoothle_device_open(const class BluetoothLEDeviceEnumerator* enumerator, BLEGattProfile **outGattProfile);
void bluetoothle_device_close(t_bluetoothle_device_handle ble_device_handle);

// -- Device Queries ----
bool bluetoothle_device_can_be_opened(const class BluetoothLEDeviceEnumerator* enumerator, char *outReason, size_t bufferSize);
bool bluetoothle_device_get_service_ids(t_bluetoothle_device_handle handle, BluetoothUUIDSet &out_service_ids);
bool bluetoothle_device_get_full_path(t_bluetoothle_device_handle handle, char *outBuffer, size_t bufferSize);
bool bluetoothle_device_get_bluetooth_address(t_bluetoothle_device_handle handle, char *outBuffer, size_t bufferSize);
bool bluetoothle_device_get_gatt_profile(t_bluetoothle_device_handle handle, BLEGattProfile **outGattProfile);
bool bluetoothle_device_get_is_open(t_bluetoothle_device_handle handle);

#endif  // BLE_DEVICE_MANAGER_H