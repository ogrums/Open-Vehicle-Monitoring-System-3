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

OvmsVehicleVectrixVX1::OvmsVehicleVectrixVX1()
  {
  ESP_LOGI(TAG, "Vectrix VX1 vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  memset(m_type,0,sizeof(m_type));
  m_charge_w = 0;

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_250KBPS);

  BmsSetCellArrangementVoltage(40, 10);
  BmsSetCellArrangementTemperature(4, 1);
  BmsSetCellLimitsVoltage(2.6,3.65);
  BmsSetCellLimitsTemperature(-5,35);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);

  MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  }

OvmsVehicleVectrixVX1::~OvmsVehicleVectrixVX1()
  {
  ESP_LOGI(TAG, "Shutdown Vectrix VX1 vehicle module");
  MyCommandApp.UnregisterCommand("xvv");
  MyWebServer.DeregisterPage("/bms/cellmon");
  }

void OvmsVehicleVectrixVX1::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x00fef105: // MC voltage and speed
      {
      StandardMetrics.ms_v_pos_speed->SetValue((float)(((((int)d[2]&0xff)<<8) + ((int)d[1]&0xff)*0.00390625)));
      // Don't update battery voltage too quickly (as it jumps around like crazy)
      if (StandardMetrics.ms_v_bat_voltage->Age() > 10)
        StandardMetrics.ms_v_bat_voltage->SetValue((int)d[6]);
        StandardMetrics.ms_v_bat_current->SetValue((float)((int)d[7]*0.488));
      //StandardMetrics.ms_v_bat_temp->SetValue((float)((((int)d[7]&0x07)<<8)+d[6])/10);
      break;
      }
    case 0x00fdf040: // BMS Summary
      {
      //int k = (((d[7]&0xf0)>>4)&0xf);
      int v1 = (((int)(d[0]&0xFF)<<4)&0xFF0) + ((int)d[5]&0xF);
      int v2 = (((int)(d[1]&0xFF)<<4)&0xFF0) + (((int)d[5]>>4)&0xF);
      int v3 = (((int)(d[2]&0xFF)<<4)&0xFF0) + ((int)d[6]&0xF);
      int v4 = (((int)(d[3]&0xFF)<<4)&0xFF0) + (((int)d[6]>>4)&0xF);
      int v5 = (((int)(d[4]&0xFF)<<4)&0xFF0) + ((int)d[7]&0xF);
      if ((v1 != 0xfff)&&(v2 != 0xfff)&&(v3 != 0xfff)&&(v4 != 0xfff)&&(v5 != 0xfff)&&
          (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0)&&(v5 != 0))
        {
          // Voltages
          int k = (((d[7]&0xf0)>>4)&0xf);
          k = ((k * 5) - 5);
          BmsSetCellVoltage(k,   0.0015 * v1);
          BmsSetCellVoltage(k+1, 0.0015 * v2);
          BmsSetCellVoltage(k+2, 0.0015 * v3);
          BmsSetCellVoltage(k+3, 0.0015 * v4);
          BmsSetCellVoltage(k+4, 0.0015 * v5);
          //ESP_LOGD(TAG, "BMS4%02x %03x %03x %03x %03x %03x", k, v1, v2, v3, v4, v5);
        }
      break;
      }
    case 0x116: // Gear selector
      {
      switch ((d[1]&0x70)>>4)
        {
        case 1: // Park
          StandardMetrics.ms_v_env_gear->SetValue(0);
          StandardMetrics.ms_v_env_on->SetValue(false);
          StandardMetrics.ms_v_env_awake->SetValue(false);
          StandardMetrics.ms_v_env_handbrake->SetValue(true);
          StandardMetrics.ms_v_env_charging12v->SetValue(false);
          break;
        case 2: // Reverse
          StandardMetrics.ms_v_env_gear->SetValue(-1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_awake->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        default:
          break;
        }
      break;
      }
    case 0xFEE24C: // Charging related
      {
      m_charge_w = uint8_t(d[7]&0x1F);
      break;
      }
    case 0x00FEF14C: // Charging Status
      {
      float charge_v = (float)(uint16_t(d[1]<<8) + d[0])/256;
      if ((charge_v != 0)&&(m_charge_w != 0))
        { // Charging
        StandardMetrics.ms_v_charge_current->SetValue((unsigned int)(m_charge_w/charge_v));
        StandardMetrics.ms_v_charge_voltage->SetValue((unsigned int)charge_v);
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
        StandardMetrics.ms_v_charge_inprogress->SetValue(true);
        StandardMetrics.ms_v_door_chargeport->SetValue(true);
        StandardMetrics.ms_v_charge_mode->SetValue("standard");
        StandardMetrics.ms_v_charge_state->SetValue("charging");
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        StandardMetrics.ms_v_charge_climit->SetValue((unsigned int)(m_charge_w/charge_v));
        }
      else
        { // Not charging
        StandardMetrics.ms_v_charge_current->SetValue(0);
        StandardMetrics.ms_v_charge_voltage->SetValue(0);
        StandardMetrics.ms_v_charge_pilot->SetValue(false);
        StandardMetrics.ms_v_charge_inprogress->SetValue(false);
        StandardMetrics.ms_v_door_chargeport->SetValue(false);
        StandardMetrics.ms_v_charge_mode->SetValue("standard");
        StandardMetrics.ms_v_charge_state->SetValue("done");
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        StandardMetrics.ms_v_charge_climit->SetValue(0);
        }
      break;
      }
    case 0x00FEFC4C: // SOC
      {
      StandardMetrics.ms_v_bat_soc->SetValue( ((float)(((int)d[0]&0xFF )* 100) / 256) );
      break;
      }
    case 0x00FF0505: // Temperatures
      {
      //StandardMetrics.ms_v_inv_temp->SetValue((int)d[1]-40);
      StandardMetrics.ms_v_mot_temp->SetValue((int)d[3]);
      break;
      }
    case 0x508: // VIN
      {
      switch(d[0])
        {
        case 0:
          memcpy(m_vin,d+1,7);
          break;
        case 1:
          memcpy(m_vin+7,d+1,7);
          break;
        case 2:
          memcpy(m_vin+14,d+1,3);
          m_vin[17] = 0;
          StandardMetrics.ms_v_vin->SetValue(m_vin);
          break;
        }
      break;
      }
    case 0x00FEE005: // Odometer (0x562 is battery, so this is motor or car?)
      {
      StandardMetrics.ms_v_pos_odometer->SetValue((float)(((uint32_t)d[4]<<24)
                                                + ((uint32_t)d[5]<<16)
                                                + ((uint32_t)d[6]<<8)
                                                + d[7])*0.05, Kilometers);
      break;
      }
/*    case 0x00fdf041: // BMS brick voltage
;      {
;      int v1 = (((int)(d[0]&0xFF)<<4)&0xFF0) + ((int)d[5]&0xF);
;      int v2 = (((int)(d[1]&0xFF)<<4)&0xFF0) + (((int)d[5]>>4)&0xF);
;      int v3 = (((int)(d[2]&0xFF)<<4)&0xFF0) + ((int)d[6]&0xF);
;      int v4 = (((int)(d[3]&0xFF)<<4)&0xFF0) + (((int)d[6]>>4)&0xF);
;      int v5 = (((int)(d[4]&0xFF)<<4)&0xFF0) + ((int)d[7]&0xF);
;      if ((v1 != 0xfff)&&(v2 != 0xfff)&&(v3 != 0xfff)&&(v4 != 0xfff)&&
;          (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0))
;        {
;          // Voltages
;          int k = (((d[7]&0xf0)>>4)&0xf);
;          BmsSetCellVoltage(k,   0.0015 * v1);
;          BmsSetCellVoltage(k+1, 0.0015 * v2);
;          BmsSetCellVoltage(k+2, 0.0015 * v3);
;          BmsSetCellVoltage(k+3, 0.0015 * v4);
;          BmsSetCellVoltage(k+4, 0.0015 * v5);
;          ESP_LOGD(TAG, "BMS4%01x %03x %03x %03x %03x %03x", k, v1, v2, v3, v4, v5);
;        }
;      break;
;      }
;    case 0x00fdf042: // BMS brick voltage and module temperatures
;      {
;      int v1 = (((int)(d[0]&0xFF)<<4)&0xFF0) + ((int)d[5]&0xF);
;      int v2 = (((int)(d[1]&0xFF)<<4)&0xFF0) + (((int)d[5]>>4)&0xF);
;      int v3 = (((int)(d[2]&0xFF)<<4)&0xFF0) + ((int)d[6]&0xF);
;      int v4 = (((int)(d[3]&0xFF)<<4)&0xFF0) + (((int)d[6]>>4)&0xF);
;      int v5 = (((int)(d[4]&0xFF)<<4)&0xFF0) + ((int)d[7]&0xF);
;      if ((v1 != 0xfff)&&(v2 != 0xfff)&&(v3 != 0xfff)&&(v4 != 0xfff)&&
;            (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0))
;        {
;          // Voltages
;            int k = (((d[7]&0xf0)>>4)&0xf);
;            BmsSetCellVoltage(k,   0.0015 * v1);
;            BmsSetCellVoltage(k+1, 0.0015 * v2);
;            BmsSetCellVoltage(k+2, 0.0015 * v3);
;            BmsSetCellVoltage(k+3, 0.0015 * v4);
;            BmsSetCellVoltage(k+4, 0.0015 * v5);
;            ESP_LOGD(TAG, "BMS42 %03x %03x %03x %03x %03x", v1, v2, v3, v4, v5);
;        }
;      break;
;      }
;    case 0x00fdf043: // BMS brick voltage and module temperatures
;      {
;      int v1 = (((int)(d[0]&0xFF)<<4)&0xFF0) + ((int)d[5]&0xF);
;      int v2 = (((int)(d[1]&0xFF)<<4)&0xFF0) + (((int)d[5]>>4)&0xF);
;      int v3 = (((int)(d[2]&0xFF)<<4)&0xFF0) + ((int)d[6]&0xF);
;      int v4 = (((int)(d[3]&0xFF)<<4)&0xFF0) + (((int)d[6]>>4)&0xF);
;      int v5 = (((int)(d[4]&0xFF)<<4)&0xFF0) + ((int)d[7]&0xF);
;      if ((v1 != 0xfff)&&(v2 != 0xfff)&&(v3 != 0xfff)&&(v4 != 0xfff)&&
;            (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0))
;        {
;          // Voltages
;            int k = (((d[7]&0xf0)>>4)&0xf);
;            BmsSetCellVoltage(k,   0.0015 * v1);
;            BmsSetCellVoltage(k+1, 0.0015 * v2);
;            BmsSetCellVoltage(k+2, 0.0015 * v3);
;            BmsSetCellVoltage(k+3, 0.0015 * v4);
;            BmsSetCellVoltage(k+4, 0.0015 * v5);
;          ESP_LOGD(TAG, "BMS43 %03x %03x %03x %03x %03x", v1, v2, v3, v4, v5);
;        }
;    break;
;    }
;    case 0x00fdf044: // BMS brick voltage and module temperatures
;      {
;      int v1 = (((int)(d[0]&0xFF)<<4)&0xFF0) + ((int)d[5]&0xF);
;      int v2 = (((int)(d[1]&0xFF)<<4)&0xFF0) + (((int)d[5]>>4)&0xF);
;      int v3 = (((int)(d[2]&0xFF)<<4)&0xFF0) + ((int)d[6]&0xF);
;      int v4 = (((int)(d[3]&0xFF)<<4)&0xFF0) + (((int)d[6]>>4)&0xF);
;      int v5 = (((int)(d[4]&0xFF)<<4)&0xFF0) + ((int)d[7]&0xF);
;      if ((v1 != 0xfff)&&(v2 != 0xfff)&&(v3 != 0xfff)&&(v4 != 0xfff)&&
;            (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0))
;        {
;          // Voltages
;            int k = (((d[7]&0xf0)>>4)&0xf);
;            BmsSetCellVoltage(k,   0.0015 * v1);
;            BmsSetCellVoltage(k+1, 0.0015 * v2);
;            BmsSetCellVoltage(k+2, 0.0015 * v3);
;            BmsSetCellVoltage(k+3, 0.0015 * v4);
;            BmsSetCellVoltage(k+4, 0.0015 * v5);
;          ESP_LOGD(TAG, "BMS44 %03x %03x %03x %03x %03x", v1, v2, v3, v4, v5);
;        }
;    break;
;    }
;    case 0x00fdf045: // BMS brick voltage and module temperatures
;      {
;      int v1 = (((int)(d[0]&0xFF)<<4)&0xFF0) + ((int)d[5]&0xF);
;      int v2 = (((int)(d[1]&0xFF)<<4)&0xFF0) + (((int)d[5]>>4)&0xF);
;      int v3 = (((int)(d[2]&0xFF)<<4)&0xFF0) + ((int)d[6]&0xF);
;      int v4 = (((int)(d[3]&0xFF)<<4)&0xFF0) + (((int)d[6]>>4)&0xF);
;      int v5 = (((int)(d[4]&0xFF)<<4)&0xFF0) + ((int)d[7]&0xF);
;      if ((v1 != 0xfff)&&(v2 != 0xfff)&&(v3 != 0xfff)&&(v4 != 0xfff)&&
;            (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0))
;        {
;          // Voltages
;            int k = (((d[7]&0xf0)>>4)&0xf);
;            BmsSetCellVoltage(k,   0.0015 * v1);
;            BmsSetCellVoltage(k+1, 0.0015 * v2);
;            BmsSetCellVoltage(k+2, 0.0015 * v3);
;            BmsSetCellVoltage(k+3, 0.0015 * v4);
;            BmsSetCellVoltage(k+4, 0.0015 * v5);
;          ESP_LOGD(TAG, "BMS45 %03x %03x %03x %03x %03x", v1, v2, v3, v4, v5);
;        }
;    break;
;    }
;    case 0x00fdf046: // BMS brick voltage and module temperatures
;      {
;      int v1 = (((int)(d[0]&0xFF)<<4)&0xFF0) + ((int)d[5]&0xF);
;      int v2 = (((int)(d[1]&0xFF)<<4)&0xFF0) + (((int)d[5]>>4)&0xF);
;      int v3 = (((int)(d[2]&0xFF)<<4)&0xFF0) + ((int)d[6]&0xF);
;      int v4 = (((int)(d[3]&0xFF)<<4)&0xFF0) + (((int)d[6]>>4)&0xF);
;      int v5 = (((int)(d[4]&0xFF)<<4)&0xFF0) + ((int)d[7]&0xF);
;      if ((v1 != 0xfff)&&(v2 != 0xfff)&&(v3 != 0xfff)&&(v4 != 0xfff)&&
;            (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0))
;        {
;          // Voltages
;            int k = (((d[7]&0xf0)>>4)&0xf);
;            BmsSetCellVoltage(k,   0.0015 * v1);
;            BmsSetCellVoltage(k+1, 0.0015 * v2);
;            BmsSetCellVoltage(k+2, 0.0015 * v3);
;            BmsSetCellVoltage(k+3, 0.0015 * v4);
;            BmsSetCellVoltage(k+4, 0.0015 * v5);
;          ESP_LOGD(TAG, "BMS46 %03x %03x %03x %03x %03x", v1, v2, v3, v4, v5);
;        }
;    break;
;    }
;    case 0x00fdf047: // BMS brick voltage and module temperatures
;      {
;      int v1 = (((int)(d[0]&0xFF)<<4)&0xFF0) + ((int)d[5]&0xF);
;      int v2 = (((int)(d[1]&0xFF)<<4)&0xFF0) + (((int)d[5]>>4)&0xF);
;      int v3 = (((int)(d[2]&0xFF)<<4)&0xFF0) + ((int)d[6]&0xF);
;      int v4 = (((int)(d[3]&0xFF)<<4)&0xFF0) + (((int)d[6]>>4)&0xF);
;      int v5 = (((int)(d[4]&0xFF)<<4)&0xFF0) + ((int)d[7]&0xF);
;      if ((v1 != 0xfff)&&(v2 != 0xfff)&&(v3 != 0xfff)&&(v4 != 0xfff)&&
;            (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0))
;        {
;          // Voltages
;            int k = (((d[7]&0xf0)>>4)&0xf);
;            BmsSetCellVoltage(k,   0.0015 * v1);
;            BmsSetCellVoltage(k+1, 0.0015 * v2);
;            BmsSetCellVoltage(k+2, 0.0015 * v3);
;            BmsSetCellVoltage(k+3, 0.0015 * v4);
;            BmsSetCellVoltage(k+4, 0.0015 * v5);
;          ESP_LOGD(TAG, "BMS47 %03x %03x %03x %03x %03x", v1, v2, v3, v4, v5);
;        }
;    break;
;    }
;    case 0x00fdf048: // BMS brick voltage and module temperatures
;      {
;      int v1 = (((int)(d[0]&0xFF)<<4)&0xFF0) + ((int)d[5]&0xF);
;      int v2 = (((int)(d[1]&0xFF)<<4)&0xFF0) + (((int)d[5]>>4)&0xF);
;      int v3 = (((int)(d[2]&0xFF)<<4)&0xFF0) + ((int)d[6]&0xF);
;      int v4 = (((int)(d[3]&0xFF)<<4)&0xFF0) + (((int)d[6]>>4)&0xF);
;      int v5 = (((int)(d[4]&0xFF)<<4)&0xFF0) + ((int)d[7]&0xF);
;      if ((v1 != 0xfff)&&(v2 != 0xfff)&&(v3 != 0xfff)&&(v4 != 0xfff)&&
;            (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0))
;        {
;          // Voltages
;            int k = (((d[7]&0xf0)>>4)&0xf);
;            BmsSetCellVoltage(k,   0.0015 * v1);
;            BmsSetCellVoltage(k+1, 0.0015 * v2);
;            BmsSetCellVoltage(k+2, 0.0015 * v3);
;            BmsSetCellVoltage(k+3, 0.0015 * v4);
;            BmsSetCellVoltage(k+4, 0.0015 * v5);
;          ESP_LOGD(TAG, "BMS48 %03x %03x %03x %03x %03x", v1, v2, v3, v4, v5);
;        }
;    break;
    }
*/
    case 0x00fef340: // BMS brick module temperatures
    {
        int k = ((d[7]&0xf0)>>4);
        int v1 = ((int)(d[1]&0xFF));
        BmsSetCellTemperature(k, v1);
        //ESP_LOGD(TAG, "BMS %01x %02x", k, v1);
        //BmsSetCellTemperature(k+1, 0.0122 * ((v2 & 0x1FFF) - (v2 & 0x2000)));
        //BmsSetCellTemperature(k+2, 0.0122 * ((v3 & 0x1FFF) - (v3 & 0x2000)));
        //BmsSetCellTemperature(k+3, 0.0122 * ((v4 & 0x1FFF) - (v4 & 0x2000)));

    break;
    }
    //case 0x2f8: // MCU GPS speed/heading
    //  StandardMetrics.ms_v_pos_gpshdop->SetValue((float)d[0] / 10);
    //  StandardMetrics.ms_v_pos_direction->SetValue((float)(((uint32_t)d[2]<<8)+d[1])/128.0);
    //  break;
    //case 0x3d8: // MCU GPS latitude / longitude
    //  StandardMetrics.ms_v_pos_latitude->SetValue((double)(((uint32_t)(d[3]&0x0f) << 24) +
    //                                                      ((uint32_t)d[2] << 16) +
    //                                                      ((uint32_t)d[1] << 8) +
    //                                                      (uint32_t)d[0]) / 1000000.0);
    //  StandardMetrics.ms_v_pos_longitude->SetValue((double)(((uint32_t)d[6] << 20) +
    //                                                       ((uint32_t)d[5] << 12) +
    //                                                       ((uint32_t)d[4] << 4) +
    //                                                       ((uint32_t)(d[3]&0xf0) >> 4)) / 1000000.0);
    //  break;
    default:
      break;
    }
  }

void OvmsVehicleVectrixVX1::Notify12vCritical()
  { // Not supported on Vectrix
  }

void OvmsVehicleVectrixVX1::Notify12vRecovered()
  { // Not supported on Vectrix
  }

class OvmsVehicleVectrixVX1Init
  {
  public: OvmsVehicleVectrixVX1Init();
} MyOvmsVehicleVectrixVX1Init  __attribute__ ((init_priority (9000)));

OvmsVehicleVectrixVX1Init::OvmsVehicleVectrixVX1Init()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Vectrix VX1 (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleVectrixVX1>("VV","Vectrix VX1");
  }
