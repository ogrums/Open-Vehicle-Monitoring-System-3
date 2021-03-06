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

//#include "ovms_log.h"
static const char *TAG = "v-vectrixvx1";

#include "metrics_standard.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_events.h"
#include "ovms_metrics.h"
#include "ovms_notify.h"
#include "pcp.h"
#include <iomanip>
#include <stdio.h>
#include <string>

#include "vehicle_vectrixvx1.h"

using namespace std;

// CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b) can_databuffer[b]
#define CAN_UINT(b) (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b + 1))
#define CAN_UINTL(b) (((UINT)CAN_BYTE(b + 1) << 8) | CAN_BYTE(b))
#define CAN_UINT24(b)                                                          \
  (((UINT32)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b + 1) << 8) | CAN_BYTE(b + 2))
#define CAN_UINT32(b)                                                          \
  (((UINT32)CAN_BYTE(b) << 24) | ((UINT32)CAN_BYTE(b + 1) << 16) |             \
   ((UINT)CAN_BYTE(b + 2) << 8) | CAN_BYTE(b + 3))
#define CAN_UINT32L(b)                                                          \
     (((UINT32)CAN_BYTE(b + 3) << 24) | ((UINT32)CAN_BYTE(b + 2) << 16) |             \
      ((UINT)CAN_BYTE(b + 1) << 8) | CAN_BYTE(b))
#define CAN_NIBL(b) (can_databuffer[b] & 0x0f)
#define CAN_NIBH(b) (can_databuffer[b] >> 4)
#define CAN_NIB(n) (((n)&1) ? CAN_NIBL((n) >> 1) : CAN_NIBH((n) >> 1))

/**
 * Asynchronous CAN RX handler
 */

