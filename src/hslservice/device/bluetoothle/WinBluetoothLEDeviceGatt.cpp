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

static bool IsValidGattBufferResult(HRESULT hr, size_t required_count)
{
	// required_count > 0
	if (hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA))
	{
		return true;
	}

	// required_count = 0 (list doesn't exist)
	if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
	{
		return true;
	}

	// required_count = 0 (list does exist and is empty)
	if (SUCCEEDED(hr) && required_count == 0)
	{
		return true;
	}

	return false;
}

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

		const int MAX_BUF = 36 + 1;
		char szBuffer[MAX_BUF];
		int length = 0;
		length += snprintf(szBuffer, 
			(size_t)MAX_BUF, 
			"%08x-%04x-%04x-", 
			guid.Data1, guid.Data2, guid.Data3);

		for (int i = 0; i < sizeof(guid.Data4); i++) 
		{
			if (i == 2)
			{
				length += snprintf(szBuffer + length, (size_t)(MAX_BUF - length), "-");
			}

			length += snprintf(szBuffer + length, (size_t)(MAX_BUF - length), "%02x", guid.Data4[i]);
		}

		setUUID(std::string(szBuffer));
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
	, m_deviceHandle(device->getDeviceHandle())
	, serviceBufferWinAPI(nullptr)
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
			m_deviceHandle,
			0,
			NULL,
			&serviceBufferRequiredCount,
			BLUETOOTH_GATT_FLAG_NONE);

		if (!IsValidGattBufferResult(hr, serviceBufferRequiredCount))
		{
			bSuccess = false;
		}
	}

	// Fetch the service list from the windows BluetoothLE API.
	// Create a WinBLEGattService wrapper instance for each entry in the service list.
	if (bSuccess && serviceBufferRequiredCount > 0)
	{
		serviceBufferWinAPI = (PBTH_LE_GATT_SERVICE)malloc(sizeof(BTH_LE_GATT_SERVICE) * serviceBufferRequiredCount);
		assert(serviceBufferWinAPI != nullptr);
		memset(serviceBufferWinAPI, 0, sizeof(BTH_LE_GATT_SERVICE) * serviceBufferRequiredCount);

		USHORT servicesBufferActualCount = serviceBufferRequiredCount;
		HRESULT hr = BluetoothGATTGetServices(
			m_deviceHandle,
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

//-- WinBLEGattService -----
WinBLEGattService::WinBLEGattService(WinBLEGattProfile *profile, const BluetoothUUID &uuid, BTH_LE_GATT_SERVICE *service)
	: BLEGattService(profile, uuid)
	, m_serviceDeviceHandle(nullptr)
	, serviceWinAPI(service)
	, characteristicBufferWinAPI(nullptr)
{
	WinBLEDeviceState *deviceState= static_cast<WinBLEDeviceState *>(profile->getParentDevice());

	m_serviceDeviceHandle= deviceState->getServiceDeviceHandle(uuid);
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
			m_serviceDeviceHandle,
			serviceWinAPI,
			0,
			NULL,
			&characteristicsBufferRequiredCount,
			BLUETOOTH_GATT_FLAG_NONE);

		if (!IsValidGattBufferResult(hr, characteristicsBufferRequiredCount))
		{
			bSuccess = false;
		}
	}

	// Fetch the characteristics list from the windows BluetoothLE API.
	// Create a WinBLEGattCharacteristic wrapper instance for each entry in the characteristics list.
	if (bSuccess && characteristicsBufferRequiredCount > 0)
	{
		characteristicBufferWinAPI =
			(PBTH_LE_GATT_CHARACTERISTIC)malloc(sizeof(BTH_LE_GATT_CHARACTERISTIC) * characteristicsBufferRequiredCount);
		assert(characteristicBufferWinAPI != nullptr);
		memset(characteristicBufferWinAPI, 0, sizeof(BTH_LE_GATT_CHARACTERISTIC) * characteristicsBufferRequiredCount);

		USHORT characteristicsBufferActualCount = characteristicsBufferRequiredCount;
		HRESULT hr = BluetoothGATTGetCharacteristics(
			m_serviceDeviceHandle,
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

			if (!characteristicWrapper->fetchDescriptors())
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

//-- WinBLEGattCharacteristic -----
WinBLEGattCharacteristic::WinBLEGattCharacteristic(BLEGattService *service, const BluetoothUUID &uuid, BTH_LE_GATT_CHARACTERISTIC *characteristic)
	: BLEGattCharacteristic(service, uuid)
	, characteristicWinAPI(characteristic)
	, descriptorBufferWinAPI(nullptr)
{
	characteristicValue= new WinBLEGattCharacteristicValue(this);
}

WinBLEGattCharacteristic::~WinBLEGattCharacteristic()
{
	delete characteristicValue;
	characteristicValue= nullptr;

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
			getServiceDeviceHandle(),
			characteristicWinAPI,
			0,
			NULL,
			&descriptorBufferRequiredCount,
			BLUETOOTH_GATT_FLAG_NONE);

		if (!IsValidGattBufferResult(hr, descriptorBufferRequiredCount))
		{
			bSuccess = false;
		}
	}

	// Fetch the descriptor list from the windows BluetoothLE API.
	// Create a WinBLEGattDescriptor wrapper instance for each entry in the characteristics list.
	if (bSuccess && descriptorBufferRequiredCount > 0)
	{
		descriptorBufferWinAPI =
			(PBTH_LE_GATT_DESCRIPTOR)malloc(sizeof(BTH_LE_GATT_DESCRIPTOR) * descriptorBufferRequiredCount);
		assert(descriptorBufferWinAPI != nullptr);
		memset(descriptorBufferWinAPI, 0, sizeof(BTH_LE_GATT_DESCRIPTOR) * descriptorBufferRequiredCount);

		USHORT descriptorBufferActualCount = descriptorBufferRequiredCount;
		HRESULT hr = BluetoothGATTGetDescriptors(
			getServiceDeviceHandle(),
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

HANDLE WinBLEGattCharacteristic::getServiceDeviceHandle() const
{
	return static_cast<WinBLEGattService *>(parentService)->getServiceDeviceHandle();
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
			getServiceDeviceHandle(),
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
			uint8_t *data= params->CharacteristicValue->Data;
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

//-- WinBLEGattCharacteristicValue -----
WinBLEGattCharacteristicValue::WinBLEGattCharacteristicValue(BLEGattCharacteristic *characteristic)
	: BLEGattCharacteristicValue(characteristic)
	, characteristicValueBufferWinAPI(nullptr)
	, maxTailBufferSize(0)
{ }

WinBLEGattCharacteristicValue::~WinBLEGattCharacteristicValue()
{
	if (characteristicValueBufferWinAPI != nullptr)
	{
		free(characteristicValueBufferWinAPI);
		characteristicValueBufferWinAPI = nullptr;
	}
}

WinBLEGattCharacteristic* WinBLEGattCharacteristicValue::getParentCharacteristic() const
{
	return static_cast<WinBLEGattCharacteristic *>(parentCharacteristic);
}

HANDLE WinBLEGattCharacteristicValue::getServiceDeviceHandle() const
{
	return getParentCharacteristic()->getServiceDeviceHandle();
}

BTH_LE_GATT_CHARACTERISTIC *WinBLEGattCharacteristicValue::getCharacteristicWinAPI() const
{
	return getParentCharacteristic()->getCharacteristicWinAPI();
}

bool WinBLEGattCharacteristicValue::getByte(uint8_t &outValue)
{
	uint8_t *buffer= nullptr;
	size_t buffer_size= 0;
	if (getData(&buffer, &buffer_size))
	{
		if (buffer_size >= 1)
		{
			outValue= buffer[0];
			return true;
		}
	}

	return false;
}

bool WinBLEGattCharacteristicValue::getData(uint8_t **outBuffer, size_t *outBufferSize)
{
	assert(outBuffer != nullptr);
	assert(outBufferSize != nullptr);

	// Determine Descriptor Value Buffer Size
	USHORT newCharacteristicValueRequiredSize = 0;
	HRESULT hr = BluetoothGATTGetCharacteristicValue(
		getServiceDeviceHandle(),
		getParentCharacteristic()->getCharacteristicWinAPI(),
		0,
		NULL,
		&newCharacteristicValueRequiredSize,
		BLUETOOTH_GATT_FLAG_NONE);
	if (!IsValidGattBufferResult(hr, newCharacteristicValueRequiredSize))
	{
		return false;
	}

	if (newCharacteristicValueRequiredSize > 0)
	{
		// Make sure the Characteristic Value's tail buffer is big enough
		ensureTailBufferSize(getTailBufferSize(newCharacteristicValueRequiredSize));

		// Actually read the characteristic value
		hr = BluetoothGATTGetCharacteristicValue(
			getServiceDeviceHandle(),
			getCharacteristicWinAPI(),
			newCharacteristicValueRequiredSize,
			characteristicValueBufferWinAPI,
			NULL,
			BLUETOOTH_GATT_FLAG_NONE);
		if (FAILED(hr))
		{
			return false;
		}

		*outBuffer = characteristicValueBufferWinAPI->Data;
		*outBufferSize = (size_t)characteristicValueBufferWinAPI->DataSize;
	}
	else
	{
		*outBuffer = nullptr;
		*outBufferSize = 0;
	}

	return true;
}

bool WinBLEGattCharacteristicValue::setByte(uint8_t inValue)
{
	return setData(&inValue, 1);
}

bool WinBLEGattCharacteristicValue::setData(const uint8_t *inBuffer, size_t inBufferSize)
{
	if (inBufferSize == 0)
		return false;

	// Make sure the Characteristic Value's tail buffer is big enough
	ensureTailBufferSize(inBufferSize);

	// Copy inBuffer into the BTH_LE_GATT_CHARACTERISTIC_VALUE data tail buffer
	characteristicValueBufferWinAPI->DataSize= (ULONG)inBufferSize;
	memcpy(&characteristicValueBufferWinAPI->Data, inBuffer, inBufferSize);

	ULONG flag = BLUETOOTH_GATT_FLAG_NONE;
	if (!getCharacteristicWinAPI()->IsWritable) 
	{
		assert(getCharacteristicWinAPI()->IsWritableWithoutResponse);
		flag |= BLUETOOTH_GATT_FLAG_WRITE_WITHOUT_RESPONSE;
	}

	// Set the new characteristic value
	HRESULT hr = BluetoothGATTSetCharacteristicValue(
		getServiceDeviceHandle(),
		getCharacteristicWinAPI(),
		characteristicValueBufferWinAPI,
		NULL,
		flag);

	return SUCCEEDED(hr);
}

size_t WinBLEGattCharacteristicValue::getTailBufferSize(size_t characteristic_value_size)
{
	return characteristic_value_size - offsetof(BTH_LE_GATT_CHARACTERISTIC_VALUE,Data);
}

size_t WinBLEGattCharacteristicValue::getCharacteristicValueSize(size_t tail_buffer_size)
{
	return offsetof(BTH_LE_GATT_CHARACTERISTIC_VALUE,Data) + tail_buffer_size;
}

void WinBLEGattCharacteristicValue::ensureTailBufferSize(size_t tail_buffer_size)
{
	// If the existing characteristicValue's tail buffer is too small, delete it
	if (characteristicValueBufferWinAPI != nullptr &&
		tail_buffer_size > maxTailBufferSize)
	{
		free(characteristicValueBufferWinAPI);
		characteristicValueBufferWinAPI = nullptr;
	}

	// Create a characteristicValueBuffer with a tail buffer that big enough for tail_buffer_size
	if (characteristicValueBufferWinAPI == nullptr)
	{
		const size_t value_size= WinBLEGattCharacteristicValue::getCharacteristicValueSize(tail_buffer_size);

		characteristicValueBufferWinAPI = (PBTH_LE_GATT_CHARACTERISTIC_VALUE)malloc(value_size);
		assert(characteristicValueBufferWinAPI != nullptr);

		maxTailBufferSize= tail_buffer_size;
	}
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
	descriptorValue = new WinBLEGattDescriptorValue(this);
}

HANDLE WinBLEGattDescriptor::getServiceDeviceHandle() const
{
	return static_cast<WinBLEGattCharacteristic *>(parentCharacteristic)->getServiceDeviceHandle();
}

BTH_LE_GATT_DESCRIPTOR *WinBLEGattDescriptor::getDescriptorWinAPI()
{
	return descriptorWinAPI;
}

//-- WinBLEGattDescriptorValue -----
WinBLEGattDescriptorValue::WinBLEGattDescriptorValue(BLEGattDescriptor *descriptor)
	: BLEGattDescriptorValue(descriptor)
	, descriptorValueBufferWinAPI(nullptr)
	, maxTailBufferSize(0)
{ }

WinBLEGattDescriptorValue::~WinBLEGattDescriptorValue()
{
	if (descriptorValueBufferWinAPI != nullptr)
	{
		free(descriptorValueBufferWinAPI);
		descriptorValueBufferWinAPI= nullptr;
	}
}

WinBLEGattDescriptor* WinBLEGattDescriptorValue::getParentDescriptor() const
{
	return static_cast<WinBLEGattDescriptor *>(parentDescriptor);
}

HANDLE WinBLEGattDescriptorValue::getServiceDeviceHandle() const
{
	return getParentDescriptor()->getServiceDeviceHandle();
}

BTH_LE_GATT_DESCRIPTOR *WinBLEGattDescriptorValue::getDescriptorWinAPI() const
{
	return getParentDescriptor()->getDescriptorWinAPI();
}

bool WinBLEGattDescriptorValue::readDescriptorValue()
{
	// Determine Descriptor Value Buffer Size
	USHORT newDescriptorValueRequiredSize = 0;
	HRESULT hr = BluetoothGATTGetDescriptorValue(
		getServiceDeviceHandle(),
		getDescriptorWinAPI(),
		0,
		NULL,
		&newDescriptorValueRequiredSize,
		BLUETOOTH_GATT_FLAG_NONE);
	if (!IsValidGattBufferResult(hr, newDescriptorValueRequiredSize))
	{
		return false;
	}

	// Make sure the Descriptor Value's tail buffer is big enough
	ensureTailBufferSize(getTailBufferSize(newDescriptorValueRequiredSize));

	// Actually read the characteristic value
	hr = BluetoothGATTGetDescriptorValue(
		getServiceDeviceHandle(),
		getDescriptorWinAPI(),
		newDescriptorValueRequiredSize,
		descriptorValueBufferWinAPI,
		NULL,
		BLUETOOTH_GATT_FLAG_NONE);

	return hr == S_OK;
}

bool WinBLEGattDescriptorValue::writeDescriptorValue()
{
	HRESULT hr = BluetoothGATTSetDescriptorValue(
		getServiceDeviceHandle(),
		getDescriptorWinAPI(),
		descriptorValueBufferWinAPI,
		BLUETOOTH_GATT_FLAG_NONE);

	return hr == S_OK;
}

bool WinBLEGattDescriptorValue::getCharacteristicExtendedProperties(BLEGattDescriptorValue::CharacteristicExtendedProperties &outProps) const
{
	if (descriptorValueBufferWinAPI != nullptr &&
		getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::CharacteristicExtendedProperties)
	{
		outProps.IsAuxiliariesWritable = descriptorValueBufferWinAPI->CharacteristicExtendedProperties.IsAuxiliariesWritable;
		outProps.IsReliableWriteEnabled = descriptorValueBufferWinAPI->CharacteristicExtendedProperties.IsReliableWriteEnabled;

		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::getClientCharacteristicConfiguration(BLEGattDescriptorValue::ClientCharacteristicConfiguration &outConfig) const
{
	if (descriptorValueBufferWinAPI != nullptr &&
		getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::ClientCharacteristicConfiguration)
	{
		outConfig.IsSubscribeToIndication = descriptorValueBufferWinAPI->ClientCharacteristicConfiguration.IsSubscribeToIndication;
		outConfig.IsSubscribeToNotification = descriptorValueBufferWinAPI->ClientCharacteristicConfiguration.IsSubscribeToNotification;
		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::getServerCharacteristicConfiguration(BLEGattDescriptorValue::ServerCharacteristicConfiguration &outConfig) const
{
	if (descriptorValueBufferWinAPI != nullptr &&
		getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::ServerCharacteristicConfiguration)
	{
		outConfig.IsBroadcast = descriptorValueBufferWinAPI->ServerCharacteristicConfiguration.IsBroadcast;
		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::getCharacteristicFormat(BLEGattDescriptorValue::CharacteristicFormat &outFormat) const
{
	if (descriptorValueBufferWinAPI != nullptr &&
		getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::CharacteristicFormat)
	{
		outFormat.Description = WinBluetoothUUID(descriptorValueBufferWinAPI->CharacteristicFormat.Description);
		outFormat.Exponent = descriptorValueBufferWinAPI->CharacteristicFormat.Exponent;
		outFormat.Format = descriptorValueBufferWinAPI->CharacteristicFormat.Format;
		outFormat.NameSpace = descriptorValueBufferWinAPI->CharacteristicFormat.NameSpace;
		outFormat.Unit = WinBluetoothUUID(descriptorValueBufferWinAPI->CharacteristicFormat.Unit);

		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::setCharacteristicExtendedProperties(const BLEGattDescriptorValue::CharacteristicExtendedProperties &inProps)
{
	ensureTailBufferSize(0);

	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::CharacteristicExtendedProperties)
	{
		descriptorValueBufferWinAPI->CharacteristicExtendedProperties.IsAuxiliariesWritable = inProps.IsAuxiliariesWritable;
		descriptorValueBufferWinAPI->CharacteristicExtendedProperties.IsReliableWriteEnabled = inProps.IsReliableWriteEnabled;

		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::setClientCharacteristicConfiguration(const BLEGattDescriptorValue::ClientCharacteristicConfiguration &inConfig) 
{
	ensureTailBufferSize(0);

	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::ClientCharacteristicConfiguration)
	{
		descriptorValueBufferWinAPI->ClientCharacteristicConfiguration.IsSubscribeToIndication = inConfig.IsSubscribeToIndication;
		descriptorValueBufferWinAPI->ClientCharacteristicConfiguration.IsSubscribeToNotification = inConfig.IsSubscribeToNotification;

		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::setServerCharacteristicConfiguration(const BLEGattDescriptorValue::ServerCharacteristicConfiguration &inConfig)
{
	ensureTailBufferSize(0);

	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::ServerCharacteristicConfiguration)
	{
		descriptorValueBufferWinAPI->ServerCharacteristicConfiguration.IsBroadcast = inConfig.IsBroadcast;

		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::setCharacteristicFormat(const BLEGattDescriptorValue::CharacteristicFormat &inFormat) 
{
	ensureTailBufferSize(0);

	if (getParentDescriptor()->getDescriptorType() == BLEGattDescriptor::DescriptorType::CharacteristicFormat)
	{
		WinBluetoothUUID(inFormat.Description).ToWinAPI(descriptorValueBufferWinAPI->CharacteristicFormat.Description);
		descriptorValueBufferWinAPI->CharacteristicFormat.Exponent = inFormat.Exponent;
		descriptorValueBufferWinAPI->CharacteristicFormat.Format = inFormat.Format;
		descriptorValueBufferWinAPI->CharacteristicFormat.NameSpace = inFormat.NameSpace;
		WinBluetoothUUID(inFormat.Unit).ToWinAPI(descriptorValueBufferWinAPI->CharacteristicFormat.Unit);

		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::getByte(uint8_t &outValue) const
{
	uint8_t* buffer = nullptr;
	size_t buffer_size = 0;
	if (getData(&buffer, &buffer_size))
	{
		if (buffer_size >= 1)
		{
			outValue = buffer[0];
			return true;
		}
	}

	return false;
}

bool WinBLEGattDescriptorValue::getData(uint8_t **outBuffer, size_t *outBufferSize) const
{
	assert(outBuffer != nullptr);
	assert(outBufferSize != nullptr);

	if (descriptorValueBufferWinAPI != nullptr)
	{
		*outBuffer = descriptorValueBufferWinAPI->Data;
		*outBufferSize = (size_t)descriptorValueBufferWinAPI->DataSize;

		return true;
	}

	return false;
}

bool WinBLEGattDescriptorValue::setByte(uint8_t inValue)
{
	return setData(&inValue, 1);
}

bool WinBLEGattDescriptorValue::setData(const uint8_t *inBuffer, size_t inBufferSize)
{
	// Make sure the Characteristic Value's tail buffer is big enough
	ensureTailBufferSize(inBufferSize);

	// Copy inBuffer into the BTH_LE_GATT_DESCRIPTOR_VALUE data tail buffer
	if (inBuffer != nullptr && inBufferSize > 0)
	{
		descriptorValueBufferWinAPI->DataSize = (ULONG)inBufferSize;
		memcpy(&descriptorValueBufferWinAPI->Data, inBuffer, inBufferSize);
	}
	else
	{
		descriptorValueBufferWinAPI->DataSize = 0;
	}

	// Set the new characteristic value
	return true;
}

size_t WinBLEGattDescriptorValue::getTailBufferSize(size_t descriptor_value_size)
{
	return descriptor_value_size - offsetof(BTH_LE_GATT_DESCRIPTOR_VALUE, Data);
}

size_t WinBLEGattDescriptorValue::getDescriptorValueSize(size_t tail_buffer_size)
{
	size_t offset_to_tail_buffer= offsetof(BTH_LE_GATT_DESCRIPTOR_VALUE, Data);

	return max(offset_to_tail_buffer + tail_buffer_size, sizeof(BTH_LE_GATT_DESCRIPTOR_VALUE));
}

void WinBLEGattDescriptorValue::ensureTailBufferSize(size_t tail_buffer_size)
{
	BTH_LE_GATT_DESCRIPTOR_VALUE *oldDescriptorValueWinAPI= nullptr;

	// If the existing descriptorValue's tail buffer is too small, delete it
	if (descriptorValueBufferWinAPI != nullptr &&
		tail_buffer_size > maxTailBufferSize)
	{
		oldDescriptorValueWinAPI= descriptorValueBufferWinAPI;
		descriptorValueBufferWinAPI = nullptr;
	}

	// Create a characteristicValueBuffer with a tail buffer that big enough for tail_buffer_size
	if (descriptorValueBufferWinAPI == nullptr)
	{
		const size_t value_size = WinBLEGattDescriptorValue::getDescriptorValueSize(tail_buffer_size);

		descriptorValueBufferWinAPI = (PBTH_LE_GATT_DESCRIPTOR_VALUE)malloc(value_size);
		assert(descriptorValueBufferWinAPI != nullptr);
		memset(descriptorValueBufferWinAPI, 0, value_size);

		// If we had a previous descriptor value
		// copy over the stuff before the tail buffer
		if (oldDescriptorValueWinAPI != nullptr)
		{
			memcpy(descriptorValueBufferWinAPI, oldDescriptorValueWinAPI, sizeof(BTH_LE_GATT_DESCRIPTOR_VALUE));
		}

		maxTailBufferSize = tail_buffer_size;
	}

	// Clean up the old value, if we created a new one
	if (oldDescriptorValueWinAPI != nullptr)
	{
		free(oldDescriptorValueWinAPI);
	}

}