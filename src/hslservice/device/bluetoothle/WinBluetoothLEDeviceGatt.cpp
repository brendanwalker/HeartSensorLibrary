//-- includes -----
#include "Logger.h"
#include "Utility.h"
#include "WinBluetoothLEApi.h"

#include <assert.h>
#include <stdio.h>
#include <strsafe.h>

#include <functional>
#include <regex>
#include <iomanip>

#define ANSI
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <bthdef.h>
#include <bthledef.h>
#include <bluetoothapis.h>
#include <bluetoothleapis.h>
#include <combaseapi.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include <devguid.h>
#include <regstr.h>
#include <setupapi.h>
#include <winerror.h>

#ifdef _MSC_VER
#pragma warning(disable:4996) // disable warnings about strncpy
#endif

#include "WinBluetoothLEDeviceGatt.h"

//-- WinBluetoothUUID -----
WinBluetoothUUID::WinBluetoothUUID(const BluetoothUUID &uuid)
	: BluetoothUUID(uuid.getUUIDString())
{
}

WinBluetoothUUID::WinBluetoothUUID(const BTH_LE_UUID &win_uuid) : BluetoothUUID()
{
	if (win_uuid.IsShortUuid)
	{
		std::ostringstream stringStream;
		stringStream << std::hex;
		stringStream << std::setfill('0');
		stringStream << "0x" << static_cast<int>(win_uuid.Value.ShortUuid);

		setUUID(stringStream.str());
	}
	else
	{
		const GUID &guid = win_uuid.Value.LongUuid;

		std::ostringstream stringStream;
		stringStream << std::hex;
		stringStream << std::setfill('0');
		stringStream << std::setw(8) << guid.Data1;
		stringStream << "-";
		stringStream << std::setw(4) << guid.Data2;
		stringStream << "-";
		stringStream << std::setw(4) << guid.Data3;
		stringStream << "-";
		stringStream << std::setw(2);
		for (int i = 0; i < sizeof(guid.Data4); i++) {
			if (i == 2)
				stringStream << "-";
			stringStream << static_cast<int>(guid.Data4[i]);
		}

		setUUID(stringStream.str());
	}
}

void WinBluetoothUUID::ToWinAPI(BTH_LE_UUID &win_uuid)
{
	std::wostringstream stringStream;
	stringStream << L'{';
	stringStream << Utility::convert_string_to_wstring(getUUIDString());
	stringStream << L'}';

	GUID guid;
	if (CLSIDFromString(stringStream.str().c_str(), &guid) == NOERROR)
	{
		win_uuid.IsShortUuid = FALSE;
		win_uuid.Value.LongUuid = guid;
	}
}

//-- WinBLEDeviceState -----
WinBLEGattProfile::WinBLEGattProfile(WinBLEDeviceState *device)
	: BLEGattProfile(device)
{
}

WinBLEGattProfile::~WinBLEGattProfile()
{
	freeServices();
}

