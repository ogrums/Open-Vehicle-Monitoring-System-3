/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy: low level CAN communication
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

#include "ovms_log.h"
static const char *TAG = "v-vectrixvx1";

#include <stdio.h>
#include <string>
#include <iomanip>
#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"

#include "vehicle_vectrixvx1.h"

using namespace std;

// CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b)     can_databuffer[b]
#define CAN_UINT(b)     (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b+1))
#define CAN_UINT24(b)   (((UINT32)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b+1) << 8) \
                          | CAN_BYTE(b+2))
#define CAN_UINT32(b)   (((UINT32)CAN_BYTE(b) << 24) | ((UINT32)CAN_BYTE(b+1) << 16) \
                          | ((UINT)CAN_BYTE(b+2) << 8) | CAN_BYTE(b+3))
#define CAN_NIBL(b)     (can_databuffer[b] & 0x0f)
#define CAN_NIBH(b)     (can_databuffer[b] >> 4)
#define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))


 /**
  * Asynchronous CAN RX handler
  */

 void OvmsVehicleVectrixVX1::IncomingFrameCan1(CAN_frame_t* p_frame)
 {
   unsigned int u;
   //int s;

   uint8_t *can_databuffer = p_frame->data.u8;


   switch (p_frame->MsgID)
  {
    case 0x00fee04C:
    {
    // Estimated range:
    StandardMetrics.ms_v_bat_range_est->SetValue((float) ((CAN_BYTE(7) << 24) + (CAN_BYTE(6) << 16) + (CAN_BYTE(5) << 8) + CAN_BYTE(4)) * 0.5, Kilometers);
    //ESP_LOGV(TAG, "IncomingFrameCan1: Range estimated %d", StandardMetrics.ms_v_bat_range_est);
    break;
    }

    case 0x18FEE017:
    {
    // Odometer:
    //StandardMetrics.ms_v_pos_odometer->SetValue((float) ((CAN_BYTE(7) << 24) + (CAN_BYTE(6) << 16) + (CAN_BYTE(5) << 8) + CAN_BYTE(4)) * 0.05, Kilometers);
    // *** ODOMETER ***
    vx1_odometer = (((uint32_t)CAN_BYTE(4)<<24) + ((uint32_t)CAN_BYTE(5)<<16) + ((uint32_t)CAN_BYTE(6)<<8) + CAN_BYTE(7));
    if (!vx1_odometer_tripstart)
      vx1_odometer_tripstart = vx1_odometer;
    // trip A
    StandardMetrics.ms_v_pos_trip->SetValue((float) ((CAN_BYTE(1) << 8) + CAN_BYTE(0)) * 0.05, Kilometers);
    break;
    }

    case 0x00fef106:
    {
    // Charging Mode
    // Mesured Motor Controller voltage used
    //StdMetrics.ms_v_bat_voltage->SetValue((float) (CAN_BYTE(6) > 0), Volts); // Volts
    break;
    }

    case 0x00ff0505: // Motor Controller indicator, temparature
    {
    // Regen:
    //StdMetrics.ms_v_pos_speed->SetValue((double) ((CAN_BYTE(2) << 8) + CAN_BYTE(1)) * 0.00390625, Kph); // kph
    // Motor Controller Temperature
    // ?????_mc_temp->SetValue((float) (CAN_BYTE(3) > 0), Celcius); // Deg C
    // Motor controller Capacitor Temperature
    // ?????_mc_cap1temp->SetValue((float) (CAN_BYTE(4) > 0), Celcius); // Deg C
    // ?????_mc_cap2temp->SetValue((float) (CAN_BYTE(5) > 0), Celcius); // Deg C
    // ?????_mc_cap3temp->SetValue((float) (CAN_BYTE(6) > 0), Celcius); // Deg C

    // Status GO, Ready, VPE Indicator Byte 0
    //m_v_env_readyindic->SetValue((CAN_BYTE(0) & 0x01) > 0);//Byte 0 - Bit 0 - Ready
    //m_v_env_goindic->SetValue((CAN_BYTE(0) & 0x04) > 0);		//Byte 0 - Bit 2 - Go
    //m_v_env_vpeindic->SetValue((CAN_BYTE(0) & 0x10) > 0);	//Byte 0 - Bit 4 - VPE

    StandardMetrics.ms_v_inv_temp->SetValue((int)CAN_BYTE(3));
    StandardMetrics.ms_v_mot_temp->SetValue((int)CAN_BYTE(4));
    break;
    }

    case 0x00fef105: // MC voltage and speed + Power
    {
      unsigned int t;
      // RANGE:
      // we need to check for charging, as the Twizy
      // does not update range during charging
      if (((vx1_status & 0x60) == 0)
        && (can_databuffer[5] != 0xff) && (can_databuffer[5] > 0))
      {
        vx1_range_est = can_databuffer[5];
        // car values derived in ticker1()
      }

      // SPEED:
      u = ((((unsigned int) CAN_BYTE(2)&0xff)<<8) + ((unsigned int)CAN_BYTE(1)&0xff));
      if (u != 0xffff)
      {
        int delta = (int) u - (int) vx1_speed;

        // set min/max:
        if (delta < vx1_accel_min)
          vx1_accel_min = delta;
        if (delta > vx1_accel_max)
          vx1_accel_max = delta;

        // running average over 4 samples:
        vx1_accel_avg = vx1_accel_avg * 3 + delta;
        // C18: no arithmetic >> sign propagation on negative ints
        vx1_accel_avg = (vx1_accel_avg < 0)
          ? -((-vx1_accel_avg + 2) >> 2)
          : ((vx1_accel_avg + 2) >> 2);

        // switch speed state:
        if (vx1_accel_avg >= CAN_ACCEL_THRESHOLD)
          vx1_speed_state = CAN_SPEED_ACCEL;
        else if (vx1_accel_avg <= -CAN_ACCEL_THRESHOLD)
          vx1_speed_state = CAN_SPEED_DECEL;
        else
          vx1_speed_state = CAN_SPEED_CONST;

        // speed/delta sum statistics while driving:
        if (u >= CAN_SPEED_THRESHOLD)
        {
          // overall speed avg:
          vx1_speedpwr[0].spdcnt++;
          vx1_speedpwr[0].spdsum += u;

          // accel/decel speed avg:
          if (vx1_speed_state != 0)
          {
            vx1_speedpwr[vx1_speed_state].spdcnt++;
            vx1_speedpwr[vx1_speed_state].spdsum += ABS(vx1_accel_avg);
          }
        }

        vx1_speed = u;
        *StdMetrics.ms_v_pos_speed = (float) vx1_speed *0.00390625;
        CalculateAcceleration();
      }
    //StandardMetrics.ms_v_pos_speed->SetValue((float)(((((int)CAN_BYTE(2)&0xff)<<8) + ((int)CAN_BYTE(1)&0xff))*0.00390625));
    // Don't update battery voltage too quickly (as it jumps around like crazy)
    if (StandardMetrics.ms_v_bat_voltage->Age() > 10)
    {
      StandardMetrics.ms_v_bat_voltage->SetValue((int)CAN_BYTE(6));

    //StandardMetrics.ms_v_bat_temp->SetValue((float)((((int)CAN_BYTE(7)&0x07)<<8)+CAN_BYTE(6))/10);
      }
    // CURRENT & POWER:
    //StandardMetrics.ms_v_bat_current->SetValue((float)((int)CAN_BYTE(7)*0.488));
    t = ((int)CAN_BYTE(7));
    if (t > 0 && t < 0x0f00)
    {
        vx1_current = (signed int) t;
        // ...in 1/4 A

        // set min/max:
        if (vx1_current < vx1_current_min)
          vx1_current_min = vx1_current;
        if (vx1_current > vx1_current_max)
          vx1_current_max = vx1_current;

        // calculate power:
        vx1_power = (vx1_current < 0)
          ? -((((long) -vx1_current) * vx1_batt[0].volt_act + 128) >> 8)
          : ((((long) vx1_current) * vx1_batt[0].volt_act + 128) >> 8);
        // ...in 256/40 W = 6.4 W

        // set min/max:
        if (vx1_power < vx1_power_min)
          vx1_power_min = vx1_power;
        if (vx1_power > vx1_power_max)
          vx1_power_max = vx1_power;

        // calculate distance from ref:
        if (vx1_dist >= vx1_speed_distref)
          t = vx1_dist - vx1_speed_distref;
        else
          t = vx1_dist + (0x10000L - vx1_speed_distref);
        vx1_speed_distref = vx1_dist;

        // add to speed state:
        vx1_speedpwr[vx1_speed_state].dist += t;
        if (vx1_current > 0)
        {
          vx1_speedpwr[vx1_speed_state].use += vx1_power;
          vx1_level_use += vx1_power;
          vx1_charge_use += vx1_current;
        }
        else
        {
          vx1_speedpwr[vx1_speed_state].rec += -vx1_power;
          vx1_level_rec += -vx1_power;
          vx1_charge_rec += -vx1_current;
        }

        // publish metrics:
        *StdMetrics.ms_v_bat_current = (float) vx1_current * 0.488;
        *StdMetrics.ms_v_bat_power = (float) vx1_power * 64 / 10000;

        // do we need to take base power consumption into account?
        // i.e. for lights etc. -- varies...
      }
    break;
    }

    case 0x00fdf040: // BMS Summary
    {
    //int k = (((CAN_BYTE(7)&0xf0)>>4)&0xf);
    int v1 = (((int)(CAN_BYTE(0)&0xFF)<<4)&0xFF0) + ((int)CAN_BYTE(5)&0xF);
    int v2 = (((int)(CAN_BYTE(1)&0xFF)<<4)&0xFF0) + (((int)CAN_BYTE(5)>>4)&0xF);
    int v3 = (((int)(CAN_BYTE(2)&0xFF)<<4)&0xFF0) + ((int)CAN_BYTE(6)&0xF);
    int v4 = (((int)(CAN_BYTE(3)&0xFF)<<4)&0xFF0) + (((int)CAN_BYTE(6)>>4)&0xF);
    int v5 = (((int)(CAN_BYTE(4)&0xFF)<<4)&0xFF0) + ((int)CAN_BYTE(7)&0xF);
    if ((v1 != 0xfff)&&(v2 != 0xfff)&&(v3 != 0xfff)&&(v4 != 0xfff)&&(v5 != 0xfff)&&
        (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0)&&(v5 != 0))
      {
      // Voltages
      int k = (((CAN_BYTE(7)&0xf0)>>4)&0xf);
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
      // twizy_status low nibble:
      vx1_status = (vx1_status & 0xF0) | (CAN_BYTE(1) & 0x09);

      // accelerator throttle:
      u = ((((unsigned int) CAN_BYTE(1)&0xff)<<8) + ((unsigned int)CAN_BYTE(0)&0xff));

      if (u > 0x0276) {
        *StdMetrics.ms_v_env_gear = (int) 1;
        vx1_accel_pedal = (float)((u - 625) *100 / 210);
        *StdMetrics.ms_v_env_throttle = (float) (vx1_accel_pedal); // 840 = maxthrottle value - 530 min throttle value
        *StdMetrics.ms_v_env_footbrake = 0;
      }
      else if (u < 0x026C) {
        *StdMetrics.ms_v_env_gear = (int) -1;
        vx1_accel_pedal = (float)((625 - u) *-100 / 90);
        *StdMetrics.ms_v_env_footbrake = (float) (vx1_accel_pedal);
        *StdMetrics.ms_v_env_throttle = 0;
      }
      else {
        // Neutral Throttle position between 620 and 630
        *StdMetrics.ms_v_env_gear = (int) 0;
        *StdMetrics.ms_v_env_throttle = 0;
        *StdMetrics.ms_v_env_footbrake = 0;
        vx1_accel_pedal = 0;
      }

      // running average over 2 samples:
      //u = (vx1_accel_pedal + u + 1) >> 1;

      // kickdown detection:
      //s = KickdownThreshold(vx1_accel_pedal);
      //if ( ((s > 0) && (u > ((unsigned int)vx1_accel_pedal + s)))
      //  || ((vx1_kickdown_level > 0) && (u > vx1_kickdown_level)) )
      //{
        vx1_kickdown_level = u;
      //  m_sevcon->Kickdown(true);
      //}

      vx1_accel_pedal = u;

    break;
    }

    case 0x00fee24c: // Charging related
    {
    // to-do
    break;
    }

    case 0x00fef14c: // Charging Status
    {
    float charge_v = (float)(uint16_t(CAN_BYTE(1)<<8) + CAN_BYTE(0))/256;
    if ((charge_v != 0)&&(m_charge_w != 0))
      { // Charging
      //StandardMetrics.ms_v_charge_current->SetValue((unsigned int)(m_charge_w/charge_v));
      //StandardMetrics.ms_v_charge_voltage->SetValue((unsigned int)charge_v);
      StandardMetrics.ms_v_charge_current->SetValue((float)(((((int)CAN_BYTE(2)&0xff)<<8) + ((int)CAN_BYTE(1)&0xff))*0.000390625));
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
    //m_v_preheat_timer1_enabled->SetValue( CAN_BYTE(0) & 0x1 );
    //m_v_preheat_timer2_enabled->SetValue( CAN_BYTE(3) & 0x1 );
    if (((int)CAN_BYTE(0)&0x0A) != 0) {
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
    } else {
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    }
    StandardMetrics.ms_v_charge_current->SetValue((float) (((CAN_BYTE(4) & 0xFF) * 0.1) , Amps)); // Charger Output Current
    StandardMetrics.ms_v_charge_voltage->SetValue((float) (((CAN_BYTE(7) & 0xFF) > 0), Volts)); // Charger Output Volts
    break;
    }

    case 0x00fefc4c: // SOC
    {
    unsigned int t;

    // SOC:
    t = (((int)CAN_BYTE(0) & 0xFF )* 100);
    if (t > 0 && t <= 40000)
    {
      vx1_soc = t;
      *StdMetrics.ms_v_bat_soc = (float) vx1_soc / 256;

      // Remember maximum SOC for charging "done" distinction:
      if (vx1_soc > vx1_soc_max)
        vx1_soc_max = vx1_soc;

      // ...and minimum SOC for range calculation during charging:
      if (vx1_soc < vx1_soc_min)
      {
        vx1_soc_min = vx1_soc;
        vx1_soc_min_range = vx1_range_est;
      }
    }
    // SOH:
      vx1_soh = (float) 56;
      *StdMetrics.ms_v_bat_soh = vx1_soh;
    break;
    }

    case 0x00fefd4c: // SOC
    {
      // VEHICLE state:
      //  [0]: 0x20 = power line connected
      if ((CAN_BYTE(2) & 0xFF) != 0x00)
        *StdMetrics.ms_v_charge_voltage = (float) (CAN_BYTE(2) & 0xFF); // AC incoming 230 V
      else
        *StdMetrics.ms_v_charge_voltage = (float) 0;

        // init cyclic distance counter on switch-on:
        if ((CAN_BYTE(0) & CAN_STATUS_KEYON) && (!(vx1_status & CAN_STATUS_KEYON)))
          vx1_dist = vx1_speed_distref = 0;

        if (CAN_BYTE(1) & CAN_STATUS_OFFLINE)
          vx1_status = (vx1_status & 0x07) | (CAN_BYTE(1) & 0xF0);
        else
          vx1_status = (vx1_status & 0x0F) | (CAN_BYTE(1) & 0xF0);

        // Read 12V DC converter status:
        //vx1_flags.Charging12V = (int) 0;
      break;
    }

    case 0x00ff0a4c: // Charger Status
    {
    StandardMetrics.ms_v_charge_time->SetValue((int)(((uint16_t)CAN_BYTE(4) <<8) + ((uint16_t)CAN_BYTE(5))), Seconds);
    break;
    }

    case 0x00fee017: // Odometer from Instrument Cluster
    {
    StandardMetrics.ms_v_pos_odometer->SetValue((float)(((uint32_t)CAN_BYTE(4)<<24)
                                              + ((uint32_t)CAN_BYTE(5)<<16)
                                              + ((uint32_t)CAN_BYTE(6)<<8)
                                              + CAN_BYTE(7))*0.05, Kilometers);
    break;
    }

    case 0x18fee617:
    {
    // Clock:
    //if (CAN_BYTE(1) > 0 || CAN_BYTE(2) > 0 || CAN_BYTE(3) > 0)
    //vv_clock=(((CAN_BYTE(0)*60) + CAN_BYTE(1))*60) + CAN_BYTE(2);

    // check If Go Ready "ignition"
    uint8_t indGoReady = ((CAN_BYTE(0) & 0x01) || (CAN_BYTE(0) & 0x04));
    if ( indGoReady == 5) {
      StandardMetrics.ms_v_env_on->SetValue(true);
    } else {
      StandardMetrics.ms_v_env_on->SetValue(false);
    }
    break;
    }

    case 0x00FEF340: // BMS brick module temperatures
    {
    int k = ((CAN_BYTE(7)&0xf0)>>4);
    int v1 = ((int)(CAN_BYTE(1)&0xFF));
    BmsSetCellTemperature(k, v1);
    //ESP_LOGD(TAG, "BMS %01x %02x", k, v1);
    //BmsSetCellTemperature(k+1, 0.0122 * ((v2 & 0x1FFF) - (v2 & 0x2000)));
    //BmsSetCellTemperature(k+2, 0.0122 * ((v3 & 0x1FFF) - (v3 & 0x2000)));
    //BmsSetCellTemperature(k+3, 0.0122 * ((v4 & 0x1FFF) - (v4 & 0x2000)));
    break;
    }

    // Twizy to do adapt
    case 0x424:
      // --------------------------------------------------------------------------
      // CAN ID 0x424: sent every 100 ms (10 per second)

      // max drive (discharge) + recup (charge) power:
      if (CAN_BYTE(2) != 0xff)
        vx1_batt[0].max_recup_pwr = CAN_BYTE(2);
      if (CAN_BYTE(3) != 0xff)
        vx1_batt[0].max_drive_pwr = CAN_BYTE(3);

      // --------------------------------------------------------------------------
      // CAN ID 0x59E: sent every 100 ms (10 per second)

      // CYCLIC DISTANCE COUNTER:
      vx1_dist = ((UINT) CAN_BYTE(0) << 8) + CAN_BYTE(1);

      // --------------------------------------------------------------------------
      // *** CHARGER status ***
      // sent every 100 ms (10 per second) in drive & charge mode
      mt_charger_status->SetValue(CAN_UINT24(0));

      // --------------------------------------------------------------------------
      // *** BMS status ***
      // sent every 100 ms (10 per second) in drive & charge mode
      mt_bms_status->SetValue(CAN_UINT24(0));
      mt_bms_alert_12v->SetValue((CAN_BYTE(2) & 0x20) == 0x20);
      mt_bms_alert_batt->SetValue((CAN_BYTE(2) & 0x40) == 0x40);
      mt_bms_alert_temp->SetValue((CAN_BYTE(2) & 0x80) == 0x80);
      break;

#if 0
    case 0x18EF05F9:
      // --------------------------------------------------------------------------
      // *** VIN ***
      // last 7 digits of real VIN, in nibbles, reverse:
      // (assumption: no hex digits)
      if (!vx1_vin[0]) // we only need to process this once
      {
        vx1_vin[0] = '0' + CAN_NIB(7);
        vx1_vin[1] = '0' + CAN_NIB(6);
        vx1_vin[2] = '0' + CAN_NIB(5);
        vx1_vin[3] = '0' + CAN_NIB(4);
        vx1_vin[4] = '0' + CAN_NIB(3);
        vx1_vin[5] = '0' + CAN_NIB(2);
        vx1_vin[6] = '0' + CAN_NIB(1);
        vx1_vin[7] = 0;
        *StdMetrics.ms_v_vin = (string) vx1_vin;
      }
      break;
#endif


  }

}
