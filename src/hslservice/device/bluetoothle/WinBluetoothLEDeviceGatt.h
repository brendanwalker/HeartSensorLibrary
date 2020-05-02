#ifndef WIN_BLUETOOTH_LE_DEVICE_GATT_API_H
#define WIN_BLUETOOTH_LE_DEVICE_GATT_API_H

#include "BluetoothLEDeviceGatt.h"
#include <map>

//-- WinBluetoothUUID -----
class WinBluetoothUUID : public BluetoothUUID
{
public:
	WinBluetoothUUID(const BluetoothUUID &uuid);
	WinBluetoothUUID(const BTH_LE_UUID &win_uuid);

	void ToWinAPI(BTH_LE_UUID &win_uuid);
};

//-- WinBLEGattProfile -----
class WinBLEGattProfile : public BLEGattProfile
{
public:
	WinBLEGattProfile(class WinBLEDeviceState *device);
	virtual ~WinBLEGattProfile();

	bool fetchServices();
	void freeServices();

	inline HANDLE getDeviceHandle() const;

private:
	BTH_LE_GATT_SERVICE *serviceBufferWinAPI;
};

//-- WinBLEGattService -----
class WinBLEGattService : public BLEGattService
{
public:
	WinBLEGattService(WinBLEGattProfile *profile, const BluetoothUUID &uuid, BTH_LE_GATT_SERVICE *service);
	virtual ~WinBLEGattService();

	bool fetchCharacteristics();
	void freeCharacteristics();

	HANDLE getDeviceHandle() const;

protected:
	BTH_LE_GATT_SERVICE *serviceWinAPI;
	BTH_LE_GATT_CHARACTERISTIC *characteristicBufferWinAPI;
};

//-- WinBLEGattCharacteristic -----
class WinBLEGattCharacteristic : public BLEGattCharacteristic
{
public:
	WinBLEGattCharacteristic(BLEGattService *service, const BluetoothUUID &uuid, BTH_LE_GATT_CHARACTERISTIC *characteristic);
	virtual ~WinBLEGattCharacteristic();

	bool fetchDescriptors();
	void freeDescriptors();

	HANDLE getDeviceHandle() const;
	BTH_LE_GATT_CHARACTERISTIC *getCharacteristicWinAPI();

	virtual bool getIsBroadcastable() const;
	virtual bool getIsReadable() const;
	virtual bool getIsWritable() const;
	virtual bool getIsWritableWithoutResponse() const;
	virtual bool getIsSignedWritable() const;
	virtual bool getIsNotifiable() const;
	virtual bool getIsIndicatable() const;
	virtual bool getHasExtendedProperties() const;

	virtual BluetoothEventHandle registerChangeEvent(ChangeCallback callback) override;
	virtual void unregisterChangeEvent(const BluetoothEventHandle &handle) override;

protected:
	static void gattEventCallback(BTH_LE_GATT_EVENT_TYPE EventType, PVOID EventOutParameter, PVOID Context);

	BTH_LE_GATT_CHARACTERISTIC *characteristicWinAPI;
	BTH_LE_GATT_DESCRIPTOR *descriptorBufferWinAPI;

	struct ChangeCallbackContext
	{
		BluetoothEventHandle handle;
		ChangeCallback callback;
	};
	std::map<BLUETOOTH_GATT_EVENT_HANDLE, ChangeCallbackContext *> changeEventCallbacks;
};

//-- WinBLEGattCharacteristic -----
class WinBLEGattCharacteristicValue : public BLEGattCharacteristicValue
{
public:
	WinBLEGattCharacteristicValue(BLEGattCharacteristic *characteristic);
	WinBLEGattCharacteristic* getParentCharacteristic() const;

	HANDLE getDeviceHandle() const;
	BTH_LE_GATT_CHARACTERISTIC *getCharacteristicWinAPI() const;
	
	virtual bool getByte(uint8_t &outValue);
	virtual bool getData(uint8_t **outBuffer, size_t *outBufferSize);
	virtual bool setByte(uint8_t inValue);
	virtual bool setData(const uint8_t *inBuffer, size_t inBufferSize);

protected:
	static size_t getTailBufferSize(size_t characteristic_value_size);
	static size_t getCharacteristicValueSize(size_t tail_buffer_size);
	void ensureTailBufferSize(size_t tail_buffer_size);

	BTH_LE_GATT_CHARACTERISTIC_VALUE *characteristicValueBufferWinAPI;
	size_t maxTailBufferSize;
};

class WinBLEGattDescriptor : public BLEGattDescriptor
{
public:
	WinBLEGattDescriptor(BLEGattCharacteristic *characteristic, const BluetoothUUID &uuid, BTH_LE_GATT_DESCRIPTOR *descriptor);

	HANDLE getDeviceHandle() const;
	BTH_LE_GATT_DESCRIPTOR *getDescriptorWinAPI();

protected:
	BTH_LE_GATT_DESCRIPTOR *descriptorWinAPI;
};

class WinBLEGattDescriptorValue : public BLEGattDescriptorValue
{
public:
	WinBLEGattDescriptorValue(BLEGattDescriptor *descriptor, BTH_LE_GATT_DESCRIPTOR_VALUE *descriptorValue);
	WinBLEGattDescriptor* getParentDescriptor() const;

	HANDLE getDeviceHandle() const;
	BTH_LE_GATT_DESCRIPTOR *getDescriptorWinAPI() const;

	virtual bool readDescriptorValue();
	virtual bool writeDescriptorValue();

	virtual bool getCharacteristicExtendedProperties(BLEGattDescriptorValue::CharacteristicExtendedProperties &outProps) const;
	virtual bool getClientCharacteristicConfiguration(BLEGattDescriptorValue::ClientCharacteristicConfiguration &outConfig) const;
	virtual bool getServerCharacteristicConfiguration(BLEGattDescriptorValue::ServerCharacteristicConfiguration &outConfig) const;
	virtual bool getCharacteristicFormat(BLEGattDescriptorValue::CharacteristicFormat &outFormat) const;
	virtual bool setCharacteristicExtendedProperties(const BLEGattDescriptorValue::CharacteristicExtendedProperties &inProps);
	virtual bool setClientCharacteristicConfiguration(const BLEGattDescriptorValue::ClientCharacteristicConfiguration &inConfig);
	virtual bool setServerCharacteristicConfiguration(const BLEGattDescriptorValue::ServerCharacteristicConfiguration &inConfig);
	virtual bool setCharacteristicFormat(const BLEGattDescriptorValue::CharacteristicFormat &inFormat);

	virtual bool getByte(uint8_t &outValue) const;
	virtual bool getData(uint8_t **outBuffer, size_t *outBufferSize) const;
	virtual bool setByte(uint8_t inValue);
	virtual bool setData(const uint8_t *inBuffer, size_t inBufferSize);

protected:
	static size_t getTailBufferSize(size_t descriptor_value_size);
	static size_t getDescriptorValueSize(size_t tail_buffer_size);
	void ensureTailBufferSize(size_t tail_buffer_size);

	BTH_LE_GATT_DESCRIPTOR_VALUE *descriptorValueWinAPI;
	size_t maxTailBufferSize;
};

#endif // WIN_BLUETOOTH_LE_DEVICE_GATT_API_H