bool WinBLEGattProfile::fetchServices()
{
	bool bSuccess = true;

	freeServices();

	USHORT serviceBufferRequiredCount = 0;
	if (bSuccess)
	{
		HRESULT hr = BluetoothGATTGetServices(
			getDeviceHandle(),
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
		serviceBufferWinAPI = (PBTH_LE_GATT_SERVICE)malloc(sizeof(BTH_LE_GATT_SERVICE) * serviceBufferRequiredCount);
		assert(serviceBufferWinAPI != nullptr);
		memset(serviceBufferWinAPI, 0, sizeof(BTH_LE_GATT_SERVICE) * serviceBufferRequiredCount);

		USHORT servicesBufferActualCount = serviceBufferRequiredCount;
		HRESULT hr = BluetoothGATTGetServices(
			getDeviceHandle(),
			servicesBufferActualCount,
			serviceBufferWinAPI,
			&serviceBufferRequiredCount,
			BLUETOOTH_GATT_FLAG_NONE);

		if (hr == S_OK)
		{
			for (USHORT serviceIndex = 0; serviceIndex < servicesBufferActualCount; ++serviceIndex)
			{
				BTH_LE_GATT_SERVICE *serviceWinAPI = &serviceBufferWinAPI[serviceIndex];
				WinBLEGattService *serviceWrapper =
					new WinBLEGattService(this, WinBluetoothUUID(serviceWinAPI->ServiceUuid), serviceWinAPI);

				services.push_back(serviceWrapper);
			}
		}
		else
		{
			bSuccess = false;
		}
	}

	// Tell each service to fetch it's characteristics list
	if (bSuccess)
	{
		for (auto service : services)
		{
			WinBLEGattService *serviceWrapper = static_cast<WinBLEGattService *>(service);

			if (!serviceWrapper->fetchCharacteristics())
			{
				bSuccess = false;
				break;
			}
		}
	}

	return bSuccess;
}

void WinBLEGattProfile::freeServices()
{
	for (auto service : services)
	{
		delete service;
	}
	services.clear();

	if (serviceBufferWinAPI != nullptr)
	{
		free(serviceBufferWinAPI);
		serviceBufferWinAPI = nullptr;
	}
}

HANDLE WinBLEGattProfile::getDeviceHandle() const
{
	return static_cast<WinBLEDeviceState *>(parentDevice)->getDeviceHandle();
}

//-- WinBLEGattService -----
WinBLEGattService::WinBLEGattService(WinBLEGattProfile *profile, const BluetoothUUID &uuid, BTH_LE_GATT_SERVICE *service)
	: BLEGattService(profile, uuid)
	, serviceWinAPI(service)
{
}

WinBLEGattService::~WinBLEGattService()
{
	freeCharacteristics();
}

bool WinBLEGattService::fetchCharacteristics()
{
	bool bSuccess = true;

	freeCharacteristics();

	USHORT characteristicsBufferRequiredCount = 0;
	if (bSuccess)
	{
		HRESULT hr = BluetoothGATTGetCharacteristics(
			getDeviceHandle(),
			serviceWinAPI,
			0,
			NULL,
			&characteristicsBufferRequiredCount,
			BLUETOOTH_GATT_FLAG_NONE);

		if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA))
		{
			bSuccess = false;
		}
	}

	// Fetch the characteristics list from the windows BluetoothLE API.
	// Create a WinBLEGattCharacteristic wrapper instance for each entry in the characteristics list.
	if (bSuccess)
	{
		characteristicBufferWinAPI =
			(PBTH_LE_GATT_CHARACTERISTIC)malloc(sizeof(BTH_LE_GATT_CHARACTERISTIC) * characteristicsBufferRequiredCount);
		assert(characteristicBufferWinAPI != nullptr);
		memset(characteristicBufferWinAPI, 0, sizeof(BTH_LE_GATT_CHARACTERISTIC) * characteristicsBufferRequiredCount);

		USHORT characteristicsBufferActualCount = characteristicsBufferRequiredCount;
		HRESULT hr = BluetoothGATTGetCharacteristics(
			getDeviceHandle(),
			serviceWinAPI,
			characteristicsBufferActualCount,
			characteristicBufferWinAPI,
			&characteristicsBufferRequiredCount,
			BLUETOOTH_GATT_FLAG_NONE);

		if (hr == S_OK)
		{
			for (USHORT characteristicsIndex = 0; characteristicsIndex < characteristicsBufferActualCount; ++characteristicsIndex)
			{
				BTH_LE_GATT_CHARACTERISTIC *characteristicWinAPI = &characteristicBufferWinAPI[characteristicsIndex];
				WinBLEGattCharacteristic *characteristicWrapper =
					new WinBLEGattCharacteristic(this, WinBluetoothUUID(characteristicWinAPI->CharacteristicUuid), characteristicWinAPI);

				characteristics.push_back(characteristicWrapper);
			}
		}
		else
		{
			bSuccess = false;
		}
	}

	// Tell each characteristic to fetch it's descriptor list
	if (bSuccess)
	{
		for (auto characteristic : characteristics)
		{
			WinBLEGattCharacteristic *characteristicWrapper = static_cast<WinBLEGattCharacteristic *>(characteristic);

			if (!characteristicWrapper->fetchCharacteristicValue() ||
				!characteristicWrapper->fetchDescriptors())
			{
				bSuccess = false;
				break;
			}
		}
	}

	return bSuccess;
}