void OvmsVehicleVectrixVX1::IncomingFrameCan1(CAN_frame_t *p_frame) {
  unsigned int u;
  signed int recd;
  unsigned int BusVoltage;

  uint8_t *can_databuffer = p_frame->data.u8;

  switch (p_frame->MsgID) {

  case 0x18FEE017: // Odometer + Trip A from Instrument Cluster
  {
    // *** ODOMETER ***
    StandardMetrics.ms_v_pos_odometer->SetValue((float)CAN_UINT32L(4) * 0.05);
    // trip A
    StandardMetrics.ms_v_pos_trip->SetValue((float)CAN_UINTL(0) * 0.05);
    break;
  }

  case 0x00FEF106:
  {
    // Charging Mode
    // Mesured Motor Controller voltage used
    // StdMetrics.ms_v_bat_voltage->SetValue((float) (CAN_BYTE(6) > 0), Volts);
    // // Volts
    break;
  }

  case 0x00EEE005: // Motor Controller AmpSeconds, Ah Mesured since Vehicle On
  {
    recd = (signed int)CAN_UINT32L(0);
    BusVoltage = CAN_BYTE(6);
    StandardMetrics.ms_v_bat_energy_recd->SetValue((float)((recd * 0.000032744)* BusVoltage / 1000));
    StandardMetrics.ms_v_bat_energy_used->SetValue((float)CAN_UINTL(4) * BusVoltage / 1000);
    StandardMetrics.ms_v_bat_coulomb_recd->SetValue((float)((recd * 0.000032744)));
    StandardMetrics.ms_v_bat_coulomb_used->SetValue((float)CAN_UINTL(4));
    break;
  }

  case 0x00FF0505: // Motor Controller indicator, temparature
  {
    // Regen:
    // StdMetrics.ms_v_pos_speed->SetValue((double) ((CAN_BYTE(2) << 8) +
    // CAN_BYTE(1)) * 0.00390625, Kph); // kph
    // Motor Controller Temperature
    // ?????_mc_temp->SetValue((float) (CAN_BYTE(3) > 0), Celcius); // Deg C
    // Motor controller Capacitor Temperature
    // ?????_mc_cap1temp->SetValue((float) (CAN_BYTE(4) > 0), Celcius); // Deg C
    // ?????_mc_cap2temp->SetValue((float) (CAN_BYTE(5) > 0), Celcius); // Deg C
    // ?????_mc_cap3temp->SetValue((float) (CAN_BYTE(6) > 0), Celcius); // Deg C

    // Status GO, Ready, VPE Indicator Byte 0
    // m_v_env_readyindic->SetValue((CAN_BYTE(0) & 0x01) > 0);//Byte 0 - Bit 0 -
    // Ready
    // m_v_env_goindic->SetValue((CAN_BYTE(0) & 0x04) > 0);		//Byte 0 - Bit 2 -
    // Go m_v_env_vpeindic->SetValue((CAN_BYTE(0) & 0x10) > 0);	//Byte 0 - Bit 4
    // - VPE

    StandardMetrics.ms_v_inv_temp->SetValue((int)CAN_BYTE(3));
    StandardMetrics.ms_v_mot_temp->SetValue((int)CAN_BYTE(4));
    break;
  }

  case 0x00FEF105: // MC voltage and speed + Power
  {
    // RANGE:
    // we need to check for charging, as the Twizy
    // does not update range during charging
    if (((vx1_status & 0x60) == 0) && (can_databuffer[5] != 0xff) &&
        (can_databuffer[5] > 0)) {
      vx1_range_est = can_databuffer[5];
      // car values derived in ticker1()
    }

    // SPEED:
    u = ((((unsigned int)CAN_BYTE(2) & 0xff) << 8) +
         ((unsigned int)CAN_BYTE(1) & 0xff));
    if (u != 0xffff) {
      vx1_speed = u;
      *StdMetrics.ms_v_pos_speed = (float)vx1_speed * 0.00390625;
    }
    // StandardMetrics.ms_v_pos_speed->SetValue((float)(((((int)CAN_BYTE(2)&0xff)<<8)
    // + ((int)CAN_BYTE(1)&0xff))*0.00390625));
    // Don't update battery voltage too quickly (as it jumps around like crazy)
    if (StandardMetrics.ms_v_bat_voltage->Age() > 10) {
      StandardMetrics.ms_v_bat_voltage->SetValue((int)CAN_BYTE(6));

      // StandardMetrics.ms_v_bat_temp->SetValue((float)((((int)CAN_BYTE(7)&0x07)<<8)+CAN_BYTE(6))/10);
    }
    //CURRENT & POWER:
     StandardMetrics.ms_v_bat_current->SetValue((float)(CAN_BYTE(7)*0.488));
    break;
  }

  case 0x00FDF040: // BMS Summary
  {
    // int k = (((CAN_BYTE(7)&0xf0)>>4)&0xf);
    int v1 =
        (((int)(CAN_BYTE(0) & 0xFF) << 4) & 0xFF0) + ((int)CAN_BYTE(5) & 0xF);
    int v2 = (((int)(CAN_BYTE(1) & 0xFF) << 4) & 0xFF0) +
             (((int)CAN_BYTE(5) >> 4) & 0xF);
    int v3 =
        (((int)(CAN_BYTE(2) & 0xFF) << 4) & 0xFF0) + ((int)CAN_BYTE(6) & 0xF);
    int v4 = (((int)(CAN_BYTE(3) & 0xFF) << 4) & 0xFF0) +
             (((int)CAN_BYTE(6) >> 4) & 0xF);
    int v5 =
        (((int)(CAN_BYTE(4) & 0xFF) << 4) & 0xFF0) + ((int)CAN_BYTE(7) & 0xF);
    if ((v1 != 0xfff) && (v2 != 0xfff) && (v3 != 0xfff) && (v4 != 0xfff) &&
        (v5 != 0xfff) && (v1 != 0) && (v2 != 0) && (v3 != 0) && (v4 != 0) &&
        (v5 != 0)) {
      // Voltages
      int k = (((CAN_BYTE(7) & 0xf0) >> 4) & 0xf);
      k = ((k * 5) - 5);
      BmsSetCellVoltage(k, 0.0015 * v1);
      BmsSetCellVoltage(k + 1, 0.0015 * v2);
      BmsSetCellVoltage(k + 2, 0.0015 * v3);
      BmsSetCellVoltage(k + 3, 0.0015 * v4);
      BmsSetCellVoltage(k + 4, 0.0015 * v5);
      // ESP_LOGD(TAG, "BMS4%02x %03x %03x %03x %03x %03x", k, v1, v2, v3, v4,
      // v5);
    }
    break;
  }

  case 0x00F00300: // Gear selector
  {
    // twizy_status low nibble:
    vx1_status = (vx1_status & 0xF0) | (CAN_BYTE(1) & 0x09);

    // accelerator throttle:
    u = ((((unsigned int)CAN_BYTE(1) & 0xff) << 8) +
         ((unsigned int)CAN_BYTE(0) & 0xff));

    if (u > 0x0276) {
      *StdMetrics.ms_v_env_gear = (int)1;
      vx1_accel_pedal = (float)((u - 625) * 100 / 210);
      *StdMetrics.ms_v_env_throttle =
          (float)(vx1_accel_pedal); // 840 = maxthrottle value - 530 min
                                    // throttle value
      *StdMetrics.ms_v_env_footbrake = 0;
    } else if (u < 0x026C) {
      *StdMetrics.ms_v_env_gear = (int)-1;
      vx1_accel_pedal = (float)((625 - u) * -100 / 90);
      *StdMetrics.ms_v_env_footbrake = (float)(vx1_accel_pedal);
      *StdMetrics.ms_v_env_throttle = 0;
    } else {
      // Neutral Throttle position between 620 and 630
      *StdMetrics.ms_v_env_gear = (int)0;
      *StdMetrics.ms_v_env_throttle = 0;
      *StdMetrics.ms_v_env_footbrake = 0;
      vx1_accel_pedal = 0;
    }

    // running average over 2 samples:
    // u = (vx1_accel_pedal + u + 1) >> 1;

    // kickdown detection:
    // s = KickdownThreshold(vx1_accel_pedal);
    // if ( ((s > 0) && (u > ((unsigned int)vx1_accel_pedal + s)))
    //  || ((vx1_kickdown_level > 0) && (u > vx1_kickdown_level)) )
    //{
    vx1_kickdown_level = u;
    //  m_sevcon->Kickdown(true);
    //}

    vx1_accel_pedal = u;

    break;
  }

  case 0x00FEE04C:
  {
    // Estimated range:
    StandardMetrics.ms_v_bat_range_est->SetValue((float)(CAN_UINT32L(4)) * 0.5, Kilometers);
    // ESP_LOGV(TAG, "IncomingFrameCan1: Range estimated %d",
    // StandardMetrics.ms_v_bat_range_est);
    break;
  }

  case 0x00fee24c: // Charging related
  {
    // to-do
    break;
  }

  case 0x00fef14c: // Charging Status Nimh ?
  {
    float charge_v = (float)(uint16_t(CAN_BYTE(1) << 8) + CAN_BYTE(0)) / 256;
    if ((charge_v != 0) && (m_charge_w != 0)) { // Charging
      // StandardMetrics.ms_v_charge_current->SetValue((unsigned
      // int)(m_charge_w/charge_v));
      // StandardMetrics.ms_v_charge_voltage->SetValue((unsigned int)charge_v);
      // StandardMetrics.ms_v_charge_current->SetValue((float)(((((int)CAN_BYTE(2)&0xff)<<8)
      // + ((int)CAN_BYTE(1)&0xff))*0.000390625));
      StandardMetrics.ms_v_charge_pilot->SetValue(true);
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_charge_mode->SetValue("standard");
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      StandardMetrics.ms_v_charge_climit->SetValue(
          (unsigned int)(m_charge_w / charge_v));
    } else { // Not charging
      // StandardMetrics.ms_v_charge_current->SetValue(0);
      // StandardMetrics.ms_v_charge_voltage->SetValue(0);
      StandardMetrics.ms_v_charge_pilot->SetValue(false);
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_mode->SetValue("standard");
      StandardMetrics.ms_v_charge_state->SetValue("done");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      StandardMetrics.ms_v_charge_climit->SetValue(0);
    }
    break;
  }

  case 0x00fef74c: // Charger status
  {
    // m_v_preheat_timer1_enabled->SetValue( CAN_BYTE(0) & 0x1 );
    // m_v_preheat_timer2_enabled->SetValue( CAN_BYTE(3) & 0x1 );
    if (((int)CAN_BYTE(0) & 0x0A) != 0) {
      StdMetrics.ms_v_charge_inprogress->SetValue(true);
      StdMetrics.ms_v_charge_current->SetValue(
          (float)(CAN_BYTE(4) * 0.1)); // Charger Output Current
      StdMetrics.ms_v_charge_voltage->SetValue(
          (unsigned int)CAN_BYTE(7)); // Charger Output Volts
    } else {
      StdMetrics.ms_v_charge_inprogress->SetValue(false);
      StdMetrics.ms_v_charge_current->SetValue(0);
      StdMetrics.ms_v_charge_voltage->SetValue(0);
    }
    break;
  }

  case 0x00fefc4c: // SOC
  {
    StandardMetrics.ms_v_bat_soc->SetValue((float)(CAN_BYTE(0) * 0.390625));
    break;
  }

  case 0x00fefd4c: // SOC
  {
    // VEHICLE state:
    //  [0]: 0x20 = power line connected
    if ((CAN_BYTE(2) & 0xFF) != 0x00) {
      //*StdMetrics.ms_v_charge_voltage = (float) (CAN_BYTE(2) & 0xFF); // AC
      //incoming 230 V
    } else {
      //*StdMetrics.ms_v_charge_voltage = (float) 0;
    }
    break;
  }

  case 0x00ff0a4c: // Charger Status
  {
    StandardMetrics.ms_v_charge_time->SetValue((unsigned int)CAN_UINTL(4));
    break;
  }

  case 0x18fee617:
  {
    // Clock:
    // if (CAN_BYTE(1) > 0 || CAN_BYTE(2) > 0 || CAN_BYTE(3) > 0)
    // vv_clock=(((CAN_BYTE(0)*60) + CAN_BYTE(1))*60) + CAN_BYTE(2);

    // check If Go Ready "ignition"
    uint8_t indGoReady = ((CAN_BYTE(0) & 0x01) || (CAN_BYTE(0) & 0x04));
    if (indGoReady == 5) {
      StandardMetrics.ms_v_env_on->SetValue(true);
    } else {
      StandardMetrics.ms_v_env_on->SetValue(false);
    }
    break;
  }

  case 0x00FEF340: // BMS brick module temperatures
  {
    int k = ((CAN_BYTE(7) & 0xf0) >> 4);
    int v1 = ((int)(CAN_BYTE(1) & 0xFF));
    BmsSetCellTemperature(k, v1);
    // ESP_LOGD(TAG, "BMS %01x %02x", k, v1);
    break;
  }

  case 0x18EF05F9:
  {
      // --------------------------------------------------------------------------
      // *** VIN ***
      // last 7 digits of real VIN, in nibbles, reverse:
      // (assumption: no hex digits)
      //if (!vx1_vin[0]) // we only need to process this once
      //{
      //  vx1_vin[0] = '0' + CAN_NIB(7);
      //  vx1_vin[1] = '0' + CAN_NIB(6);
      //  vx1_vin[2] = '0' + CAN_NIB(5);
      //  vx1_vin[3] = '0' + CAN_NIB(4);
      //  vx1_vin[4] = '0' + CAN_NIB(3);
      //  vx1_vin[5] = '0' + CAN_NIB(2);
      //  vx1_vin[6] = '0' + CAN_NIB(1);
      //  vx1_vin[7] = 0;
      //  *StdMetrics.ms_v_vin = (string) vx1_vin;
      //}
      break;
   }




  default:
  {
    break;
  }

 }
}

