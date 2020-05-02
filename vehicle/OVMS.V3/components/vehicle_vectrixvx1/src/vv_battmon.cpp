/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy battery monitor
 *
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
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

//#include "ovms_log.h"
static const char *TAG = "v-vectrixvx1";

//#include <math.h>

#include "vv_battmon.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "string_writer.h"

#include "vehicle_vectrixvx1.h"

/**
 * BatteryInit:
 */
void OvmsVehicleVectrixVX1::BatteryInit()
{
  ESP_LOGI(TAG, "battmon subsystem init");

  // BMS configuration:
  //    Note: layout currently fixed to 10 voltages + 1 temperature per module,
  //          this may need refinement for custom batteries
  BmsSetCellArrangementVoltage(40, 10);
  BmsSetCellArrangementTemperature(4, 1);
  BmsSetCellLimitsVoltage(2.6, 3.60);
  BmsSetCellLimitsTemperature(-5, 35);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);

}

/**
 * BatteryReset: reset deviations, alerts & watches
 */
void OvmsVehicleVectrixVX1::BatteryReset()
{
  ESP_LOGD(TAG, "battmon reset");

  BmsResetCellStats();

}