void WinBLEGattService::freeCharacteristics()
{
	for (auto characteristic : characteristics)
	{
		delete characteristic;
	}
	characteristics.clear();

	if (characteristicBufferWinAPI != nullptr)
	{
		free(characteristicBufferWinAPI);
		characteristicBufferWinAPI = nullptr;
	}
}

HANDLE WinBLEGattService::getDeviceHandle() const
{
	return static_cast<WinBLEGattProfile *>(parentProfile)->getDeviceHandle();
}

//-- WinBLEGattCharacteristic -----
WinBLEGattCharacteristic::WinBLEGattCharacteristic(BLEGattService *service, const BluetoothUUID &uuid, BTH_LE_GATT_CHARACTERISTIC *characteristic)
	: BLEGattCharacteristic(service, uuid)
	, characteristicWinAPI(characteristic)
{
}

WinBLEGattCharacteristic::~WinBLEGattCharacteristic()
{
	freeCharacteristicValue();
	freeDescriptors();

	while (!changeEventCallbacks.empty())
	{
		auto it = changeEventCallbacks.begin();
		const BluetoothEventHandle &handle= it->second->handle;

		unregisterChangeEvent(handle);
	}
}

bool WinBLEGattCharacteristic::fetchDescriptors()
{
	bool bSuccess = true;

	freeDescriptors();

	USHORT descriptorBufferRequiredCount = 0;
	if (bSuccess)
	{
		HRESULT hr = BluetoothGATTGetDescriptors(
			getDeviceHandle(),
			characteristicWinAPI,
			0,
			NULL,
			&descriptorBufferRequiredCount,
			BLUETOOTH_GATT_FLAG_NONE);

		if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA))
		{
			bSuccess = false;
		}
	}

	// Fetch the descriptor list from the windows BluetoothLE API.
	// Create a WinBLEGattDescriptor wrapper instance for each entry in the characteristics list.
	if (bSuccess)
	{
		descriptorBufferWinAPI =
			(PBTH_LE_GATT_DESCRIPTOR)malloc(sizeof(BTH_LE_GATT_DESCRIPTOR) * descriptorBufferRequiredCount);
		assert(descriptorBufferWinAPI != nullptr);
		memset(descriptorBufferWinAPI, 0, sizeof(BTH_LE_GATT_DESCRIPTOR) * descriptorBufferRequiredCount);

		USHORT descriptorBufferActualCount = descriptorBufferRequiredCount;
		HRESULT hr = BluetoothGATTGetDescriptors(
			getDeviceHandle(),
			characteristicWinAPI,
			descriptorBufferActualCount,
			descriptorBufferWinAPI,
			&descriptorBufferRequiredCount,
			BLUETOOTH_GATT_FLAG_NONE);

		if (hr == S_OK)
		{
			for (USHORT descriptorIndex = 0; descriptorIndex < descriptorBufferActualCount; ++descriptorIndex)
			{
				BTH_LE_GATT_DESCRIPTOR *descriptorWinAPI = &descriptorBufferWinAPI[descriptorIndex];
				WinBLEGattDescriptor *descriptorWrapper =
					new WinBLEGattDescriptor(this, WinBluetoothUUID(descriptorWinAPI->DescriptorUuid), descriptorWinAPI);

				descriptors.push_back(descriptorWrapper);
			}
		}
		else
		{
			bSuccess = false;
		}
	}

	// Tell each characteristic to fetch it's descriptor list
	if (bSuccess)
	{
		for (auto descriptor : descriptors)
		{
			WinBLEGattDescriptor *descriptorWinAPI = static_cast<WinBLEGattDescriptor *>(descriptor);

			if (!descriptorWinAPI->fetchDescriptorValue())
			{
				bSuccess = false;
				break;
			}
		}
	}

	return bSuccess;
}

