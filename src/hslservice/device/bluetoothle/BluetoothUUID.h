#ifndef BLUETOOTH_UUID_H
#define BLUETOOTH_UUID_H

#include <string>
#include <set>

// Opaque wrapper around a Bluetooth UUID. Instances of UUID represent the
// 128-bit universally unique identifiers (UUIDs) of profiles and attributes
// used in Bluetooth based communication, such as a peripheral's services,
// characteristics, and characteristic descriptors. An instance are
// constructed using a string representing 16, 32, or 128 bit UUID formats.
class BluetoothUUID 
{
public:
	BluetoothUUID();

	// Single argument constructor. |uuid| can be a 16, 32, or 128 bit UUID
	// represented as a 4, 8, or 36 character string with the following
	// formats:
	//   xxxx
	//   0xxxxx
	//   xxxxxxxx
	//   0xxxxxxxxx
	//   xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
	//
	// 16 and 32 bit UUIDs will be internally converted to a 128 bit UUID using
	// the base UUID defined in the Bluetooth specification, hence custom UUIDs
	// should be provided in the 128-bit format. If |uuid| is in an unsupported
	// format, the result might be invalid. Use isValid to check for validity
	// after construction.
	explicit BluetoothUUID(const std::string& uuid);

	void setUUID(const std::string& in_uuid);

	// Returns true, if the UUID is in a valid canonical format.
	bool isValid() const;

	// Returns the underlying 128-bit value as a string in the following format:
	//   xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
	// where x is a lowercase hex digit.
	const std::string& getUUIDString() const { return uuid128_string; }

	// Permit sufficient comparison to allow a UUID to be used as a key in a
	// std::map.
	bool operator<(const BluetoothUUID& uuid) const;
	// Equality operators.
	bool operator==(const BluetoothUUID& uuid) const;
	bool operator!=(const BluetoothUUID& uuid) const;

private:
	// The 128-bit string representation of the UUID.
	std::string uuid128_string;
};

class BluetoothUUIDSet
{
public:
	BluetoothUUIDSet();

	inline bool isEmpty() const { return uuid_set.empty(); }
	void addUUID(const BluetoothUUID &uuid);
	bool containsUUID(const BluetoothUUID &uuid);

	// Equality operators.
	bool operator==(const BluetoothUUIDSet& other_set) const;
	bool operator!=(const BluetoothUUIDSet& other_set) const;

private:
	std::set<BluetoothUUID> uuid_set;
};

#endif // BLUETOOTH_UUID_H