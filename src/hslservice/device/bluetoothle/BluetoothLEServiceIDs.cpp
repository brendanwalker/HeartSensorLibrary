//-- includes -----
#include "BluetoothLEServiceIDs.h"

//-- Services -----

//-- Device Information Service --
const BluetoothUUID g_Service_DeviceInformation_UUID("180A");
const BluetoothUUID *k_Service_DeviceInformation_UUID= &g_Service_DeviceInformation_UUID;

const BluetoothUUID g_Characteristic_SystemID_UUID("2A23");
const BluetoothUUID *k_Characteristic_SystemID_UUID= &g_Characteristic_SystemID_UUID;

const BluetoothUUID g_Characteristic_ModelNumberString_UUID("2A24");
const BluetoothUUID *k_Characteristic_ModelNumberString_UUID= &g_Characteristic_ModelNumberString_UUID;

const BluetoothUUID g_Characteristic_SerialNumberString_UUID("2A25");
const BluetoothUUID *k_Characteristic_SerialNumberString_UUID= &g_Characteristic_SerialNumberString_UUID;

const BluetoothUUID g_Characteristic_FirmwareRevisionString_UUID("2A26");
const BluetoothUUID *k_Characteristic_FirmwareRevisionString_UUID= &g_Characteristic_FirmwareRevisionString_UUID;

const BluetoothUUID g_Characteristic_HardwareRevisionString_UUID("2A27");
const BluetoothUUID *k_Characteristic_HardwareRevisionString_UUID= &g_Characteristic_HardwareRevisionString_UUID;

const BluetoothUUID g_Characteristic_SoftwareRevisionString_UUID("2A28");
const BluetoothUUID *k_Characteristic_SoftwareRevisionString_UUID= &g_Characteristic_SoftwareRevisionString_UUID;

const BluetoothUUID g_Characteristic_ManufacturerNameString_UUID("2A29");
const BluetoothUUID *k_Characteristic_ManufacturerNameString_UUID= &g_Characteristic_ManufacturerNameString_UUID;

//-- Battery Service --
const BluetoothUUID g_Service_Battery_UUID("180F");
const BluetoothUUID *k_Service_Battery_UUID= &g_Service_Battery_UUID;
const BluetoothUUID g_Characteristic_BatteryLevel_UUID("2A19");
const BluetoothUUID *k_Characteristic_BatteryLevel_UUID= &g_Characteristic_BatteryLevel_UUID;
const BluetoothUUID g_Descriptor_BatteryLevel_UUID("2902");
const BluetoothUUID *k_Descriptor_BatteryLevel_UUID= &g_Descriptor_BatteryLevel_UUID;

//-- Heart Rate Service --
const BluetoothUUID g_Service_HeartRate_UUID("180D");
const BluetoothUUID *k_Service_HeartRate_UUID= &g_Service_HeartRate_UUID;
const BluetoothUUID g_Characteristic_HeartRateMeasurement_UUID("2A37");
const BluetoothUUID *k_Characteristic_HeartRateMeasurement_UUID= &g_Characteristic_HeartRateMeasurement_UUID;
const BluetoothUUID g_Descriptor_HeartRateMeasurement_UUID("2902");
const BluetoothUUID *k_Descriptor_HeartRateMeasurement_UUID= &g_Descriptor_HeartRateMeasurement_UUID;
const BluetoothUUID g_Characteristic_BodySensorLocation_UUID("2A38");
const BluetoothUUID *k_Characteristic_BodySensorLocation_UUID= &g_Characteristic_BodySensorLocation_UUID;
const BluetoothUUID g_Characteristic_HeartRateControlPoint_UUID("2A39");
const BluetoothUUID *k_Characteristic_HeartRateControlPoint_UUID= &g_Characteristic_HeartRateControlPoint_UUID;

//-- Skin Electrodermal Activity Service --
const BluetoothUUID g_Service_EDA_UUID("BCC80E00-5875-4884-A84B-E3EDF3598BF3");
const BluetoothUUID *k_Service_EDA_UUID= &g_Service_EDA_UUID;
const BluetoothUUID g_Characteristic_EDA_Measurement_UUID("BCC80E01-5875-4884-A84B-E3EDF3598BF3");
const BluetoothUUID *k_Characteristic_EDA_Measurement_UUID= &g_Characteristic_EDA_Measurement_UUID;
const BluetoothUUID g_Descriptor_EDA_Measurement_UUID("2902");
const BluetoothUUID *k_Descriptor_EDA_Measurement_UUID= &g_Descriptor_EDA_Measurement_UUID;
const BluetoothUUID g_Characteristic_EDA_Period_UUID("ADAF0001-C332-42A8-93BD-25E905756CB8");
const BluetoothUUID *k_Characteristic_EDA_Period_UUID= &g_Characteristic_EDA_Period_UUID;