void WinBLEGattCharacteristic::freeDescriptors()
{
	for (auto descriptor : descriptors)
	{
		delete descriptor;
	}
	descriptors.clear();

	if (descriptorBufferWinAPI != nullptr)
	{
		free(descriptorBufferWinAPI);
		descriptorBufferWinAPI = nullptr;
	}
}

bool WinBLEGattCharacteristic::fetchCharacteristicValue()
{
	bool bSuccess = true;

	freeCharacteristicValue();

	// Determine Descriptor Value Buffer Size
	USHORT characteristicValueRequiredSize = 0;
	if (bSuccess)
	{
		HRESULT hr = BluetoothGATTGetCharacteristicValue(
			getDeviceHandle(),
			characteristicWinAPI,
			0,
			NULL,
			&characteristicValueRequiredSize,
			BLUETOOTH_GATT_FLAG_NONE);

		if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA))
		{
			bSuccess = false;
		}
	}

	if (bSuccess)
	{
		characteristicValueBufferWinAPI = (PBTH_LE_GATT_CHARACTERISTIC_VALUE)malloc(characteristicValueRequiredSize);
		assert(characteristicValueBufferWinAPI != nullptr);
		memset(characteristicValueBufferWinAPI, 0, characteristicValueRequiredSize);

		characteristicValue = new WinBLEGattCharacteristicValue(this, characteristicValueBufferWinAPI);

		bSuccess = characteristicValue->readCharacteristicValue();
	}

	return bSuccess;
}

void WinBLEGattCharacteristic::freeCharacteristicValue()
{
	if (characteristicValue != nullptr)
	{
		delete characteristicValue;
		characteristicValue = nullptr;
	}

	if (characteristicValueBufferWinAPI != nullptr)
	{
		free(characteristicValueBufferWinAPI);
		characteristicValueBufferWinAPI = nullptr;
	}
}

HANDLE WinBLEGattCharacteristic::getDeviceHandle() const
{
	return static_cast<WinBLEGattService *>(parentService)->getDeviceHandle();
}

BTH_LE_GATT_CHARACTERISTIC *WinBLEGattCharacteristic::getCharacteristicWinAPI()
{
	return characteristicWinAPI;
}

bool WinBLEGattCharacteristic::getIsBroadcastable() const
{
	return characteristicWinAPI != nullptr ? characteristicWinAPI->IsBroadcastable : false;
}

bool WinBLEGattCharacteristic::getIsReadable() const
{
	return characteristicWinAPI != nullptr ? characteristicWinAPI->IsReadable : false;
}

bool WinBLEGattCharacteristic::getIsWritable() const
{
	return characteristicWinAPI != nullptr ? characteristicWinAPI->IsWritable : false;
}

bool WinBLEGattCharacteristic::getIsWritableWithoutResponse() const
{
	return characteristicWinAPI != nullptr ? characteristicWinAPI->IsWritableWithoutResponse : false;
}

bool WinBLEGattCharacteristic::getIsSignedWritable() const
{
	return characteristicWinAPI != nullptr ? characteristicWinAPI->IsSignedWritable : false;
}

bool WinBLEGattCharacteristic::getIsNotifiable() const
{
	return characteristicWinAPI != nullptr ? characteristicWinAPI->IsNotifiable : false;
}

bool WinBLEGattCharacteristic::getIsIndicatable() const
{
	return characteristicWinAPI != nullptr ? characteristicWinAPI->IsIndicatable : false;
}

bool WinBLEGattCharacteristic::getHasExtendedProperties() const
{
	return characteristicWinAPI != nullptr ? characteristicWinAPI->HasExtendedProperties : false;
}

