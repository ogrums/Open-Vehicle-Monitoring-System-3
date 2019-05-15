/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
 ;    (C) 2019       Thomas Heuer
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
 ;
 ; Most of the CAN Messages are based on https://github.com/MyLab-odyssey/ED_BMSdiag
 */

#ifndef __VEHICLE_SMARTED_H__
#define __VEHICLE_SMARTED_H__

#include "vehicle.h"
#include "freertos/timers.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

using namespace std;


class OvmsVehicleSmartED : public OvmsVehicle
{
  public:
    OvmsVehicleSmartED();
    ~OvmsVehicleSmartED();

  public:
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    char m_vin[18];

  public:
    void WebInit();
    void WebDeInit();
    static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    void ConfigChanged(OvmsConfigParam* param);

  public:
    virtual vehicle_command_t CommandSetChargeCurrent(uint16_t limit);
    virtual vehicle_command_t CommandSetChargeTimer(bool timeron, uint32_t timerstart);
    virtual vehicle_command_t CommandClimateControl(bool enable);
    virtual vehicle_command_t CommandLock(const char* pin);
    virtual vehicle_command_t CommandUnlock(const char* pin);
    virtual vehicle_command_t CommandWakeup();
    virtual vehicle_command_t CommandHomelink(int button, int durationms=1000);
    virtual vehicle_command_t CommandActivateValet(const char* pin);
    virtual vehicle_command_t CommandDeactivateValet(const char* pin);
    virtual vehicle_command_t CommandStat(int verbosity, OvmsWriter* writer);
    
  protected:
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker10(uint32_t ticker);
    virtual void Ticker60(uint32_t ticker);
    void GetDashboardConfig(DashboardConfig& cfg);
    void vehicle_smarted_car_on(bool isOn);
    TimerHandle_t m_locking_timer;
    
    void HandleCharging();
    int  calcMinutesRemaining(float target, float charge_voltage, float charge_current);
    void HandleChargingStatus(bool status);
    void PollReply_BMS_BattVolts(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_BMS_BattTemp(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_BMS_ModuleTemp(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_NLG6_ChargerPN_HW(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_NLG6_ChargerVoltages(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_NLG6_ChargerAmps(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_NLG6_ChargerSelCurrent(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_NLG6_ChargerTemperatures(uint8_t reply_data[], uint16_t reply_len);
    void PollReply_LogDebug(const char* name, uint8_t reply_data[], uint16_t reply_len);
    
    OvmsMetricInt *mt_vehicle_time;         // vehicle time
    OvmsMetricInt *mt_trip_start;           // trip since start
    OvmsMetricInt *mt_trip_reset;           // trip since Reset
    OvmsMetricBool *mt_hv_active;           // HV active

  private:
    unsigned int m_candata_timer;
    unsigned int m_candata_poll;
    unsigned int m_egpio_timer;

  protected:
    int m_doorlock_port;                    // … MAX7317 output port number (3…9, default 9 = EGPIO_8)
    int m_doorunlock_port;                  // … MAX7317 output port number (3…9, default 8 = EGPIO_7)
    int m_ignition_port;                    // … MAX7317 output port number (3…9, default 7 = EGPIO_6)
    int m_range_ideal;                      // … Range Ideal (default 135 km)
    int m_egpio_timout;                     // … EGPIO Ignition Timout (default 5 min)
    bool m_soc_rsoc;                        // Display SOC=SOC or rSOC=SOC
    
    char NLG6_PN_HW[12] = "4519822221";     //!< Part number for NLG6 fast charging hardware
    bool NLG6present = false;               //!< Flag to show NLG6 detected in system
    float NLG6MainsAmps[3];                 //!< AC current of L1, L2, L3
    float NLG6MainsVoltage[3];              //!< AC voltage of L1, L2, L3
    float NLG6Amps_setpoint;                //!< AC charging current set by user in BC (Board Computer)
    float NLG6AmpsCableCode;                //!< Maximum current cable (resistor coded)
    float NLG6AmpsChargingpoint;            //!< Maxiumum current of chargingpoint
    float NLG6DC_Current;                   //!< DC current measured by charger
    float NLG6DC_HV;                        //!< DC HV measured by charger
    float NLG6LV;                           //!< 12V onboard voltage of Charger DC/DC
    float NLG6Temps[8];                     //!< internal temperatures in charger unit and heat exchanger
    float NLG6ReportedTemp;                 //!< mean temperature, reported by charger
    float NLG6SocketTemp;                   //!< temperature of mains socket charger
    float NLG6CoolingPlateTemp;             //!< temperature of cooling plate 
    char NLG6PN_HW[12];                     //!< Part number of base hardware (wo revisioning)
    
    #define DEFAULT_BATTERY_CAPACITY 17600
    #define MAX_POLL_DATA_LEN 238
    #define CELLCOUNT 93
    #define SE_CANDATA_TIMEOUT 10
    
};

#endif //#ifndef __VEHICLE_SMARTED_H__
