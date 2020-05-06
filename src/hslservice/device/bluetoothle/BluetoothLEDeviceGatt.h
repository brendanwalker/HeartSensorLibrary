#ifndef BLUETOOTH_LE_DEVICE_GATT_API_H
#define BLUETOOTH_LE_DEVICE_GATT_API_H

#include "BluetoothUUID.h"
#include <vector>
#include <functional>

class BluetoothGattHandle
{
public:
	BluetoothGattHandle() : m_handleValue(0x0000) {}
	BluetoothGattHandle(unsigned short handleValue) : m_handleValue(handleValue) {}

	inline unsigned short getHandleValue() const { return m_handleValue; }
	inline bool operator < (const BluetoothGattHandle &other) const { return m_handleValue < other.m_handleValue; }
	inline bool operator > (const BluetoothGattHandle &other) const { return m_handleValue > other.m_handleValue; }
	inline bool operator == (const BluetoothGattHandle &other) const { return m_handleValue == other.m_handleValue; }
	inline bool operator != (const BluetoothGattHandle &other) const { return m_handleValue != other.m_handleValue; }

private:
	unsigned short m_handleValue;
};
const BluetoothGattHandle k_invalid_ble_gatt_handle = { 0x0000 };

class BluetoothEventHandle
{
public:
	BluetoothEventHandle() : m_handleData(nullptr) {}
	BluetoothEventHandle(void *handle) : m_handleData(handle) {}

	inline bool isValid() const { return m_handleData != nullptr; }
	inline void* getHandleData() const { return m_handleData; }
    inline bool operator == (const BluetoothEventHandle& other) const { return m_handleData == other.m_handleData; }
    inline bool operator != (const BluetoothEventHandle& other) const { return m_handleData != other.m_handleData; }

private:
	void *m_handleData;
};
const BluetoothEventHandle k_invalid_ble_gatt_event_handle = { nullptr };

//-- definitions -----
class BLEGattProfile
{
public:
	BLEGattProfile(class BluetoothLEDeviceState *device);
	virtual ~BLEGattProfile();

	inline class BluetoothLEDeviceState *getParentDevice() const { return parentDevice; }

	inline const std::vector<class BLEGattService *> &getServices() const { return services; }
	const class BLEGattService* findService(const BluetoothUUID& uuid) const;

protected:
	class BluetoothLEDeviceState *parentDevice;
	std::vector<class BLEGattService *> services;
};

class BLEGattService
{
public:
	BLEGattService(class BLEGattProfile *profile, const BluetoothUUID &uuid);
	virtual ~BLEGattService();

	const BluetoothUUID &getServiceUuid() const { return serviceUuid; }
	const std::vector<class BLEGattCharacteristic *> &getCharacteristics() const { return characteristics; }

	class BLEGattCharacteristic *findCharacteristic(const BluetoothUUID& uuid) const;

protected:
	BLEGattProfile *parentProfile;
	BluetoothUUID serviceUuid;
	std::vector<class BLEGattCharacteristic *> characteristics;
};

class BLEGattCharacteristic
{
public:
	using ChangeCallback = std::function<void(BluetoothGattHandle attributrHandle, uint8_t *data, size_t data_size)>;

	BLEGattCharacteristic(BLEGattService *service, const BluetoothUUID &uuid);
	virtual ~BLEGattCharacteristic();

	const BluetoothUUID &getCharacteristicUuid() const { return characteristicUuid; }

	virtual bool getIsBroadcastable() const = 0;
	virtual bool getIsReadable() const = 0;
	virtual bool getIsWritable() const = 0;
	virtual bool getIsWritableWithoutResponse() const = 0;
	virtual bool getIsSignedWritable() const = 0;
	virtual bool getIsNotifiable() const = 0;
	virtual bool getIsIndicatable() const = 0;
	virtual bool getHasExtendedProperties() const = 0;

	inline class BLEGattCharacteristicValue* getCharacteristicValue() const { return characteristicValue; }

	const std::vector<class BLEGattDescriptor *> &getDescriptors() const { return descriptors; }
	class BLEGattDescriptor *findDescriptor(const BluetoothUUID& uuid) const;