BluetoothEventHandle WinBLEGattCharacteristic::registerChangeEvent(
	BLEGattCharacteristic::ChangeCallback callback)
{
	BluetoothEventHandle resultHandle;

	if (characteristicWinAPI->IsNotifiable)
	{
		BLUETOOTH_GATT_VALUE_CHANGED_EVENT_REGISTRATION eventParameterIn;
		eventParameterIn.Characteristics[0] = *characteristicWinAPI;
		eventParameterIn.NumCharacteristics = 1;

		ChangeCallbackContext *callbackContext = new ChangeCallbackContext;
		callbackContext->callback = callback;

		BLUETOOTH_GATT_EVENT_HANDLE eventHandle;
		HRESULT hr = BluetoothGATTRegisterEvent(
			getDeviceHandle(),
			CharacteristicValueChangedEvent,
			&eventParameterIn,
			WinBLEGattCharacteristic::gattEventCallback,
			callbackContext,
			&eventHandle,
			BLUETOOTH_GATT_FLAG_NONE);

		if (hr == S_OK)
		{
			callbackContext->handle= BluetoothEventHandle(eventHandle);

			changeEventCallbacks.insert(std::make_pair(eventHandle, callbackContext));

			resultHandle = callbackContext->handle;
		}
	}

	return resultHandle;
}

void WinBLEGattCharacteristic::gattEventCallback(
	BTH_LE_GATT_EVENT_TYPE EventType,
	PVOID EventOutParameter,
	PVOID Context)
{
	ChangeCallbackContext *callbackContext = reinterpret_cast<ChangeCallbackContext *>(Context);

	if (callbackContext != nullptr)
	{
		PBLUETOOTH_GATT_VALUE_CHANGED_EVENT params = (PBLUETOOTH_GATT_VALUE_CHANGED_EVENT)EventOutParameter;

		if (params->CharacteristicValue->DataSize > 0)
		{
			unsigned char *data= params->CharacteristicValue->Data;
			size_t data_size = (size_t)params->CharacteristicValue->DataSize;
			
			callbackContext->callback(BluetoothGattHandle(params->ChangedAttributeHandle), data, data_size);
		}
	}
}

void WinBLEGattCharacteristic::unregisterChangeEvent(
	const BluetoothEventHandle &handle)
{
	auto it = changeEventCallbacks.find(handle.getHandleData());
	if (it != changeEventCallbacks.end())
	{
		ChangeCallbackContext *callbackContext = it->second;

		if (callbackContext != nullptr)
		{
			delete callbackContext;
		}

		BluetoothGATTUnregisterEvent(handle.getHandleData(), BLUETOOTH_GATT_FLAG_NONE);

		changeEventCallbacks.erase(it);
	}
}

//-- WinBLEGattCharacteristic -----
WinBLEGattCharacteristicValue::WinBLEGattCharacteristicValue(BLEGattCharacteristic *characteristic, BTH_LE_GATT_CHARACTERISTIC_VALUE *characteristicValue)
	: BLEGattCharacteristicValue(characteristic)
	, descriptorWinAPI(descriptorWinAPI)
	, characteristicValueBufferWinAPI(characteristicValue)
{ }

WinBLEGattCharacteristic* WinBLEGattCharacteristicValue::getParentCharacteristic() const
{
	return static_cast<WinBLEGattCharacteristic *>(parentCharacteristic);
}

HANDLE WinBLEGattCharacteristicValue::getDeviceHandle() const
{
	return getParentCharacteristic()->getDeviceHandle();
}

BTH_LE_GATT_CHARACTERISTIC *WinBLEGattCharacteristicValue::getCharacteristicWinAPI() const
{
	return getParentCharacteristic()->getCharacteristicWinAPI();
}

bool WinBLEGattCharacteristicValue::readCharacteristicValue()
{
	HRESULT hr = BluetoothGATTGetCharacteristicValue(
		getDeviceHandle(),
		getCharacteristicWinAPI(),
		characteristicValueBufferWinAPI->DataSize,
		characteristicValueBufferWinAPI,
		NULL,
		BLUETOOTH_GATT_FLAG_NONE);

	return hr == S_OK;
}

bool WinBLEGattCharacteristicValue::writeCharacteristicValue()
{
	HRESULT hr = BluetoothGATTSetCharacteristicValue(
		getDeviceHandle(),
		getCharacteristicWinAPI(),
		characteristicValueBufferWinAPI,
		NULL,
		BLUETOOTH_GATT_FLAG_NONE);

	return hr == S_OK;
}

