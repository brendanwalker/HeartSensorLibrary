#ifndef SENSOR_BLE_DEVICE_ENUMERATOR_H
#define SENSOR_BLE_DEVICE_ENUMERATOR_H

//-- includes -----
#include "DeviceEnumerator.h"
#include "BluetoothLEApiInterface.h"
#include <string>

//-- definitions -----
class SensorBluetoothLEDeviceEnumerator : public DeviceEnumerator
{
public:
	SensorBluetoothLEDeviceEnumerator();
	SensorBluetoothLEDeviceEnumerator(const std::string &ble_path);
	~SensorBluetoothLEDeviceEnumerator();

	bool isValid() const override;
	bool next() override;
    const std::string &getFriendlyName() const override;
	const std::string &getPath() const override;
	const std::string &getUniqueIdentifier() const;
	inline eBluetoothLEApiType getBluetoothLEApiType() const { return m_currentDriverType; }
	inline class BluetoothLEDeviceEnumerator* getBluetoothLEDeviceEnumerator() const { return m_ble_enumerator; }

protected: 
	bool testBLEEnumerator();

private:
    std::string m_currentFriendlyName;
    std::string m_currentBLEPath;
    std::string m_currentBLEIdentifier;
	eBluetoothLEApiType m_currentDriverType;
	class BluetoothLEDeviceEnumerator* m_ble_enumerator;
	int m_sensorIndex;
};

#endif // SENSOR_BLE_DEVICE_ENUMERATOR_H
