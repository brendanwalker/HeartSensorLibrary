/* Adapted from BLEAdafruitSensor.cpp in https://github.com/adafruit/Adafruit_nRF52_Arduino
 * Reposting original license information
 * --------------------------------------
 * 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org) for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "BLEAdafruitGSR.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

/* UUID128 template for service and characteristic:
 *    B9C8xxxx-5875-4884-A84B-E3EDF3598BF3
 *
 * Shared Characteristics
 *  - Measurement Period  0001 | int32_t | Read + Write |
 *    ms between measurements, -1: stop reading, 0: update when changes
 *
 * GSR service  0E00
 *  - Data                0E01 | uint16_t | Read + Notify | unit-less (0-1023)
 *  - Measurement Period  0001
 */

const uint8_t BLEAdafruitGSR::UUID128_GSR_SERVICE[16] =
{
  0xF3, 0x8B, 0x59, 0xF3, 0xED, 0xE3, 0x4B, 0xA8,
  0x84, 0x48, 0x75, 0x58, 0x00, 0x0E, 0xC8, 0xBC
};

const uint8_t BLEAdafruitGSR::UUID128_GSR_CHARACTERISTIC_MEASUREMENT[16] =
{
  0xF3, 0x8B, 0x59, 0xF3, 0xED, 0xE3, 0x4B, 0xA8,
  0x84, 0x48, 0x75, 0x58, 0x01, 0x0E, 0xC8, 0xBC
};

BLEAdafruitGSR::BLEAdafruitGSR()
  : BLEService(UUID128_GSR_SERVICE)
  , _measurement(UUID128_GSR_CHARACTERISTIC_MEASUREMENT)
  , _period(UUID128_CHR_ADAFRUIT_MEASUREMENT_PERIOD)
{
  // Setup Measurement Characteristic
  _measurement.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  _measurement.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  _measurement.setFixedLen(2);
  
  _notify_cb = NULL;
}

err_t BLEAdafruitGSR::begin(int ms)
{
  // Invoke base class begin()
  VERIFY_STATUS( BLEService::begin() );

  _measurement.setCccdWriteCallback(sensor_data_cccd_cb, true);
  VERIFY_STATUS( _measurement.begin() );
  _measurement.write32(0); // zero 2 bytes could help to init some services

  _period.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE);
  _period.setPermission(SECMODE_OPEN, SECMODE_OPEN);
  _period.setFixedLen(4);
  VERIFY_STATUS( _period.begin() );
  _period.write32(ms);

  _period.setWriteCallback(sensor_period_write_cb, true);

  // setup timer
  _timer.begin(ms, sensor_timer_cb, this, true);

  return ERROR_NONE;
}

void BLEAdafruitGSR::setPeriod(int period_ms)
{
  _period.write32(period_ms);
  _update_timer(period_ms);
}

void BLEAdafruitGSR::setNotifyCallback(notify_callback_t fp)
{
  _notify_cb = fp;
}

//--------------------------------------------------------------------+
// Internal API
//--------------------------------------------------------------------+
void BLEAdafruitGSR::_notify_handler(uint16_t conn_hdl, uint16_t value)
{
  // notify enabled
  if (value & BLE_GATT_HVX_NOTIFICATION)
  {
    _timer.start();
  }else
  {
    _timer.stop();
  }

  // invoke callback if any
  if (_notify_cb) _notify_cb(conn_hdl, value);

  // send initial notification if period = 0
  //  if ( 0 == svc._period.read32() )
  //  {
  //    svc._measurement.notify();
  //  }
}

void BLEAdafruitGSR::_update_timer(int32_t ms)
{
  if ( ms < 0 )
  {
    _timer.stop();
  }
  else if ( ms > 0)
  {
    _timer.setPeriod(ms);
  }
  else
  {
    // Period = 0: keeping the current interval, but report on changes only
  }
}

void BLEAdafruitGSR::_measure_handler(void)
{
  // read the Galvanic Skin Response voltage on analog pin 0
  uint16_t sensorValue = analogRead(A0);  

  // TODO multiple connections
  _measurement.notify(&sensorValue, 2);
}

//--------------------------------------------------------------------+
// Static Callbacks
//--------------------------------------------------------------------+

void BLEAdafruitGSR::sensor_timer_cb(TimerHandle_t xTimer)
{
  BLEAdafruitGSR* svc = (BLEAdafruitGSR*) pvTimerGetTimerID(xTimer);
  svc->_measure_handler();
}

// Client update period, adjust timer accordingly
void BLEAdafruitGSR::sensor_period_write_cb(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len)
{
  (void) conn_hdl;
  BLEAdafruitGSR* svc = (BLEAdafruitGSR*) &chr->parentService();

  int32_t ms = 0;
  memcpy(&ms, data, len);

  svc->_update_timer(ms);
}

void BLEAdafruitGSR::sensor_data_cccd_cb(uint16_t conn_hdl, BLECharacteristic* chr, uint16_t value)
{
  BLEAdafruitGSR* svc = (BLEAdafruitGSR*) &chr->parentService();

  svc->_notify_handler(conn_hdl, value);
}

