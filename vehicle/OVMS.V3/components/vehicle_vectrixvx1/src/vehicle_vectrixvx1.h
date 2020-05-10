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
#ifdef CONFIG_OVMS_COMP_WEBSERVER
 #include "ovms_webserver.h"
#endif

#include "vv_types.h"
#include "vv_battmon.h"

using namespace std;

class OvmsVehicleVectrixVX1: public OvmsVehicle
  {

  public:
    OvmsVehicleVectrixVX1();
    ~OvmsVehicleVectrixVX1();
    static OvmsVehicleVectrixVX1* GetInstance(OvmsWriter* writer=NULL);
    virtual const char* VehicleShortName();


  // --------------------------------------------------------------------------
  // Framework integration
  //

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);

  protected:
    static size_t m_modifier;
    OvmsMetricString *m_version;

  // --------------------------------------------------------------------------
  // General status
  //

  public:
    //vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);

  protected:
    // Car + charge status from CAN:
    #define CAN_STATUS_BRAKE_R      0x01        //  bit 0 = 0x01: 1 = Front/right brake presse
    #define CAN_STATUS_BRAKE_L      0x04        //  bit 2 = 0x04: 1 = Rear/Left brake presse
    #define CAN_STATUS_KILLSWITCHON 0x10        //  bit 4 = 0x10: 1 = Kill Switch On (0 = Off)
    #define CAN_STATUS_SEATTRUNK    0x20        //  bit 5 = 0x20: 1 = Seat Trunk Open (O = closed)
    #define CAN_STATUS_SIDESTAND    0x40        //  bit 6 = 0x40: 1 = Side Stand down
    #define CAN_STATUS_MODE_D       0x02        //  bit 1 = 0x02: 1 = Forward mode "D"
    #define CAN_STATUS_MODE_R       0xC0        //  bit 7&6 = 0xC0: 1 = Reverse Indicator "R"
    #define CAN_STATUS_REGEN_L      0x01        //  bit 0 = 0x01: 1 = Brake light when regen
    #define CAN_STATUS_MODE         0x06        //  mask
    #define CAN_STATUS_READY        0x0C        //  bit 1&0 = 0x03: 1 = "Ready" light = Kill Switch ON (Ignition)
    #define CAN_STATUS_GO           0x0C        //  bit 3&2 = 0x0C: 1 = "GO" light = Motor ON (Ignition)
    #define CAN_STATUS_KEYON        0x30        //  bit 5&4 = 0x30: 1 = Power Enable
    #define CAN_STATUS_CHARGING     0x1F        //  byte 7 = 0x1F: 0 != Charging
    #define CAN_STATUS_OFFLINE      0x40        //  bit 6 = 0x40: 1 = Switch-ON/-OFF phase / 0 = normal operation
    #define CAN_STATUS_ONLINE       0x80        //  bit 7 = 0x80: 1 = CAN-Bus online (test flag to detect offline)
    unsigned char vx1_status = CAN_STATUS_OFFLINE;


    int cfg_suffsoc = 0;                            // Configured sufficient SOC
    int cfg_suffrange = 0;                          // Configured sufficient range

    int vx1_chargeduration = 0;           // charge duration in seconds

    unsigned int vx1_range_est = 50;              // Vectrix estimated range in km

    unsigned int vx1_speed = 0;                   // current speed in 1/100 km/h
    unsigned int vx1_dist = 0;                    // cyclic distance counter in 1/10 m = 10 cm

    // Accelerator pedal & kickdown detection:

    volatile UINT8 vx1_accel_pedal;           // accelerator pedal (running avg for 2 samples = 200 ms)
    volatile UINT8 vx1_kickdown_level;        // kickdown detection & pedal max level
    UINT8 vx1_kickdown_hold;                  // kickdown hold countdown (1/10 seconds)
    #define VX1_KICKDOWN_HOLDTIME 50          // kickdown hold time (1/10 seconds)

  protected:
    unsigned int vx1_notifications = 0;
    void RequestNotify(unsigned int which);
    void DoNotify();





  // --------------------------------------------------------------------------
  // Webserver subsystem
  //  - implementation: vv_web.(h,cpp)
  //

  public:
    void WebInit();
    static void WebPowerMon(PageEntry_t& p, PageContext_t& c);
    //static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    //static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
    void WebDeInit();
    void GetDashboardConfig(DashboardConfig& cfg);

  protected:

    char m_vin[18];
    char m_type[5];
    uint16_t m_charge_w;


    // --------------------------------------------------------------------------
    // Twizy battery monitoring subsystem
    //  - implementation: rt_battmon.(h,cpp)
    //

    public:
      void BatteryInit();
      void BatteryReset();

  };

#endif //#ifndef __VEHICLE_VECTRIXVX1_H__
