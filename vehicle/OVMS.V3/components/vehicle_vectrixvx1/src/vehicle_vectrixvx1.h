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

#ifndef __VEHICLE_VECTRIXVX1_H__
#define __VEHICLE_VECTRIXVX1_H__

#include "can.h"
#include "vehicle.h"

#include "ovms_log.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_command.h"

#include "vv_types.h"
#include "vv_battmon.h"
#include "vv_pwrmon.h"

using namespace std;

class OvmsVehicleVectrixVX1: public OvmsVehicle
  {
  public:
    OvmsVehicleVectrixVX1();
    ~OvmsVehicleVectrixVX1();

  // --------------------------------------------------------------------------
  // Framework integration
  //

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);

  protected:
    static size_t m_modifier;
    OvmsMetricString *m_version;
    OvmsCommand *cmd_xvv;

  // --------------------------------------------------------------------------
  // General status
  //

  public:
    vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);

  protected:
    int vx1_tmotor = 0;                           // motor temperature [°C]
    int vx1_soh = 0;                              // BMS SOH [%]

    unsigned int vx1_last_soc = 0;                // sufficient charge SOC threshold helper
    unsigned int twizy_last_idealrange = 0;         // sufficient charge range threshold helper

    unsigned int vx1_soc = 5000;                  // detailed SOC (1/100 %)
    unsigned int vx1_soc_min = 10000;             // min SOC reached during last discharge
    unsigned int vx1_soc_min_range = 100;         // twizy_range at min SOC
    unsigned int vx1_soc_max = 0;                 // max SOC reached during last charge
    unsigned int vx1_soc_tripstart = 0;           // SOC at last power on
    unsigned int vx1_soc_tripend = 0;             // SOC at last power off

    int cfg_suffsoc = 0;                            // Configured sufficient SOC
    int cfg_suffrange = 0;                          // Configured sufficient range

    #define CFG_DEFAULT_MAXRANGE 80
    int cfg_maxrange = CFG_DEFAULT_MAXRANGE;        // Configured max range at 20 °C
    float vx1_maxrange = 0;                       // Max range at current temperature

    #define CFG_DEFAULT_CAPACITY 108
    float cfg_bat_cap_actual_prc = 100;     // actual capacity in percent of…
    float cfg_bat_cap_nominal_ah = CFG_DEFAULT_CAPACITY; // …usable capacity of fresh battery in Ah

    int vx1_chargeduration = 0;           // charge duration in seconds

    unsigned int vx1_range_est = 50;              // Twizy estimated range in km
    unsigned int vx1_range_ideal = 50;            // calculated ideal range in km

    unsigned int vx1_speed = 0;                   // current speed in 1/100 km/h
    unsigned long vx1_odometer = 0;               // current odometer in 1/100 km = 10 m
    unsigned long vx1_odometer_tripstart = 0;     // odometer at last power on
    unsigned int vx1_dist = 0;                    // cyclic distance counter in 1/10 m = 10 cm


    // GPS log:
    int cfg_gpslog_interval = 5;        // GPS-Log interval while driving [seconds]
    uint32_t vx1_last_gpslog = 0;     // Time of last GPS-Log update

    // Notifications:
    #define SEND_BatteryAlert           (1<< 0)  // alert: battery problem
    #define SEND_PowerNotify            (1<< 1)  // info: power usage summary
    #define SEND_PowerStats             (1<< 2)  // data: RT-PWR-Stats history entry
    #define SEND_GPSLog                 (1<< 3)  // data: RT-GPS-Log history entry
    #define SEND_BatteryStats           (1<< 4)  // data: separate battery stats (large)
    #define SEND_CodeAlert              (1<< 5)  // alert: fault code (SEVCON/inputs/...)
    #define SEND_TripLog                (1<< 6)  // data: RT-PWR-Log history entry
    #define SEND_ResetResult            (1<< 7)  // info/alert: RESET OK/FAIL
    #define SEND_SuffCharge             (1<< 8)  // info: sufficient SOC/range reached
    #define SEND_SDOLog                 (1<< 9)  // data: RT-ENG-SDO history entry

    // NOTE: the power values are derived by...
    //    vx1_power = ( vx1_current * vx1_batt[0].volt_act + 128 ) / 256
    // The vx1_current measurement has a high time resolution of 1/100 s,
    // but the battery voltage has a low time resolution of just 1 s, so the
    // power values need to be seen as an estimation.

    signed int vx1_power = 0;                     // momentary battery power in 256/40 W = 6.4 W, negative=charging
    signed int vx1_power_min = 32767;             // min "power" in current GPS log section
    signed int vx1_power_max = -32768;            // max "power" in current GPS log section

    // power is collected in 6.4 W & 1/100 s resolution
    // to convert power sums to Wh:
    //  Wh = power_sum * 6.4 * 1/100 * 1/3600
    //  Wh = power_sum / 56250
    #define WH_DIV (56250L)
    #define WH_RND (28125L)


    signed int vx1_current = 0;           // momentary battery current in 1/4 A, negative=charging
    signed int vx1_current_min = 32767;   // min battery current in GPS log section
    signed int vx1_current_max = -32768;  // max battery current in GPS log section

    UINT32 vx1_charge_use = 0;            // coulomb count: batt current used (discharged) 1/400 As
    UINT32 vx1_charge_rec = 0;            // coulomb count: batt current recovered (charged) 1/400 As

    // cAh = [1/400 As] / 400 / 3600 * 100
    #define CAH_DIV (14400L)
    #define CAH_RND (7200L)
    #define AH_DIV (CAH_DIV * 100L)

    int vx1_chargestate = 0;              // 0="" (none), 1=charging, 2=top off, 4=done, 21=stopped charging

    UINT8 vx1_chg_power_request = 0;      // BMS to CHG power level request (0..7)

    // battery capacity helpers:
    UINT32 vx1_cc_charge = 0;             // coulomb count: charge sum in CC phase 1/400 As
    UINT8 vx1_cc_power_level = 0;         // end of CC phase detection
    UINT vx1_cc_soc = 0;                  // SOC at end of CC phase

    #define CFG_DEFAULT_CAPACITY 108
    float cfg_bat_cap_actual_prc = 100;     // actual capacity in percent of…
    float cfg_bat_cap_nominal_ah = CFG_DEFAULT_CAPACITY; // …usable capacity of fresh battery in Ah

    int vx1_chargeduration = 0;           // charge duration in seconds

    int cfg_aux_fan_port = 0;               // EGPIO port number for auxiliary charger fan
    int cfg_aux_charger_port = 0;           // EGPIO port number for auxiliary charger

    UINT8 vx1_fan_timer = 0;              // countdown timer for auxiliary charger fan
    #define VX1_FAN_THRESHOLD   45    // temperature in °C
    #define VX1_FAN_OVERSHOOT   5     // hold time in minutes after switch-off


    // Accelerator pedal & kickdown detection:

    volatile UINT8 vx1_accel_pedal;           // accelerator pedal (running avg for 2 samples = 200 ms)
    volatile UINT8 vx1_kickdown_level;        // kickdown detection & pedal max level
    UINT8 vx1_kickdown_hold;                  // kickdown hold countdown (1/10 seconds)
    #define VX1_KICKDOWN_HOLDTIME 50          // kickdown hold time (1/10 seconds)

    #define CFG_DEFAULT_KD_THRESHOLD    35
    #define CFG_DEFAULT_KD_COMPZERO     120
    int cfg_kd_threshold = CFG_DEFAULT_KD_THRESHOLD;
    int cfg_kd_compzero = CFG_DEFAULT_KD_COMPZERO;

    // pedal nonlinearity compensation:
    //    constant threshold up to pedal=compzero,
    //    then linear reduction by factor 1/8
    int KickdownThreshold(int pedal) {
      return (cfg_kd_threshold - \
        (((int)(pedal) <= cfg_kd_compzero) \
          ? 0 : (((int)(pedal) - cfg_kd_compzero) >> 3)));
    }



  protected:
    unsigned int vx1_notifications = 0;
    void RequestNotify(unsigned int which);
    void DoNotify();

    void SendGPSLog();
    void SendTripLog();



  // --------------------------------------------------------------------------
  // Webserver subsystem
  //  - implementation: vv_web.(h,cpp)
  //

  public:
    void WebInit();
    void WebDeInit();
    //static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    //static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);

  public:
    void GetDashboardConfig(DashboardConfig& cfg);

  protected:
    virtual void Notify12vCritical();
    virtual void Notify12vRecovered();

  protected:
    char m_vin[18];
    char m_type[5];
    uint16_t m_charge_w;

    // --------------------------------------------------------------------------
    // Power statistics subsystem
    //  - implementation: vv_pwrmon.(h,cpp)
    //

    public:
      void PowerInit();
      void PowerUpdateMetrics();
      void PowerUpdate();
      void PowerReset();
      bool PowerIsModified();
      vehicle_command_t CommandPower(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

    protected:
      OvmsCommand *cmd_power;
      void PowerCollectData();

      speedpwr vx1_speedpwr[3];                 // speed power usage statistics
      UINT8 vx1_speed_state = 0;                    // speed state, one of:
      #define CAN_SPEED_CONST         0           // constant speed
      #define CAN_SPEED_ACCEL         1           // accelerating
      #define CAN_SPEED_DECEL         2           // decelerating

      // Speed resolution is ~11 (=0.11 kph) & ~100 ms
      #define CAN_SPEED_THRESHOLD     10

      // Speed change threshold for accel/decel detection:
      //  ...applied to vx1_accel_avg (4 samples):
      #define CAN_ACCEL_THRESHOLD     7

      signed int vx1_accel_avg = 0;                 // running average of speed delta

      signed int vx1_accel_min = 0;                 // min speed delta of trip
      signed int vx1_accel_max = 0;                 // max speed delta of trip

      unsigned int vx1_speed_distref = 0;           // distance reference for speed section


      levelpwr vx1_levelpwr[2];                 // level power usage statistics
      #define CAN_LEVEL_UP            0           // uphill
      #define CAN_LEVEL_DOWN          1           // downhill

      unsigned long vx1_level_odo = 0;              // level section odometer reference
      signed int vx1_level_alt = 0;                 // level section altitude reference
      unsigned long vx1_level_use = 0;              // level section use collector
      unsigned long vx1_level_rec = 0;              // level section rec collector
      #define CAN_LEVEL_MINSECTLEN    100         // min section length (in m)
      #define CAN_LEVEL_THRESHOLD     1           // level change threshold (grade percent)


    // --------------------------------------------------------------------------
    // Twizy battery monitoring subsystem
    //  - implementation: rt_battmon.(h,cpp)
    //

    public:
      void BatteryInit();
      bool BatteryLock(int maxwait_ms);
      void BatteryUnlock();
      void BatteryUpdateMetrics();
      void BatteryUpdate();
      void BatteryReset();
      void BatterySendDataUpdate(bool force = false);
      vehicle_command_t CommandBatt(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

    protected:
      void FormatPackData(int verbosity, OvmsWriter* writer, int pack);
      void FormatCellData(int verbosity, OvmsWriter* writer, int cell);
      void FormatBatteryStatus(int verbosity, OvmsWriter* writer, int pack);
      void FormatBatteryVolts(int verbosity, OvmsWriter* writer, bool show_deviations);
      void FormatBatteryTemps(int verbosity, OvmsWriter* writer, bool show_deviations);
      void BatteryCheckDeviations();

      #define BATT_PACKS      1
      #define BATT_CELLS      16                  // 14 on LiPo pack, 16 on LiFe pack
      //#define BATT_CELLS      40                  // 40 Cells on life pack Vectrix
      #define BATT_CMODS      8                   // 7 on LiPo pack, 8 on LiFe pack
      //#define BATT_CMODS      4                   // 4 BMS on LiFe Vectrix

      UINT8 batt_pack_count = 1;
      UINT8 batt_cmod_count = 7;      // Just for Twizy
      UINT8 batt_cell_count = 14;     // Just for Twizy

      OvmsCommand *cmd_batt;

      OvmsMetricFloat *m_batt_use_soc_min;
      OvmsMetricFloat *m_batt_use_soc_max;
      OvmsMetricFloat *m_batt_use_volt_min;
      OvmsMetricFloat *m_batt_use_volt_max;
      OvmsMetricFloat *m_batt_use_temp_min;
      OvmsMetricFloat *m_batt_use_temp_max;

      //OvmsMetricFloat *m_batt_max_drive_pwr;
      //OvmsMetricFloat *m_batt_max_recup_pwr;

      battery_pack vx1_batt[BATT_PACKS];
      battery_cmod vx1_cmod[BATT_CMODS];
      battery_cell vx1_cell[BATT_CELLS];

      // vx1_batt_sensors_state:
      //  A consistent state needs all 5 [6] battery sensor messages
      //  of one series (i.e. 554-6-7-E-F [700]) to be read.
      //  state=BATT_SENSORS_START begins a new fetch cycle.
      //  IncomingFrameCan1() will advance/reset states accordingly to incoming msgs.
      //  BatteryCheckDeviations() will not process the data until BATT_SENSORS_READY
      //  has been reached, after processing it will reset state to _START.
      volatile uint8_t vx1_batt_sensors_state;
      #define BATT_SENSORS_START     0  // start group fetch
      #define BATT_SENSORS_GOT556    3  // mask: lowest 2 bits (state)
      #define BATT_SENSORS_GOT554    4  // bit
      #define BATT_SENSORS_GOT557    8  // bit
      #define BATT_SENSORS_GOT55E   16  // bit
      #define BATT_SENSORS_GOT55F   32  // bit
      #define BATT_SENSORS_GOT700   64  // bit (only used for 16 cell batteries)
      #define BATT_SENSORS_GOTALL   ((batt_cell_count==14) ? 61 : 125)  // threshold: data complete
      #define BATT_SENSORS_READY    ((batt_cell_count==14) ? 63 : 127)  // value: group complete
      SemaphoreHandle_t m_batt_sensors = 0;
      bool m_batt_doreset = false;

  };

#endif //#ifndef __VEHICLE_VECTRIXVX1_H__
