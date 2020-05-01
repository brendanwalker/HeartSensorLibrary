//-- includes -----
#include "DeviceTypeManager.h"
#include "DeviceEnumerator.h"
#include "Logger.h"
#include "HSLService.h"
#include "ServerDeviceView.h"
#include "ServiceRequestHandler.h"
#include "Utility.h"

//-- methods -----
/// Constructor and set intervals (ms) for reconnect and polling
DeviceTypeManager::DeviceTypeManager(const int recon_int, const int poll_int)
	: reconnect_interval(recon_int)
	, poll_interval(poll_int)
	, m_deviceViews(nullptr)
	, m_bIsDeviceListDirty(false)
{
}

DeviceTypeManager::~DeviceTypeManager()
{
	assert(m_deviceViews == nullptr);
}

/// Override if the device type needs to initialize any services
bool DeviceTypeManager::startup()
{
	assert(m_deviceViews == nullptr);

	const int maxDeviceCount = getMaxDevices();
	m_deviceViews = new ServerDeviceViewPtr[maxDeviceCount];

	// Allocate all of the device views
	for (int device_id = 0; device_id < maxDeviceCount; ++device_id)
	{
		ServerDeviceViewPtr deviceView = ServerDeviceViewPtr(allocate_device_view(device_id));

		m_deviceViews[device_id] = deviceView;
	}

	// Rebuild the device list the first chance we get
	m_bIsDeviceListDirty = true;

	return true;
}

/// Override if the device type needs to teardown any services
void DeviceTypeManager::shutdown()
{
	if (m_deviceViews != nullptr)
	{
		// Close any devices that were opened
		for (int device_id = 0; device_id < getMaxDevices(); ++device_id)
		{
			ServerDeviceViewPtr device = m_deviceViews[device_id];

			if (device->getIsOpen())
			{
				device->close();
			}

			m_deviceViews[device_id] = ServerDeviceViewPtr();
		}

		// Free the device view pointer list
		delete[] m_deviceViews;
		m_deviceViews = nullptr;
	}
}

/// Calls poll_devices and update_connected_devices if poll_interval and reconnect_interval has elapsed, respectively.
void DeviceTypeManager::pollConnectedDevices()
{
	std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();

	// See if it's time to try update the list of connected devices
	if (reconnect_interval > 0)
	{
		std::chrono::duration<double, std::milli> reconnect_diff = now - m_last_reconnect_time;

		if (reconnect_diff.count() >= reconnect_interval)
		{
			m_bIsDeviceListDirty = true;
		}
	}

	if (m_bIsDeviceListDirty)
	{
		if (update_connected_devices())
		{
			m_bIsDeviceListDirty = false;
			m_last_reconnect_time = now;
		}
	}
}

