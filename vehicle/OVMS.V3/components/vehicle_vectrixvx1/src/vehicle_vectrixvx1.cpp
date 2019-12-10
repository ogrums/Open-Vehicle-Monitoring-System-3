/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_log.h"
static const char *TAG = "v-vectrixvx1";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_vectrixvx1.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "ovms_webserver.h"

class OvmsVehicleVectrixVX1Init
  {
  public: OvmsVehicleVectrixVX1Init();
} MyOvmsVehicleVectrixVX1Init  __attribute__ ((init_priority (9000)));

OvmsVehicleVectrixVX1Init::OvmsVehicleVectrixVX1Init()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Vectrix VX1 (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleVectrixVX1>("VV","Vectrix VX1");
  }

OvmsVehicleVectrixVX1::OvmsVehicleVectrixVX1()
  {
  ESP_LOGI(TAG, "Vectrix VX1 vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  memset(m_type,0,sizeof(m_type));
  //m_charge_w = 0;

  StandardMetrics.ms_v_bat_12v_voltage->SetValue(12);
  StandardMetrics.ms_v_bat_12v_voltage_alert->SetValue(false);
  //StandardMetrics.ms_v_charge_type->SetValue(undefined);

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_250KBPS);

 /** Test change to integration vv_battmon
  BmsSetCellArrangementVoltage(40, 10);
  BmsSetCellArrangementTemperature(4, 1);
  BmsSetCellLimitsVoltage(2.6,3.60);
  BmsSetCellLimitsTemperature(-5,35);
  BmsSetCellDefaultThresholdsVoltage(0.030, 0.040);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
*/

  // init commands:
  cmd_xvv = MyCommandApp.RegisterCommand("xvv", "Vectrix VX1");


  // init subsystems:
  PowerInit();
  BatteryInit();

  #ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebInit();
  #endif

  }

OvmsVehicleVectrixVX1::~OvmsVehicleVectrixVX1()
  {
  ESP_LOGI(TAG, "Shutdown Vectrix VX1 vehicle module");
  MyCommandApp.UnregisterCommand("xvv");
  #ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebDeInit();
  #endif
  }

void OvmsVehicleVectrixVX1::Notify12vCritical()
  { // Not supported on Vectrix
  }

void OvmsVehicleVectrixVX1::Notify12vRecovered()
  { // Not supported on Vectrix
  }

/**
 * CommandStat: Twizy implementation of vehicle status output
 */
OvmsVehicleVectrixVX1::vehicle_command_t OvmsVehicleVectrixVX1::CommandStat(int verbosity, OvmsWriter* writer)
{
  metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

  bool chargeport_open = StdMetrics.ms_v_door_chargeport->AsBool();
  if (chargeport_open)
  {
    std::string charge_state = StdMetrics.ms_v_charge_state->AsString();

    if (charge_state != "done") {
      if (cfg_suffsoc > 0 && vx1_soc >= cfg_suffsoc*100 && cfg_suffrange > 0 && vx1_range_ideal >= cfg_suffrange)
        writer->puts("Sufficient SOC and range reached.");
      else if (cfg_suffsoc > 0 && vx1_soc >= cfg_suffsoc*100)
        writer->puts("Sufficient SOC level reached.");
      else if (cfg_suffrange > 0 && vx1_range_ideal >= cfg_suffrange)
        writer->puts("Sufficient range reached.");
    }

    // Translate state codes:
    if (charge_state == "charging")
      charge_state = "Charging";
    else if (charge_state == "topoff")
      charge_state = "Topping off";
    else if (charge_state == "done")
      charge_state = "Charge Done";
    else if (charge_state == "stopped")
      charge_state = "Charge Stopped";

    writer->puts(charge_state.c_str());

    // Power sums: battery input:
    float pwr_batt = StdMetrics.ms_v_bat_energy_recd->AsFloat() * 1000;
    // Grid drain estimation:
    //  charge efficiency is ~88.1% w/o 12V charge and ~86.4% with
    //  (depending on when to cut the grid connection)
    //  so we're using 87.2% as an average efficiency:
    float pwr_grid = pwr_batt / 0.872;
    writer->printf("CHG: %d (~%d) Wh\n", (int) pwr_batt, (int) pwr_grid);
  }
  else
  {
    // Charge port door is closed, not charging
    writer->puts("Not charging");
  }

  // Estimated charge time for 100%:
  if (vx1_soc < 10000)
  {
    int duration_full = StdMetrics.ms_v_charge_duration_full->AsInt();
    if (duration_full)
      writer->printf("Full: %d min.\n", duration_full);
  }

  // Estimated + Ideal Range:
  const char* range_est = StdMetrics.ms_v_bat_range_est->AsString("?", rangeUnit, 0).c_str();
  const char* range_ideal = StdMetrics.ms_v_bat_range_ideal->AsUnitString("?", rangeUnit, 0).c_str();
  writer->printf("Range: %s - %s\n", range_est, range_ideal);

  // SOC + min/max:
  writer->printf("SOC: %s (%s..%s)\n",
    (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str(),
    (char*) m_batt_use_soc_min->AsString("-", Native, 1).c_str(),
    (char*) m_batt_use_soc_max->AsUnitString("-", Native, 1).c_str());

  // ODOMETER:
  const char* odometer = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
  if (*odometer != '-')
    writer->printf("ODO: %s\n", odometer);

  // BATTERY CAPACITY:
  if (cfg_bat_cap_actual_prc > 0)
  {
    writer->printf("CAP: %.1f%% %s\n",
      cfg_bat_cap_actual_prc,
      StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str());
  }

  // BATTERY SOH:
  if (vx1_soh > 0)
  {
    writer->printf("SOH: %s\n",
      StdMetrics.ms_v_bat_soh->AsUnitString("-", Native, 0).c_str());
  }

  return Success;
}
