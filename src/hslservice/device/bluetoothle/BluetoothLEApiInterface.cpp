#include "BluetoothLEApiInterface.h"
#include <algorithm>

// -- BLEDeviceState ----
BluetoothLEDeviceState::BluetoothLEDeviceState()
	: deviceHandle(k_invalid_ble_device_handle)
{
}

BluetoothLEDeviceState::~BluetoothLEDeviceState()
{
	if (gattProfile != nullptr)
	{
		delete gattProfile;
	}
}