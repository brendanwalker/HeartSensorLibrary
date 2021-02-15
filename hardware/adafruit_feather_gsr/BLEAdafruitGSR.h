/* Adapted from BLEAdafruitSensor.h in https://github.com/adafruit/Adafruit_nRF52_Arduino
 * Reposting original license information
 * --------------------------------------
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

#ifndef BLEADAFRUIT_GSR_H_
#define BLEADAFRUIT_GSR_H_

#include "BLEAdafruitService.h"

class BLEAdafruitGSR : public BLEService
{
  public:
    static const uint8_t UUID128_GSR_SERVICE[16];
    static const uint8_t UUID128_GSR_CHARACTERISTIC_MEASUREMENT[16];
    static const int DEFAULT_PERIOD = 100;

    typedef void (*notify_callback_t)(uint16_t conn_hdl, bool enabled);

    BLEAdafruitGSR();

    virtual err_t begin(int ms = DEFAULT_PERIOD);

    void setPeriod(int period_ms);
    void setNotifyCallback(notify_callback_t fp);

  protected:
    BLECharacteristic _period;
    BLECharacteristic _measurement;

    notify_callback_t  _notify_cb;

    SoftwareTimer _timer;

    virtual void _update_timer(int32_t ms);
    virtual void _measure_handler(void);
    virtual void _notify_handler(uint16_t conn_hdl, uint16_t value);

    static void sensor_timer_cb(TimerHandle_t xTimer);
    static void sensor_period_write_cb(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
    static void sensor_data_cccd_cb(uint16_t conn_hdl, BLECharacteristic* chr, uint16_t value);
};

#endif /* BLEADAFRUIT_GSR_H_ */
