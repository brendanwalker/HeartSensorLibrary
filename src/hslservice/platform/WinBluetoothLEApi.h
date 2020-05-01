#ifndef WIN_BLE_API_H
#define WIN_BLE_API_H

#include "BluetoothLEApiInterface.h"

typedef void * HANDLE;

class WinBLEDeviceState : public BluetoothLEDeviceState
{
public:
	WinBLEDeviceState(const class WinBLEDeviceEnumerator* enumerator);
	virtual ~WinBLEDeviceState();

	inline const std::string& getDevicePath() const { return devicePath; }
	inline const std::string& getDeviceInstanceId() const { return deviceInstanceId; }
	inline const std::string& getFriendlyName() const { return friendlyName; }
	inline const std::string& getBluetoothAddress() const { return bluetoothAddress; }
	inline const BluetoothUUIDSet& getServiceUUIDSet() const { return m_serviceUUIDSet; }

	bool openDevice(BLEGattProfile **out_gatt_profile);
	void closeDevice();

	inline HANDLE getDeviceHandle() const { return deviceHandle; }

private:
	std::string devicePath;
	std::string deviceInstanceId;
	std::string friendlyName;
	std::string bluetoothAddress;
	BluetoothUUIDSet m_serviceUUIDSet;

	HANDLE deviceHandle;
};

class WinBLEApi : public IBluetoothLEApi
{
public:
	WinBLEApi();

	eBluetoothLEApiType getRuntimeBLEApiType() const override { return _BLEApiType_WinBLE; }
	static eBluetoothLEApiType getStaticBLEApiType() { return _BLEApiType_WinBLE; }
	static WinBLEApi *getInterface();

	// IBLEApi
	bool startup() override;
	void shutdown() override;

	BluetoothLEDeviceEnumerator* deviceEnumeratorCreate() override;
	bool deviceEnumeratorGetServiceIDs(const BluetoothLEDeviceEnumerator* enumerator, class BluetoothUUIDSet *out_service_ids) const override;
	bool deviceEnumeratorGetPath(const BluetoothLEDeviceEnumerator* enumerator, char *outBuffer, size_t bufferSize) const override;
	bool deviceEnumeratorGetUniqueIdentifier(const BluetoothLEDeviceEnumerator* enumerator, char *outBuffer, size_t bufferSize) const override;
	bool deviceEnumeratorIsValid(const BluetoothLEDeviceEnumerator* enumerator) override;
	void deviceEnumeratorNext(BluetoothLEDeviceEnumerator* enumerator) override;
	void deviceEnumeratorDispose(BluetoothLEDeviceEnumerator* enumerator) override;

	BluetoothLEDeviceState *openBluetoothLEDevice(const BluetoothLEDeviceEnumerator* enumerator, BLEGattProfile **outGattProfile) override;
	void closeBluetoothLEDevice(BluetoothLEDeviceState* device_state) override;
	bool canBluetoothLEDeviceBeOpened(const BluetoothLEDeviceEnumerator* enumerator, char *outReason, size_t bufferSize) override;

	bool getBluetoothLEDeviceServiceIDs(const BluetoothLEDeviceState* device_state, class BluetoothUUIDSet *out_service_ids) const override;
	bool getBluetoothLEDevicePath(const BluetoothLEDeviceState* device_state, char *outBuffer, size_t bufferSize) const override;
	bool getBluetoothLEAddress(const BluetoothLEDeviceState* device_state, char *outBuffer, size_t bufferSize) const override;
	bool getBluetoothLEGattProfile(const BluetoothLEDeviceState* device_state, BLEGattProfile **outGattProfile) const override;

private:
	std::vector<struct WinBLEAsyncTransfer *> m_pendingAsyncTransfers;
};

#endif // WIN_BLE_API_H