bool DeviceTypeManager::update_connected_devices()
{
	bool success = false;

	// Don't do any connection opening/closing until all pending bluetooth operations are finished
	if (can_update_connected_devices())
	{
		const int maxDeviceCount = getMaxDevices();
		bool exists_in_enumerator[64];
		bool bSendSensorUpdatedNotification = false;

		// Initialize temp table used to keep track of open devices
		// still found in the enumerator
		assert(maxDeviceCount <= 64);
		memset(exists_in_enumerator, 0, sizeof(exists_in_enumerator));

		// Step 1
		// Mark any open devices that still show up in the enumerator.
		// Open devices shown in the enumerator that we haven't open yet.
		{
			DeviceEnumerator *enumerator = allocate_device_enumerator();

			while (enumerator->isValid())
			{
				// Find device index for the device with the matching device path
				int device_id = find_open_device_device_id(enumerator);

				// Existing device case (Most common)
				if (device_id != -1)
				{
					// Mark the device as having showed up in the enumerator
					exists_in_enumerator[device_id]= true;
				}
				// New sensor connected case
				else
				{
					int device_id_ = find_first_closed_device_device_id();

					if (device_id_ != -1)
					{
						// Fetch the sensor from it's existing sensor slot
						ServerDeviceViewPtr availableDeviceView = getDeviceViewPtr(device_id_);

						// Attempt to open the device
						if (availableDeviceView->open(enumerator))
						{
							const std::string friendlyName= availableDeviceView->getDevice()->getFriendlyName();

							HSL_LOG_INFO("DeviceTypeManager::update_connected_devices") <<
									"Device device_id " << device_id_ << " (" << friendlyName << ") opened";

							// Mark the device as having showed up in the enumerator
							exists_in_enumerator[device_id_] = true;

							// Send notification to clients that a new device was added
							bSendSensorUpdatedNotification = true;
						}
						else
						{
							HSL_LOG_ERROR("DeviceTypeManager::update_connected_devices") << 
									"Device device_id " << device_id_ << " (" << enumerator->getPath() << ") failed to open!";
						}
					}
					else
					{
						HSL_LOG_ERROR("DeviceTypeManager::update_connected_devices") << 
								"Can't connect any more new devices. Too many open device.";
						break;
					}
				}

				enumerator->next();
			}

			free_device_enumerator(enumerator);
		}

		// Step 2
		// Close any device that is open and wasn't found in the enumerator
		for (int device_id = 0; device_id < maxDeviceCount; ++device_id)
		{
			ServerDeviceViewPtr existingDevice = getDeviceViewPtr(device_id);

			// This probably shouldn't happen very often (at all?) as polling should catch
			// disconnected devices first.
			if (existingDevice->getIsOpen() && !exists_in_enumerator[device_id])
			{
				const std::string friendlyName= existingDevice->getDevice()->getFriendlyName();

				HSL_LOG_WARNING("DeviceTypeManager::update_connected_devices") << "Closing device "
						<< device_id << " (" << friendlyName << ") since it's no longer in the device list.";
				existingDevice->close();
				bSendSensorUpdatedNotification = true;
			}
		}

		// List of open devices changed, tell the clients
		if (bSendSensorUpdatedNotification)
		{
			send_device_list_changed_notification();
		}

		success = true;
	}

	return success;
}

void DeviceTypeManager::send_device_list_changed_notification()
{
	HSLEventMessage message;
	message.event_type= static_cast<HSLEventType>(getListUpdatedResponseType());

	HSLService::getInstance()->getRequestHandler()->publish_notification(message);
}

bool DeviceTypeManager::can_poll_connected_devices()
{
	return true;
}

bool DeviceTypeManager::can_update_connected_devices()
{
	return true;
}

void DeviceTypeManager::poll_devices()
{
	if (can_poll_connected_devices())
	{
		send_device_list_changed_notification();
	}
}

int DeviceTypeManager::find_open_device_device_id(
	const DeviceEnumerator *enumerator)
{
	int result_device_id = -1;

	for (int device_id = 0; device_id < getMaxDevices(); ++device_id)
	{
		ServerDeviceViewPtr device = getDeviceViewPtr(device_id);

		if (device && device->matchesDeviceEnumerator(enumerator))
		{
			result_device_id = device_id;
			break;
		}
	}

	return result_device_id;
}

int DeviceTypeManager::find_first_closed_device_device_id()
{
	int result_device_id = -1;
	for (int device_id = 0; device_id < getMaxDevices(); ++device_id)
	{
		ServerDeviceViewPtr device = getDeviceViewPtr(device_id);

		if (device && !device->getIsOpen())
		{
			result_device_id = device_id;
			break;
		}
	}
	return result_device_id;
}

ServerDeviceViewPtr DeviceTypeManager::getDeviceViewPtr(int device_id)
{
	assert(m_deviceViews != nullptr);

	return m_deviceViews[device_id];
}

void DeviceTypeManager::handle_device_connected(
	enum DeviceClass device_class, 
	const std::string &device_path)
{
	m_bIsDeviceListDirty = true;
}

void DeviceTypeManager::handle_device_disconnected(
	enum DeviceClass device_class, 
	const std::string &device_path)
{
	m_bIsDeviceListDirty = true;
}