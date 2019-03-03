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
  //m_charge_w = 0;

  StandardMetrics.ms_v_bat_12v_voltage->SetValue(12);
  StandardMetrics.ms_v_bat_12v_voltage_alert->SetValue(false);
  //StandardMetrics.ms_v_charge_type->SetValue(undefined);

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_250KBPS);

  BmsSetCellArrangementVoltage(40, 10);
  BmsSetCellArrangementTemperature(4, 1);
  BmsSetCellLimitsVoltage(2.6,3.60);
  BmsSetCellLimitsTemperature(-5,35);
  BmsSetCellDefaultThresholdsVoltage(0.030, 0.040);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);
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

void OvmsVehicleVectrixVX1::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x00fee04C:
		{
		// Estimated range:
		StandardMetrics.ms_v_bat_range_est->SetValue((float) ((d[7] << 24) + (d[6] << 16) + (d[5] << 8) + d[4]) * 0.5, Kilometers);
		break;
		}


	  case 0x18FEE017:
		{
		// Odometer:
		StandardMetrics.ms_v_pos_odometer->SetValue((float) ((d[7] << 24) + (d[6] << 16) + (d[5] << 8) + d[4]) * 0.05, Kilometers);
		// trip A
		StandardMetrics.ms_v_pos_trip->SetValue((float) ((d[1] << 8) + d[0]) * 0.05, Kilometers);
		break;
		}

	  case 0x00fef106:
		{
		// Charging Mode
		// Mesured Motor Controller voltage used
		//StdMetrics.ms_v_bat_voltage->SetValue((float) (d[6] > 0), Volts); // Volts
		break;
		}


	  case 0x00ff0505:
		{
		// Regen:
		//StdMetrics.ms_v_pos_speed->SetValue((double) ((d[2] << 8) + d[1]) * 0.00390625, Kph); // kph
		// Motor Controller Temperature
		// ?????_mc_temp->SetValue((float) (d[3] > 0), Celcius); // Deg C
		// Motor controller Capacitor Temperature
		// ?????_mc_cap1temp->SetValue((float) (d[4] > 0), Celcius); // Deg C
		// ?????_mc_cap2temp->SetValue((float) (d[5] > 0), Celcius); // Deg C
		// ?????_mc_cap3temp->SetValue((float) (d[6] > 0), Celcius); // Deg C

		// Status GO, Ready, VPE Indicator Byte 0
		//m_v_env_readyindic->SetValue((d[0] & 0x01) > 0);//Byte 0 - Bit 0 - Ready
		//m_v_env_goindic->SetValue((d[0] & 0x04) > 0);		//Byte 0 - Bit 2 - Go
		//m_v_env_vpeindic->SetValue((d[0] & 0x10) > 0);	//Byte 0 - Bit 4 - VPE

    //StandardMetrics.ms_v_inv_temp->SetValue((int)d[1]-40);
    StandardMetrics.ms_v_mot_temp->SetValue((int)d[3]);
		break;
		}


    case 0x00fef105: // MC voltage and speed
    {
    StandardMetrics.ms_v_pos_speed->SetValue((float)(((((int)d[2]&0xff)<<8) + ((int)d[1]&0xff))*0.00390625));
    // Don't update battery voltage too quickly (as it jumps around like crazy)
    if (StandardMetrics.ms_v_bat_voltage->Age() > 10)
      {
      StandardMetrics.ms_v_bat_voltage->SetValue((int)d[6]);
      StandardMetrics.ms_v_bat_current->SetValue((float)((int)d[7]*0.488));
    //StandardMetrics.ms_v_bat_temp->SetValue((float)((((int)d[7]&0x07)<<8)+d[6])/10);
      }
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


    case 0x00f00300: // Gear selector
      {
      switch ((d[1]&0x70)>>4)
        {
        case 1: // Park
          StandardMetrics.ms_v_env_gear->SetValue(0);
          StandardMetrics.ms_v_env_on->SetValue(false);
          //StandardMetrics.ms_v_env_awake->SetValue(false);
          StandardMetrics.ms_v_env_handbrake->SetValue(true);
          break;
        case 2: // Reverse
          StandardMetrics.ms_v_env_gear->SetValue(-1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          //StandardMetrics.ms_v_env_awake->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          break;
        default:
          break;
        }
      break;
      }


    case 0x00fee24c: // Charging related
      {
      // to-do
      break;
      }

    case 0x00fef14c: // Charging Status
      {
      float charge_v = (float)(uint16_t(d[1]<<8) + d[0])/256;
      if ((charge_v != 0)&&(m_charge_w != 0))
        { // Charging
        //StandardMetrics.ms_v_charge_current->SetValue((unsigned int)(m_charge_w/charge_v));
        //StandardMetrics.ms_v_charge_voltage->SetValue((unsigned int)charge_v);
        StandardMetrics.ms_v_charge_current->SetValue((float)(((((int)d[2]&0xff)<<8) + ((int)d[1]&0xff))*0.000390625));
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
        StandardMetrics.ms_v_charge_inprogress->SetValue(true);
        StandardMetrics.ms_v_charge_mode->SetValue("standard");
        StandardMetrics.ms_v_charge_state->SetValue("charging");
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        StandardMetrics.ms_v_charge_climit->SetValue((unsigned int)(m_charge_w/charge_v));
        }
      else
        { // Not charging
        //StandardMetrics.ms_v_charge_current->SetValue(0);
        //StandardMetrics.ms_v_charge_voltage->SetValue(0);
        StandardMetrics.ms_v_charge_pilot->SetValue(false);
        StandardMetrics.ms_v_charge_inprogress->SetValue(false);
        StandardMetrics.ms_v_charge_mode->SetValue("standard");
        StandardMetrics.ms_v_charge_state->SetValue("done");
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        StandardMetrics.ms_v_charge_climit->SetValue(0);
        }
      break;
      }


    case 0x00fef74c: //Charger status
		  {
		  //m_v_preheat_timer1_enabled->SetValue( d[0] & 0x1 );
		  //m_v_preheat_timer2_enabled->SetValue( d[3] & 0x1 );
		  StandardMetrics.ms_v_charge_current->SetValue((float) (((d[4] & 0xFF) * 0.1) , Amps)); // Charger Output Current
		  StandardMetrics.ms_v_charge_voltage->SetValue((float) (((d[7] & 0xFF) > 0), Volts)); // Charger Output Volts
		break;
		  }


    case 0x00fefc4c: // SOC
      {
      StandardMetrics.ms_v_bat_soc->SetValue( ((float)(((int)d[0] & 0xFF )* 100) / 256) );
      break;
      }


    case 0x00fee017: // Odometer from Instrument Cluster
      {
      StandardMetrics.ms_v_pos_odometer->SetValue((float)(((uint32_t)d[4]<<24)
                                                + ((uint32_t)d[5]<<16)
                                                + ((uint32_t)d[6]<<8)
                                                + d[7])*0.05, Kilometers);
      break;
      }


    case 0x18fee617:
      {
        // Clock:
        //if (d[1] > 0 || d[2] > 0 || d[3] > 0)
        //vv_clock=(((d[0]*60) + d[1])*60) + d[2];

        // check If Go Ready "ignition"
        uint8_t indGoReady = ((d[0] & 0x01) || (d[0] & 0x04));
        if ( indGoReady == 5) {
          StandardMetrics.ms_v_env_on->SetValue(true);
        } else {
          StandardMetrics.ms_v_env_on->SetValue(false);
        }
      break;
      }



    case 0x00FEF340: // BMS brick module temperatures
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
