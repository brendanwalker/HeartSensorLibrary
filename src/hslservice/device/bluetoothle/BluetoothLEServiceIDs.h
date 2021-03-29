#ifndef BLUETOOTH_LE_SERVICE_IDS_H
#define BLUETOOTH_LE_SERVICE_IDS_H

// -- includes -----
#include "BluetoothUUID.h"

// -- constants -----
extern const BluetoothUUID *k_Service_DeviceInformation_UUID;
extern const BluetoothUUID *k_Characteristic_SystemID_UUID;
extern const BluetoothUUID *k_Characteristic_ModelNumberString_UUID;
extern const BluetoothUUID *k_Characteristic_SerialNumberString_UUID;
extern const BluetoothUUID *k_Characteristic_FirmwareRevisionString_UUID;
extern const BluetoothUUID *k_Characteristic_HardwareRevisionString_UUID;
extern const BluetoothUUID *k_Characteristic_SoftwareRevisionString_UUID;
extern const BluetoothUUID *k_Characteristic_ManufacturerNameString_UUID;

extern const BluetoothUUID *k_Service_Battery_UUID;
extern const BluetoothUUID *k_Characteristic_BatteryLevel_UUID;
extern const BluetoothUUID *k_Descriptor_BatteryLevel_UUID;

extern const BluetoothUUID *k_Service_HeartRate_UUID;
extern const BluetoothUUID *k_Characteristic_HeartRateMeasurement_UUID;
extern const BluetoothUUID *k_Descriptor_HeartRateMeasurement_UUID;
extern const BluetoothUUID *k_Characteristic_BodySensorLocation_UUID;
extern const BluetoothUUID *k_Characteristic_HeartRateControlPoint_UUID;

extern const BluetoothUUID *k_Service_EDA_UUID;
extern const BluetoothUUID *k_Characteristic_EDA_Measurement_UUID;
extern const BluetoothUUID *k_Descriptor_EDA_Measurement_UUID;
extern const BluetoothUUID *k_Characteristic_EDA_Period_UUID;

#endif // BLUETOOTH_LE_SERVICE_IDS_H