bool WinBLEGattCharacteristicValue::getByte(unsigned char &outValue)
{
	if (characteristicValueBufferWinAPI->DataSize > 0)
	{
		outValue = characteristicValueBufferWinAPI->Data[0];
		return true;
	}

	return false;
}

bool WinBLEGattCharacteristicValue::getData(unsigned char *outBuffer, size_t outBufferSize)
{
	if (characteristicValueBufferWinAPI->DataSize > 0 &&
		characteristicValueBufferWinAPI->DataSize <= outBufferSize)
	{
		memcpy(outBuffer, characteristicValueBufferWinAPI->Data, outBufferSize);
		return true;
	}

	return false;
}

bool WinBLEGattCharacteristicValue::setByte(unsigned char inValue)
{
	if (characteristicValueBufferWinAPI->DataSize > 0)
	{
		characteristicValueBufferWinAPI->Data[0] = inValue;
		return true;
	}

	return false;
}

bool WinBLEGattCharacteristicValue::setData(const unsigned char *inBuffer, size_t inBufferSize)
{
	if (inBufferSize > 0 && inBufferSize <= characteristicValueBufferWinAPI->DataSize)
	{
		memcpy(characteristicValueBufferWinAPI->Data, inBuffer, inBufferSize);
		return true;
	}

	return false;
}

//-- WinBLEGattCharacteristic -----
WinBLEGattDescriptor::WinBLEGattDescriptor(BLEGattCharacteristic *characteristic, const BluetoothUUID &uuid, BTH_LE_GATT_DESCRIPTOR *descriptor)
	: BLEGattDescriptor(characteristic, uuid)
	, descriptorWinAPI(descriptor)
{
	switch (descriptor->DescriptorType)
	{
	case CharacteristicExtendedProperties:
		this->descriptorType = BLEGattDescriptor::DescriptorType::CharacteristicExtendedProperties;
		break;
	case CharacteristicUserDescription:
		this->descriptorType = BLEGattDescriptor::DescriptorType::CharacteristicUserDescription;
		break;
	case ClientCharacteristicConfiguration:
		this->descriptorType = BLEGattDescriptor::DescriptorType::ClientCharacteristicConfiguration;
		break;
	case ServerCharacteristicConfiguration:
		this->descriptorType = BLEGattDescriptor::DescriptorType::ServerCharacteristicConfiguration;
		break;
	case CharacteristicFormat:
		this->descriptorType = BLEGattDescriptor::DescriptorType::CharacteristicFormat;
		break;
	case CharacteristicAggregateFormat:
		this->descriptorType = BLEGattDescriptor::DescriptorType::CharacteristicAggregateFormat;
		break;
	case CustomDescriptor:
		this->descriptorType = BLEGattDescriptor::DescriptorType::CustomDescriptor;
		break;
	}

	attributeHandle = BluetoothGattHandle((unsigned short)descriptor->AttributeHandle);
}

HANDLE WinBLEGattDescriptor::getDeviceHandle() const
{
	return static_cast<WinBLEGattCharacteristic *>(parentCharacteristic)->getDeviceHandle();
}

BTH_LE_GATT_DESCRIPTOR *WinBLEGattDescriptor::getDescriptorWinAPI()
{
	return descriptorWinAPI;
}

bool WinBLEGattDescriptor::fetchDescriptorValue()
{
	bool bSuccess = true;

	freeDescriptorValue();

	// Determine Descriptor Value Buffer Size
	USHORT descValueRequiredSize = 0;
	if (bSuccess)
	{
		HRESULT hr = BluetoothGATTGetDescriptorValue(
			getDeviceHandle(),
			descriptorWinAPI,
			0,
			NULL,
			&descValueRequiredSize,
			BLUETOOTH_GATT_FLAG_NONE);

		if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA))
		{
			bSuccess = false;
		}
	}

	if (bSuccess)
	{
		descriptorValueBufferWinAPI = (PBTH_LE_GATT_DESCRIPTOR_VALUE)malloc(descValueRequiredSize);
		assert(descriptorValueBufferWinAPI != nullptr);
		memset(descriptorValueBufferWinAPI, 0, descValueRequiredSize);

		descriptorValue = new WinBLEGattDescriptorValue(this, descriptorValueBufferWinAPI);

		bSuccess = descriptorValue->readDescriptorValue();
	}

	return bSuccess;
}

