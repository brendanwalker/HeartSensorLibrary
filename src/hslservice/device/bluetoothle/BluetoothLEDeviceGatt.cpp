#include "BluetoothLEDeviceGatt.h"
#include <algorithm>

// -- BLEGattProfile ----
BLEGattProfile::BLEGattProfile(BluetoothLEDeviceState *device)
	: parentDevice(device)
{
}

BLEGattProfile::~BLEGattProfile()
{
	for (auto service : services)
	{
		delete service;
	}
}

const BLEGattService* BLEGattProfile::findService(const BluetoothUUID& uuid) const
{
	auto it = std::find_if(
		services.begin(), services.end(),
		[&uuid](const BLEGattService *service)
	{
		return service->getServiceUuid() == uuid;
	});

	return it != services.end() ? *it : nullptr;
}

// -- BLEGattService ----
BLEGattService::BLEGattService(BLEGattProfile *profile, const BluetoothUUID &uuid)
	: parentProfile(profile)
	, serviceUuid(uuid)
{
}

BLEGattService::~BLEGattService()
{
	for (auto characteristic : characteristics)
	{
		delete characteristic;
	}
}

BLEGattCharacteristic *BLEGattService::findCharacteristic(const BluetoothUUID& uuid) const
{
	auto it = std::find_if(
		characteristics.begin(), characteristics.end(),
		[&uuid](const BLEGattCharacteristic *characteristic)
	{
		return characteristic->getCharacteristicUuid() == uuid;
	});

	return it != characteristics.end() ? *it : nullptr;
}

// -- BLEGattCharacteristic ----
BLEGattCharacteristic::BLEGattCharacteristic(BLEGattService *service, const BluetoothUUID &uuid)
	: parentService(service)
	, characteristicUuid(uuid)
	, characteristicValue(nullptr)
{

}

BLEGattCharacteristic::~BLEGattCharacteristic()
{
	for (auto descriptor : descriptors)
	{
		delete descriptor;
	}

	if (characteristicValue != nullptr)
	{
		delete characteristicValue;
	}
}

BLEGattDescriptor *BLEGattCharacteristic::findDescriptor(const BluetoothUUID& uuid) const
{
	auto it = std::find_if(
		descriptors.begin(), descriptors.end(),
		[&uuid](const BLEGattDescriptor *descriptor)
	{
		return descriptor->getDescriptorUuid() == uuid;
	});

	return it != descriptors.end() ? *it : nullptr;
}

// -- BLEGattCharacteristicValue ----
BLEGattCharacteristicValue::BLEGattCharacteristicValue(BLEGattCharacteristic *characteristic)
	: parentCharacteristic(characteristic)
{
}

BLEGattCharacteristicValue::~BLEGattCharacteristicValue()
{
}

// -- BLEGattDescriptor ----
BLEGattDescriptor::BLEGattDescriptor(BLEGattCharacteristic *characteristic, const BluetoothUUID &uuid)
	: parentCharacteristic(characteristic)
	, descriptorUuid(uuid)
	, descriptorValue(nullptr)
{
}

BLEGattDescriptor::~BLEGattDescriptor()
{
	if (descriptorValue != nullptr)
	{
		delete descriptorValue;
	}
}

// -- BLEGattDescriptorValue ----
BLEGattDescriptorValue::BLEGattDescriptorValue(BLEGattDescriptor *descriptor)
	: parentDescriptor(descriptor)
{
}

BLEGattDescriptorValue::~BLEGattDescriptorValue()
{
}