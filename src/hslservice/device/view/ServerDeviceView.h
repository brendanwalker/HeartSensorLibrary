#ifndef SERVER_DEVICE_VIEW_H
#define SERVER_DEVICE_VIEW_H

//-- includes -----
#include "DeviceInterface.h"
#include <chrono>
#include <assert.h>

// -- declarations -----
class ServerDeviceView
{
public:
    ServerDeviceView(const int device_id);
    virtual ~ServerDeviceView();
    
    virtual bool open(const class DeviceEnumerator *enumerator);
    virtual void close();
    
    bool matchesDeviceEnumerator(const class DeviceEnumerator *enumerator) const;
    
    // getters
    inline int getDeviceID() const
    { return m_deviceID; }
    
    virtual IDeviceInterface* getDevice() const=0;

    // Returns true if device opened successfully
    bool getIsOpen() const;
    inline std::chrono::time_point<std::chrono::high_resolution_clock> getLastNewDataTimestamp() const
    { return m_lastNewDataTimestamp; }
        
protected:
    virtual bool allocateDeviceInterface(const class DeviceEnumerator *enumerator) = 0;
    virtual void freeDeviceInterface() = 0;
    
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastNewDataTimestamp;
    
private:
    int m_deviceID;
};

#endif // SERVER_DEVICE_VIEW_H