void WinBLEGattDescriptor::freeDescriptorValue()
{
	if (descriptorValue != nullptr)
	{
		delete descriptorValue;
		descriptorValue = nullptr;
	}

	if (descriptorValueBufferWinAPI != nullptr)
	{
		free(descriptorValueBufferWinAPI);
		descriptorValueBufferWinAPI = nullptr;
	}
}

//-- WinBLEGattDescriptorValue -----
WinBLEGattDescriptorValue::WinBLEGattDescriptorValue(BLEGattDescriptor *descriptor, BTH_LE_GATT_DESCRIPTOR_VALUE *descriptorValue)
	: BLEGattDescriptorValue(descriptor)
	, descriptorValueWinAPI(descriptorValue)
{ }

WinBLEGattDescriptor* WinBLEGattDescriptorValue::getParentDescriptor() const
{
	return static_cast<WinBLEGattDescriptor *>(parentDescriptor);
}

HANDLE WinBLEGattDescriptorValue::getDeviceHandle() const
{
	return getParentDescriptor()->getDeviceHandle();
}

BTH_LE_GATT_DESCRIPTOR *WinBLEGattDescriptorValue::getDescriptorWinAPI() const
{
	return getParentDescriptor()->getDescriptorWinAPI();
}

bool WinBLEGattDescriptorValue::readDescriptorValue()
{
	HRESULT hr = BluetoothGATTGetDescriptorValue(
		getDeviceHandle(),
		getDescriptorWinAPI(),
		descriptorValueWinAPI->DataSize,
		descriptorValueWinAPI,
		NULL,
		BLUETOOTH_GATT_FLAG_NONE);

	return hr == S_OK;
}

bool WinBLEGattDescriptorValue::writeDescriptorValue()
{
	HRESULT hr = BluetoothGATTSetDescriptorValue(
		getDeviceHandle(),
		getDescriptorWinAPI(),
		descriptorValueWinAPI,
		BLUETOOTH_GATT_FLAG_NONE);

	return hr == S_OK;
}

bool WinBLEGattDescriptorValue::getCharacteristicExtendedProperties(BLEGattDescriptorValue::CharacteristicExtendedProperties &outProps) const
{
	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::CharacteristicExtendedProperties)
	{
		outProps.IsAuxiliariesWritable = descriptorValueWinAPI->CharacteristicExtendedProperties.IsAuxiliariesWritable;
		outProps.IsReliableWriteEnabled = descriptorValueWinAPI->CharacteristicExtendedProperties.IsReliableWriteEnabled;

		return true;
	}

	return true;
}

bool WinBLEGattDescriptorValue::getClientCharacteristicConfiguration(BLEGattDescriptorValue::ClientCharacteristicConfiguration &outConfig) const
{
	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::ClientCharacteristicConfiguration)
	{
		outConfig.IsSubscribeToIndication = descriptorValueWinAPI->ClientCharacteristicConfiguration.IsSubscribeToIndication;
		outConfig.IsSubscribeToNotification = descriptorValueWinAPI->ClientCharacteristicConfiguration.IsSubscribeToNotification;
		return true;
	}

	return true;
}

bool WinBLEGattDescriptorValue::getServerCharacteristicConfiguration(BLEGattDescriptorValue::ServerCharacteristicConfiguration &outConfig) const
{
	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::ServerCharacteristicConfiguration)
	{
		outConfig.IsBroadcast = descriptorValueWinAPI->ServerCharacteristicConfiguration.IsBroadcast;
		return true;
	}

	return true;
}

