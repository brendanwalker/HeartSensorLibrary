#ifndef BLE_API_INTERFACE_H
#define BLE_API_INTERFACE_H

#include "BluetoothUUID.h"
#include "BluetoothLEDeviceGatt.h"

#include <stddef.h>
#include <vector>

//-- constants -----
enum eBluetoothLEApiType : short
{
	_BLEApiType_INVALID = -1,

	_BLEApiType_WinBLE,

	_BLEApiType_COUNT
};


//-- typedefs -----
struct t_bluetoothle_device_handle
{
	eBluetoothLEApiType api_type;
	short unique_id;

	inline bool operator < (const t_bluetoothle_device_handle &other) const { return unique_id < other.unique_id; }
	inline bool operator > (const t_bluetoothle_device_handle &other) const { return unique_id > other.unique_id; }
	inline bool operator == (const t_bluetoothle_device_handle &other) const { return unique_id == other.unique_id; }
	inline bool operator != (const t_bluetoothle_device_handle &other) const { return unique_id != other.unique_id; }
};
const t_bluetoothle_device_handle k_invalid_ble_device_handle = { _BLEApiType_INVALID, -1 };

//-- macros -----
//#define DEBUG_BLE
#if defined(DEBUG_BLE)
#define debug(...) fprintf(stdout, __VA_ARGS__)
#else
#define debug(...) 
#endif

//-- definitions -----
class BluetoothLEDeviceEnumerator
{
public:
	eBluetoothLEApiType api_type;
	int device_index;
};

class BluetoothLEDeviceState
{
public:
	BluetoothLEDeviceState();
	virtual ~BluetoothLEDeviceState();

	void assignPublicHandle(t_bluetoothle_device_handle handle) { deviceHandle = handle; }
	t_bluetoothle_device_handle getPublicHandle() const { return deviceHandle; }
	inline BLEGattProfile *getGattProfile() const { return gattProfile; }

protected:
	t_bluetoothle_device_handle deviceHandle;
	BLEGattProfile *gattProfile;
};

//-- interface -----
class IBluetoothLEApi
{
public:
	IBluetoothLEApi() {}
	virtual ~IBluetoothLEApi() {}

	virtual eBluetoothLEApiType getRuntimeBLEApiType() const = 0;

	virtual bool startup() = 0;
	virtual void shutdown() = 0;

	virtual BluetoothLEDeviceEnumerator* deviceEnumeratorCreate() = 0;
	virtual bool deviceEnumeratorGetServiceIDs(const BluetoothLEDeviceEnumerator* enumerator, class BluetoothUUIDSet *out_service_ids) const = 0;
	virtual bool deviceEnumeratorGetFriendlyName(const BluetoothLEDeviceEnumerator* enumerator, char *outBuffer, size_t bufferSize) const = 0;
	virtual bool deviceEnumeratorGetPath(const BluetoothLEDeviceEnumerator* enumerator, char *outBuffer, size_t bufferSize) const = 0;
	virtual bool deviceEnumeratorGetUniqueIdentifier(const BluetoothLEDeviceEnumerator* enumerator, char *outBuffer, size_t bufferSize) const = 0;
	virtual bool deviceEnumeratorIsValid(const BluetoothLEDeviceEnumerator* enumerator) = 0;
	virtual void deviceEnumeratorNext(BluetoothLEDeviceEnumerator* enumerator) = 0;
	virtual void deviceEnumeratorDispose(BluetoothLEDeviceEnumerator* enumerator) = 0;

	virtual BluetoothLEDeviceState *openBluetoothLEDevice(const BluetoothLEDeviceEnumerator* enumerator, BLEGattProfile **out_gatt_profile) = 0;
	virtual void closeBluetoothLEDevice(BluetoothLEDeviceState* device_state) = 0;
	virtual bool canBluetoothLEDeviceBeOpened(const BluetoothLEDeviceEnumerator* enumerator, char *outReason, size_t bufferSize) = 0;

	virtual bool getBluetoothLEDeviceServiceIDs(const BluetoothLEDeviceState* device_state, class BluetoothUUIDSet *out_service_ids) const = 0;
	virtual bool getBluetoothLEDevicePath(const BluetoothLEDeviceState* device_state, char *outBuffer, size_t bufferSize) const = 0;
	virtual bool getBluetoothLEAddress(const BluetoothLEDeviceState* device_state, char *outBuffer, size_t bufferSize) const = 0;
	virtual bool getBluetoothLEGattProfile(const BluetoothLEDeviceState* device_state, BLEGattProfile **outGattProfile) const = 0;
};

#endif // BLE_API_INTERFACE_H