void OvmsVehicleVectrixVX1::IncomingFrameCan2(CAN_frame_t *p_frame) {

  uint8_t *can_databuffer = p_frame->data.u8;

  switch (p_frame->MsgID) {

  //case 0x002: // Adresse BMS 0x002
  //case 0x001: // Adresse from 0x001 to 0x00F BMS JK-DZ08-BxA24S (x = 1, 2, 3 A equalizer)
  default:
   {
    // Response from request 0x001 : 0xFF (Len 1 byte)
    //
    // Adresse sames as above 0x001
    // Frame 1 : 0x01, Byte#1#2 (Temp °C), Byte#3#4 (*10mv TotalVoltage), Byte#5#6 (AverageVoltageCell), Byte#7 identify quantity
    // Frame 2 : 0x02, Highest cell, Lowest Cell, Balance with report, Byte#3#4, largest volt diff mV, Byte#6#7 equalizer current mA
    // Frame 3 : 0x03, Byte#1#2 Trigger Diff voltage mV, Byte#3#4 max balance current mA, Balanced switch, Nb of cells, Byte#7 nothing
    // Frame 4 : 0x04, cell Number, byte#2#3 cell voltage N mV, byte#4#5 cell voltage N+1 mV, byte#6#7 cell voltage N+2 mV
    // Notes :Equalization and alarm bytes "Balance with report"
    //  BIT0 means equalized battery (0) Stanby / (1) charging cell
    //  BIT1 means equalized battery (0) Stanby / (1) discharge cell
    //  BIT4 Number of units set (0) match correctly / (1) don't match
    //  BIT5 Wire Resistance is (0) normal / (1) Bad
    // Identify quantity is number of cell check by BMS, number of cells is the numer put in setting
    // The Cell Number N is the number of the firt cell voltage in the frame
    // Balanced Switch : 0x01 = On / 0x00 = Off

    int bmsn = p_frame->MsgID;
    //ESP_LOGD(TAG, "BMS Number : %02d", bmsn);

    switch (CAN_BYTE(0))
    {
      case 0x01: // Temperation °C, Total Voltage, Average Cell Voltage, Number Cell Check
      {
        int tempjk = CAN_UINT(1); // Temperature Sensor
        if (bmsn == 1) {
          BmsSetCellTemperature(1, tempjk);
        } else {
          BmsSetCellTemperature(2, tempjk);
        }
        int totalvoltagejk = CAN_UINT(3); // Total Voltage BMS
        int avgvoltagecell = CAN_UINT(5); // Average Voltage Cell
        int detectedcell = CAN_BYTE(7); // Number of Cell detected
        ESP_LOGD(TAG, "BMS Nb : %02d, Temp : %02d, TotalVoltage : %04d, AvgCellVolt : %04d, NbCell : %02d", bmsn, tempjk, totalvoltagejk, avgvoltagecell, detectedcell);

        break;
      }
      case 0x02: // Highest Cell Number, Lowest Cell Number, Status Equal, Max diff Volt mV, Equal Current mA
      {
        int highestcell = CAN_BYTE(1); // Number highest Cell
        int lowestcell = CAN_BYTE(2); // Number Lowest Cell
        int balstatus = CAN_BYTE(3); // Balance status report
        int voltdiff = CAN_UINT(4); // Largest volt Differential
        int equalizcurrent = CAN_UINT(6); // Equalization current realtime
        ESP_LOGD(TAG, "BMS NbH : %02d, NbL : %02d, BalStatus : %02x, VolDiff : %04d, EquaCurr : %04d", highestcell, lowestcell, balstatus, voltdiff, equalizcurrent);
        break;
      }
      case 0x03: // Setting BMS Trigger Diff Voltage mV, Max Balance Current Ma, Active Balanced, Nb cell Setting
      {
        int triggerdiffvolt = CAN_UINT(1); // Trigger diff voltage for balancing
        int maxbalcurr = CAN_UINT(3); // Max Current Set for balancing
        int balswitch = CAN_BYTE(5); // Balancing switch 0x00 = Off; 0x01 = On
        int nbcellset = CAN_BYTE(6); // Number Cells Set
        ESP_LOGD(TAG, "BMS triggerV : %04d, MaxCurr : %04d, SwitchBal : %02x, NbCellSet : %02d", triggerdiffvolt, maxbalcurr, balswitch, nbcellset);
        break;
      }
      case 0x04: // Cell voltage N, N+1 and N+2 in mV
      {
        switch (CAN_BYTE(1))
        {
          case 0x00: // Cells 1-3
          {
            if (bmsn == 2) {
              jkbms_cell[20].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[21].volt_act = CAN_UINT(4); // Cell N+1
              jkbms_cell[22].volt_act = CAN_UINT(6); // Cell N+2
            } else {
              jkbms_cell[0].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[1].volt_act = CAN_UINT(4); // Cell N+1
              jkbms_cell[2].volt_act = CAN_UINT(6); // Cell N+2
            }
            break;
          }
          case 0x03: // Cells 4-6
          {
            if (bmsn == 2) {
              jkbms_cell[23].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[24].volt_act = CAN_UINT(4); // Cell N+1
              jkbms_cell[25].volt_act = CAN_UINT(6); // Cell N+2
            } else {
              jkbms_cell[3].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[4].volt_act = CAN_UINT(4); // Cell N+1
              jkbms_cell[5].volt_act = CAN_UINT(6); // Cell N+2
            }
            break;
          }
          case 0x06: // Cells 7-9
          {
            if (bmsn == 2) {
              jkbms_cell[26].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[27].volt_act = CAN_UINT(4); // Cell N+1
              jkbms_cell[28].volt_act = CAN_UINT(6); // Cell N+2
            } else {
              jkbms_cell[6].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[7].volt_act = CAN_UINT(4); // Cell N+1
              jkbms_cell[8].volt_act = CAN_UINT(6); // Cell N+2
            }
            break;
          }
          case 0x09: // Cells 10-12
          {
            if (bmsn == 2) {
              jkbms_cell[29].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[30].volt_act = CAN_UINT(4); // Cell N+1
              jkbms_cell[31].volt_act = CAN_UINT(6); // Cell N+2
            } else {
              jkbms_cell[9].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[10].volt_act = CAN_UINT(4); // Cell N+1
              jkbms_cell[11].volt_act = CAN_UINT(6); // Cell N+2
            }
            break;
          }
          case 0x0C: // Cells 13-15
          {
            if (bmsn == 2) {
              jkbms_cell[32].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[33].volt_act = CAN_UINT(4); // Cell N+1
              jkbms_cell[34].volt_act = CAN_UINT(6); // Cell N+2
            } else {
              jkbms_cell[12].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[13].volt_act = CAN_UINT(4); // Cell N+1
              jkbms_cell[14].volt_act = CAN_UINT(6); // Cell N+2
            }
            break;
          }
          case 0x0F: // Cells 16-18
          {
            if (bmsn == 2) {
              jkbms_cell[35].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[36].volt_act = CAN_UINT(4); // Cell N+1
              //jkbms_cell[37].volt_act = CAN_UINT(6); // Cell N+2
            } else {
              jkbms_cell[15].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[16].volt_act = CAN_UINT(4); // Cell N+1
              jkbms_cell[17].volt_act = CAN_UINT(6); // Cell N+2
            }
            break;
          }
          case 0x12: // Cells 19-21
          {
            if (bmsn == 2) {
              //jkbms_cell[38].volt_act = CAN_UINT(2); // Cell N
              //jkbms_cell[39].volt_act = CAN_UINT(4); // Cell N+1
              //jkbms_cell[40].volt_act = CAN_UINT(6); // Cell N+2
            } else {
              jkbms_cell[18].volt_act = CAN_UINT(2); // Cell N
              jkbms_cell[19].volt_act = CAN_UINT(4); // Cell N+1
              //jkbms_cell[20] = CAN_UINT(6); // Cell N+2
            }
            break;
          }
          case 0x15: // Cells 22-24
          {
            if (bmsn == 2) {
              //jkbms_cell[41].volt_act = CAN_UINT(2); // Cell N
              //jkbms_cell[42].volt_act = CAN_UINT(4); // Cell N+1
              //jkbms_cell[43].volt_act = CAN_UINT(6); // Cell N+2
            } else {
              //jkbms_cell[21].volt_act = CAN_UINT(2); // Cell N
              //jkbms_cell[22].volt_act = CAN_UINT(4); // Cell N+1
              //jkbms_cell[23].volt_act = CAN_UINT(6); // Cell N+2
            }
            break;
          }
        }
        int i;
        for (i = 0; i < 37; i++) {
          BmsSetCellVoltage(i, (float) jkbms_cell[i].volt_act * 0.001);
          //BmsSetCellVoltage(k + 1, 0.001 * v1jk);
          //BmsSetCellVoltage(k + 2, 0.001 * v2jk);
          //BmsSetCellVoltage(k + 3, 0.001 * v3jk);
          //ESP_LOGD(TAG, "BMS %02d, NbCell : %02x, CellN %04d %04d %04d", bmsn, k, v1jk, v2jk, v3jk);
          //ESP_LOGD(TAG, "BMS %02d, Cells volt : %04d", i, jkbms_cell[i].volt_act);
        }

       break;
      }
      break;
    }
    break;
  }

  //default:
  //{
  //  break;
  //}

 }
}

OvmsVehicle::vehicle_command_t OvmsVehicleVectrixVX1::CommandStartBalance()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can2;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 2;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x001;
  frame.data.u8[0] = 0xF6;
  frame.data.u8[1] = 0x01; // Active Balance
  m_can2->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVectrixVX1::CommandStopBalance()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can2;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 2;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x001;
  frame.data.u8[0] = 0xF6;
  frame.data.u8[1] = 0x00; // Stop Balance
  m_can2->Write(&frame);

  return Success;
  }

void OvmsVehicleVectrixVX1::SendRequestInfoBmsOne()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can2;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 1;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x001;
  frame.data.u8[0] = 0xFF; //Request Info
  m_can2->Write(&frame);

  }
void OvmsVehicleVectrixVX1::SendRequestInfoBmsTwo()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can2;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 1;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x002;
  frame.data.u8[0] = 0xFF; //Request Info
  m_can2->Write(&frame);

  }
