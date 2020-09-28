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

//#include "ovms_log.h"
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

  OvmsVehicleVectrixVX1* OvmsVehicleVectrixVX1::GetInstance(OvmsWriter* writer /*=NULL*/)
  {
    OvmsVehicleVectrixVX1* vx1 = (OvmsVehicleVectrixVX1*) MyVehicleFactory.ActiveVehicle();
    string type = StdMetrics.ms_v_type->AsString();
    if (!vx1 || type != "VV") {
      if (writer)
        writer->puts("Error: Vectrix vehicle module not selected");
      return NULL;
    }
    return vx1;
  }


OvmsVehicleVectrixVX1::OvmsVehicleVectrixVX1()
  {
  ESP_LOGI(TAG, "Vectrix VX1 vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  memset(m_type,0,sizeof(m_type));
  m_charge_w = 0;

  //StandardMetrics.ms_v_bat_12v_voltage->SetValue(12.6);
  //StandardMetrics.ms_v_bat_12v_voltage_alert->SetValue(false);
  //StandardMetrics.ms_v_charge_type->SetValue(undefined);

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_250KBPS);
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_250KBPS);

 /** Test change to integration vv_battmon
  BmsSetCellArrangementVoltage(40, 10);
  BmsSetCellArrangementTemperature(4, 1);
  BmsSetCellLimitsVoltage(2.6,3.60);
  BmsSetCellLimitsTemperature(-5,35);
  BmsSetCellDefaultThresholdsVoltage(0.030, 0.040);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
*/

  // init subsystems:
  BatteryInit();

  #ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebInit();
  #endif


  }

OvmsVehicleVectrixVX1::~OvmsVehicleVectrixVX1()
  {
  ESP_LOGI(TAG, "Shutdown Vectrix VX1 vehicle module");
  #ifdef CONFIG_OVMS_COMP_WEBSERVER
    WebDeInit();
  #endif
  }

  const char* OvmsVehicleVectrixVX1::VehicleShortName()
  {
    return "vx1";
  }