bool WinBLEGattDescriptorValue::getCharacteristicFormat(BLEGattDescriptorValue::CharacteristicFormat &outFormat) const
{
	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::CharacteristicFormat)
	{
		outFormat.Description = WinBluetoothUUID(descriptorValueWinAPI->CharacteristicFormat.Description);
		outFormat.Exponent = descriptorValueWinAPI->CharacteristicFormat.Exponent;
		outFormat.Format = descriptorValueWinAPI->CharacteristicFormat.Format;
		outFormat.NameSpace = descriptorValueWinAPI->CharacteristicFormat.NameSpace;
		outFormat.Unit = WinBluetoothUUID(descriptorValueWinAPI->CharacteristicFormat.Unit);

		return true;
	}

	return true;
}

bool WinBLEGattDescriptorValue::setCharacteristicExtendedProperties(const BLEGattDescriptorValue::CharacteristicExtendedProperties &inProps) const
{
	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::CharacteristicExtendedProperties)
	{
		descriptorValueWinAPI->CharacteristicExtendedProperties.IsAuxiliariesWritable = inProps.IsAuxiliariesWritable;
		descriptorValueWinAPI->CharacteristicExtendedProperties.IsReliableWriteEnabled = inProps.IsReliableWriteEnabled;

		return true;
	}

	return true;
}

bool WinBLEGattDescriptorValue::setClientCharacteristicConfiguration(const BLEGattDescriptorValue::ClientCharacteristicConfiguration &inConfig) const
{
	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::ClientCharacteristicConfiguration)
	{
		descriptorValueWinAPI->ClientCharacteristicConfiguration.IsSubscribeToIndication = inConfig.IsSubscribeToIndication;
		descriptorValueWinAPI->ClientCharacteristicConfiguration.IsSubscribeToNotification = inConfig.IsSubscribeToNotification;

		return true;
	}

	return true;
}

bool WinBLEGattDescriptorValue::setServerCharacteristicConfiguration(const BLEGattDescriptorValue::ServerCharacteristicConfiguration &inConfig) const
{
	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::ServerCharacteristicConfiguration)
	{
		descriptorValueWinAPI->ServerCharacteristicConfiguration.IsBroadcast = inConfig.IsBroadcast;

		return true;
	}

	return true;
}

bool WinBLEGattDescriptorValue::setCharacteristicFormat(const BLEGattDescriptorValue::CharacteristicFormat &inFormat) const
{
	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::CharacteristicFormat)
	{
		WinBluetoothUUID(inFormat.Description).ToWinAPI(descriptorValueWinAPI->CharacteristicFormat.Description);
		descriptorValueWinAPI->CharacteristicFormat.Exponent = inFormat.Exponent;
		descriptorValueWinAPI->CharacteristicFormat.Format = inFormat.Format;
		descriptorValueWinAPI->CharacteristicFormat.NameSpace = inFormat.NameSpace;
		WinBluetoothUUID(inFormat.Unit).ToWinAPI(descriptorValueWinAPI->CharacteristicFormat.Unit);

		return true;
	}

	return true;
}

bool WinBLEGattDescriptorValue::getByte(unsigned char &outValue)
{
	if (descriptorValueWinAPI->DataSize > 0)
	{
		outValue = descriptorValueWinAPI->Data[0];
		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::getData(unsigned char *outBuffer, size_t outBufferSize)
{
	if (descriptorValueWinAPI->DataSize > 0 &&
		descriptorValueWinAPI->DataSize <= outBufferSize)
	{
		memcpy(outBuffer, descriptorValueWinAPI->Data, outBufferSize);
		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::setByte(unsigned char inValue)
{
	if (descriptorValueWinAPI->DataSize > 0)
	{
		descriptorValueWinAPI->Data[0] = inValue;
		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::setData(const unsigned char *inBuffer, size_t inBufferSize)
{
	if (inBufferSize > 0 && inBufferSize <= descriptorValueWinAPI->DataSize)
	{
		memcpy(descriptorValueWinAPI->Data, inBuffer, inBufferSize);
		return true;
	}

	return false;
}