	virtual BluetoothEventHandle registerChangeEvent(ChangeCallback callback) = 0;
	virtual void unregisterChangeEvent(const BluetoothEventHandle &handle) = 0;

protected:
	BLEGattService *parentService;
	BluetoothUUID characteristicUuid;
	std::vector<class BLEGattDescriptor *> descriptors;
	class BLEGattCharacteristicValue *characteristicValue;
};

class BLEGattCharacteristicValue
{
public:
	BLEGattCharacteristicValue(BLEGattCharacteristic *characteristic);
	virtual ~BLEGattCharacteristicValue();

	virtual bool getByte(uint8_t &outValue) = 0;
	virtual bool getData(uint8_t **outBuffer, size_t *outBufferSize) = 0;

	virtual bool setByte(uint8_t inValue) = 0;
	virtual bool setData(const uint8_t *inBuffer, size_t inBufferSize) = 0;

protected:
	BLEGattCharacteristic *parentCharacteristic;
};

class BLEGattDescriptor
{
public:
	enum class DescriptorType : uint8_t
	{
		CharacteristicExtendedProperties,
		CharacteristicUserDescription,
		ClientCharacteristicConfiguration,
		ServerCharacteristicConfiguration,
		CharacteristicFormat,
		CharacteristicAggregateFormat,
		CustomDescriptor
	};

	BLEGattDescriptor(BLEGattCharacteristic *characteristic, const BluetoothUUID &uuid);
	virtual ~BLEGattDescriptor();

	inline const BluetoothUUID &getDescriptorUuid() const { return descriptorUuid; }
	inline DescriptorType getDescriptorType() const { return descriptorType; }
	inline BluetoothGattHandle getAttributeHandle() const { return attributeHandle; }
	inline class BLEGattDescriptorValue* getDescriptorValue() const { return descriptorValue; }

protected:
	BLEGattCharacteristic *parentCharacteristic;
	BluetoothUUID descriptorUuid;
	DescriptorType descriptorType;
	BluetoothGattHandle attributeHandle;
	class BLEGattDescriptorValue *descriptorValue;
};

class BLEGattDescriptorValue
{
public:
	struct CharacteristicExtendedProperties
	{
		bool IsReliableWriteEnabled;
		bool IsAuxiliariesWritable;
	};

	struct ClientCharacteristicConfiguration
	{
		bool IsSubscribeToNotification;
		bool IsSubscribeToIndication;
	};

	struct ServerCharacteristicConfiguration
	{
		bool IsBroadcast;
	};

	struct CharacteristicFormat
	{
		uint8_t Format;
		uint8_t Exponent;
		BluetoothUUID Unit;
		uint8_t NameSpace;
		BluetoothUUID Description;
	};

	BLEGattDescriptorValue(BLEGattDescriptor *descriptor);
	virtual ~BLEGattDescriptorValue();

	BLEGattDescriptor::DescriptorType getDescriptorType() const { return parentDescriptor->getDescriptorType(); }
	const BluetoothUUID &getBluetoothUUID() const { return parentDescriptor->getDescriptorUuid(); }

	virtual bool readDescriptorValue() = 0;
	virtual bool writeDescriptorValue() = 0;

	virtual bool getCharacteristicExtendedProperties(CharacteristicExtendedProperties &outProps) const = 0;
	virtual bool getClientCharacteristicConfiguration(ClientCharacteristicConfiguration &outConfig) const = 0;
	virtual bool getServerCharacteristicConfiguration(ServerCharacteristicConfiguration &outConfig) const = 0;
	virtual bool getCharacteristicFormat(CharacteristicFormat &outFormat) const = 0;

	virtual bool setCharacteristicExtendedProperties(const CharacteristicExtendedProperties &inProps) = 0;
	virtual bool setClientCharacteristicConfiguration(const ClientCharacteristicConfiguration &inConfig) = 0;
	virtual bool setServerCharacteristicConfiguration(const ServerCharacteristicConfiguration &inConfig) = 0;
	virtual bool setCharacteristicFormat(const CharacteristicFormat &inFormat) = 0;

	virtual bool getByte(uint8_t &outValue) const = 0;
	virtual bool getData(uint8_t **outBuffer, size_t *outBufferSize) const = 0;

	virtual bool setByte(uint8_t inValue) = 0;
	virtual bool setData(const uint8_t *inBuffer, size_t inBufferSize) = 0;

protected:
	BLEGattDescriptor *parentDescriptor;
};

#endif // BLUETOOTH_LE_DEVICE_GATT_API_H