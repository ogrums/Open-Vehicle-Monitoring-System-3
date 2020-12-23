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
static const char *TAG = "vehicle";

#include <stdio.h>
#include <algorithm>
#include <ovms_command.h>
#include <ovms_script.h>
#include <ovms_metrics.h>
#include <ovms_notify.h>
#include <metrics_standard.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_webserver.h>
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_peripherals.h>
#include <string_writer.h>
#include "vehicle.h"

#undef SQR
#define SQR(n) ((n)*(n))
#undef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))
#undef LIMIT_MIN
#define LIMIT_MIN(n,lim) ((n) < (lim) ? (lim) : (n))
#undef LIMIT_MAX
#define LIMIT_MAX(n,lim) ((n) > (lim) ? (lim) : (n))

#undef TRUNCPREC
#define TRUNCPREC(fval,prec) (trunc((fval) * pow(10,(prec))) / pow(10,(prec)))
#undef ROUNDPREC
#define ROUNDPREC(fval,prec) (round((fval) * pow(10,(prec))) / pow(10,(prec)))
#undef CEILPREC
#define CEILPREC(fval,prec)  (ceil((fval)  * pow(10,(prec))) / pow(10,(prec)))


OvmsVehicleFactory MyVehicleFactory __attribute__ ((init_priority (2000)));


OvmsVehicleFactory::OvmsVehicleFactory()
  {
  ESP_LOGI(TAG, "Initialising VEHICLE Factory (2000)");

  m_currentvehicle = NULL;
  m_currentvehicletype.clear();

  OvmsCommand* cmd_vehicle = MyCommandApp.RegisterCommand("vehicle","Vehicle framework");
  cmd_vehicle->RegisterCommand("module","Set (or clear) vehicle module",vehicle_module,"<type>",0,1,true,vehicle_validate);
  cmd_vehicle->RegisterCommand("list","Show list of available vehicle modules",vehicle_list);
  cmd_vehicle->RegisterCommand("status","Show vehicle module status",vehicle_status);

  MyCommandApp.RegisterCommand("wakeup","Wake up vehicle",vehicle_wakeup);
  MyCommandApp.RegisterCommand("homelink","Activate specified homelink button",vehicle_homelink,"<homelink> [<duration=100ms>]",1,2);
  OvmsCommand* cmd_climate = MyCommandApp.RegisterCommand("climatecontrol","(De)Activate Climate Control");
  cmd_climate->RegisterCommand("on","Activate Climate Control",vehicle_climatecontrol_on);
  cmd_climate->RegisterCommand("off","Deactivate Climate Control",vehicle_climatecontrol_off);
  MyCommandApp.RegisterCommand("lock","Lock vehicle",vehicle_lock,"<pin>",1,1);
  MyCommandApp.RegisterCommand("unlock","Unlock vehicle",vehicle_unlock,"<pin>",1,1);
  MyCommandApp.RegisterCommand("valet","Activate valet mode",vehicle_valet,"<pin>",1,1);
  MyCommandApp.RegisterCommand("unvalet","Deactivate valet mode",vehicle_unvalet,"<pin>",1,1);

  OvmsCommand* cmd_charge = MyCommandApp.RegisterCommand("charge","Charging framework");
  OvmsCommand* cmd_chargemode = cmd_charge->RegisterCommand("mode","Set vehicle charge mode");
  cmd_chargemode->RegisterCommand("standard","Set vehicle standard charge mode",vehicle_charge_mode);
  cmd_chargemode->RegisterCommand("storage","Set vehicle standard charge mode",vehicle_charge_mode);
  cmd_chargemode->RegisterCommand("range","Set vehicle standard charge mode",vehicle_charge_mode);
  cmd_chargemode->RegisterCommand("performance","Set vehicle standard charge mode",vehicle_charge_mode);
  cmd_charge->RegisterCommand("start","Start a vehicle charge",vehicle_charge_start);
  cmd_charge->RegisterCommand("stop","Stop a vehicle charge",vehicle_charge_stop);
  cmd_charge->RegisterCommand("current","Limit charge current",vehicle_charge_current,"<amps>",1,1);
  cmd_charge->RegisterCommand("cooldown","Start a vehicle cooldown",vehicle_charge_cooldown);

  MyCommandApp.RegisterCommand("stat","Show vehicle status",vehicle_stat);

  OvmsCommand* cmd_bms = MyCommandApp.RegisterCommand("bms","BMS framework", bms_status, "", 0, 0, false);
  cmd_bms->RegisterCommand("status","Show BMS status",bms_status);
  cmd_bms->RegisterCommand("reset","Reset BMS statistics",bms_reset);
  cmd_bms->RegisterCommand("alerts","Show BMS alerts",bms_alerts);

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  DuktapeObjectRegistration* dto = new DuktapeObjectRegistration("OvmsVehicle");
  dto->RegisterDuktapeFunction(DukOvmsVehicleType, 0, "Type");
  dto->RegisterDuktapeFunction(DukOvmsVehicleWakeup, 0, "Wakeup");
  dto->RegisterDuktapeFunction(DukOvmsVehicleHomelink, 2, "Homelink");
  dto->RegisterDuktapeFunction(DukOvmsVehicleClimateControl, 1, "ClimateControl");
  dto->RegisterDuktapeFunction(DukOvmsVehicleLock, 1, "Lock");
  dto->RegisterDuktapeFunction(DukOvmsVehicleUnlock, 1, "Unlock");
  dto->RegisterDuktapeFunction(DukOvmsVehicleValet, 1, "Valet");
  dto->RegisterDuktapeFunction(DukOvmsVehicleUnvalet, 1, "Unvalet");
  dto->RegisterDuktapeFunction(DukOvmsVehicleSetChargeMode, 1, "SetChargeMode");
  dto->RegisterDuktapeFunction(DukOvmsVehicleSetChargeCurrent, 1, "SetChargeCurrent");
  dto->RegisterDuktapeFunction(DukOvmsVehicleSetChargeTimer, 2, "SetChargeTimer");
  dto->RegisterDuktapeFunction(DukOvmsVehicleStopCharge, 0, "StopCharge");
  dto->RegisterDuktapeFunction(DukOvmsVehicleStartCharge, 0, "StartCharge");
  dto->RegisterDuktapeFunction(DukOvmsVehicleStartCooldown, 0, "StartCooldown");
  dto->RegisterDuktapeFunction(DukOvmsVehicleStopCooldown, 0, "StopCooldown");
  MyDuktape.RegisterDuktapeObject(dto);
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }

OvmsVehicleFactory::~OvmsVehicleFactory()
  {
  if (m_currentvehicle)
    {
    m_currentvehicle->m_ready = false;
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    m_currentvehicletype.clear();
    }
  }

OvmsVehicle* OvmsVehicleFactory::NewVehicle(const char* VehicleType)
  {
  OvmsVehicleFactory::map_vehicle_t::iterator iter = m_vmap.find(VehicleType);
  if (iter != m_vmap.end())
    {
    return iter->second.construct();
    }
  return NULL;
  }

void OvmsVehicleFactory::ClearVehicle()
  {
  if (m_currentvehicle)
    {
    m_currentvehicle->m_ready = false;
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    m_currentvehicletype.clear();
    StandardMetrics.ms_v_type->SetValue("");
    MyEvents.SignalEvent("vehicle.type.cleared", NULL);
    }
  }

void OvmsVehicleFactory::SetVehicle(const char* type)
  {
  if (m_currentvehicle)
    {
    m_currentvehicle->m_ready = false;
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    m_currentvehicletype.clear();
    MyEvents.SignalEvent("vehicle.type.cleared", NULL);
    }
  m_currentvehicle = NewVehicle(type);
  if (m_currentvehicle)
  {
  	m_currentvehicle->m_ready = true;
  }
  m_currentvehicletype = std::string(type);
  StandardMetrics.ms_v_type->SetValue(m_currentvehicle ? type : "");
  MyEvents.SignalEvent("vehicle.type.set", (void*)type, strlen(type)+1);
  }

void OvmsVehicleFactory::AutoInit()
  {
  std::string type = MyConfig.GetParamValue("auto", "vehicle.type");
  if (!type.empty())
    SetVehicle(type.c_str());
  }

OvmsVehicle* OvmsVehicleFactory::ActiveVehicle()
  {
  return m_currentvehicle;
  }

const char* OvmsVehicleFactory::ActiveVehicleType()
  {
  return m_currentvehicletype.c_str();
  }

const char* OvmsVehicleFactory::ActiveVehicleName()
  {
  map_vehicle_t::iterator it = m_vmap.find(m_currentvehicletype.c_str());
  if (it != m_vmap.end())
    return it->second.name;
  return "";
  }

const char* OvmsVehicleFactory::ActiveVehicleShortName()
  {
  return m_currentvehicle ? m_currentvehicle->VehicleShortName() : "";
  }

static void OvmsVehicleRxTask(void *pvParameters)
  {
  OvmsVehicle *me = (OvmsVehicle*)pvParameters;
  me->RxTask();
  }

OvmsVehicle::OvmsVehicle()
  {
  m_can1 = NULL;
  m_can2 = NULL;
  m_can3 = NULL;
  m_can4 = NULL;

  m_ticker = 0;
  m_12v_ticker = 0;
  m_chargestate_ticker = 0;
  m_idle_ticker = 0;
  m_registeredlistener = false;
  m_autonotifications = true;
  m_ready = false;

  m_poll_state = 0;
  m_poll_bus = NULL;
  m_poll_bus_default = NULL;
  m_poll_plist = NULL;
  m_poll_plcur = NULL;
  m_poll_ticker = 0;
  m_poll_moduleid_sent = 0;
  m_poll_moduleid_low = 0;
  m_poll_moduleid_high = 0;
  m_poll_type = 0;
  m_poll_pid = 0;
  m_poll_ml_remain = 0;
  m_poll_ml_offset = 0;
  m_poll_ml_frame = 0;
  m_poll_wait = 0;
  m_poll_sequence_max = 1;
  m_poll_sequence_cnt = 0;
  m_poll_fc_septime = 25;       // response default timing: 25 milliseconds

  m_bms_voltages = NULL;
  m_bms_vmins = NULL;
  m_bms_vmaxs = NULL;
  m_bms_vdevmaxs = NULL;
  m_bms_valerts = NULL;
  m_bms_valerts_new = 0;
  m_bms_has_voltages = false;

  m_bms_temperatures = NULL;
  m_bms_tmins = NULL;
  m_bms_tmaxs = NULL;
  m_bms_tdevmaxs = NULL;
  m_bms_talerts = NULL;
  m_bms_talerts_new = 0;
  m_bms_has_temperatures = false;

  m_bms_bitset_v.clear();
  m_bms_bitset_t.clear();
  m_bms_bitset_cv = 0;
  m_bms_bitset_ct = 0;
  m_bms_readings_v = 0;
  m_bms_readingspermodule_v = 0;
  m_bms_readings_t = 0;
  m_bms_readingspermodule_t = 0;

  m_bms_limit_tmin = -1000;
  m_bms_limit_tmax = 1000;
  m_bms_limit_vmin = -1000;
  m_bms_limit_vmax = 1000;

  m_bms_defthr_vwarn  = BMS_DEFTHR_VWARN;
  m_bms_defthr_valert = BMS_DEFTHR_VALERT;
  m_bms_defthr_twarn  = BMS_DEFTHR_TWARN;
  m_bms_defthr_talert = BMS_DEFTHR_TALERT;

  m_minsoc = 0;
  m_minsoc_triggered = 0;

  m_accel_refspeed = 0;
  m_accel_reftime = 0;
  m_accel_smoothing = 2.0;

  m_batpwr_smoothing = 2.0;
  m_batpwr_smoothed = 0;

  m_brakelight_enable = false;
  m_brakelight_on = 1.3;
  m_brakelight_off = 0.7;
  m_brakelight_port = 1;
  m_brakelight_start = 0;
  m_brakelight_basepwr = 0;
  m_brakelight_ignftbrk = false;

  m_rxqueue = xQueueCreate(CONFIG_OVMS_VEHICLE_CAN_RX_QUEUE_SIZE,sizeof(CAN_frame_t));
  xTaskCreatePinnedToCore(OvmsVehicleRxTask, "OVMS Vehicle",
    CONFIG_OVMS_VEHICLE_RXTASK_STACK, (void*)this, 10, &m_rxtask, CORE(1));

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&OvmsVehicle::VehicleTicker1, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.changed", std::bind(&OvmsVehicle::VehicleConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.mounted", std::bind(&OvmsVehicle::VehicleConfigChanged, this, _1, _2));
  VehicleConfigChanged("config.mounted", NULL);

  MyMetrics.RegisterListener(TAG, "*", std::bind(&OvmsVehicle::MetricModified, this, _1));
  }

OvmsVehicle::~OvmsVehicle()
  {
  if (m_can1) m_can1->SetPowerMode(Off);
  if (m_can2) m_can2->SetPowerMode(Off);
  if (m_can3) m_can3->SetPowerMode(Off);
  if (m_can4) m_can4->SetPowerMode(Off);

  if (m_bms_voltages != NULL)
    {
    delete [] m_bms_voltages;
    m_bms_voltages = NULL;
    }
  if (m_bms_vmins != NULL)
    {
    delete [] m_bms_vmins;
    m_bms_vmins = NULL;
    }
  if (m_bms_vmaxs != NULL)
    {
    delete [] m_bms_vmaxs;
    m_bms_vmaxs = NULL;
    }
  if (m_bms_vdevmaxs != NULL)
    {
    delete [] m_bms_vdevmaxs;
    m_bms_vdevmaxs = NULL;
    }
  if (m_bms_valerts != NULL)
    {
    delete [] m_bms_valerts;
    m_bms_valerts = NULL;
    }

  if (m_bms_temperatures != NULL)
    {
    delete [] m_bms_temperatures;
    m_bms_temperatures = NULL;
    }
  if (m_bms_tmins != NULL)
    {
    delete [] m_bms_tmins;
    m_bms_tmins = NULL;
    }
  if (m_bms_tmaxs != NULL)
    {
    delete [] m_bms_tmaxs;
    m_bms_tmaxs = NULL;
    }
  if (m_bms_tdevmaxs != NULL)
    {
    delete [] m_bms_tdevmaxs;
    m_bms_tdevmaxs = NULL;
    }
  if (m_bms_talerts != NULL)
    {
    delete [] m_bms_talerts;
    m_bms_talerts = NULL;
    }

  if (m_registeredlistener)
    {
    MyCan.DeregisterListener(m_rxqueue);
    m_registeredlistener = false;
    }

  vQueueDelete(m_rxqueue);
  vTaskDelete(m_rxtask);

  MyEvents.DeregisterEvent(TAG);
  MyMetrics.DeregisterListener(TAG);
  }

const char* OvmsVehicle::VehicleShortName()
  {
  return MyVehicleFactory.ActiveVehicleName();
  }

void OvmsVehicle::RxTask()
  {
  CAN_frame_t frame;

  while(1)
    {
    if (xQueueReceive(m_rxqueue, &frame, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      if (!m_ready)
        continue;
      if (m_poll_wait && frame.origin == m_poll_bus && m_poll_plist)
        {
        // This is a quick filter check to see if the frame is possibly intended for our poller.
        // The filter will be checked again in PollerReceive() after locking the mutex.
        // ESP_LOGI(TAG, "Poller Rx candidate ID=%03x (expecting %03x-%03x)",frame.MsgID,m_poll_moduleid_low,m_poll_moduleid_high);
        uint32_t msgid;
        if (m_poll_protocol == ISOTP_EXTADR)
          msgid = frame.MsgID << 8 | frame.data.u8[0];
        else
          msgid = frame.MsgID;
        if (msgid >= m_poll_moduleid_low && msgid <= m_poll_moduleid_high)
          {
          PollerReceive(&frame, msgid);
          }
        }
      if (m_can1 == frame.origin) IncomingFrameCan1(&frame);
      else if (m_can2 == frame.origin) IncomingFrameCan2(&frame);
      else if (m_can3 == frame.origin) IncomingFrameCan3(&frame);
      else if (m_can4 == frame.origin) IncomingFrameCan4(&frame);
      }
    }
  }

void OvmsVehicle::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicle::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicle::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicle::IncomingFrameCan4(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicle::Status(int verbosity, OvmsWriter* writer)
  {
  writer->printf("Vehicle module %s loaded and running\n", VehicleShortName());
  }

void OvmsVehicle::RegisterCanBus(int bus, CAN_mode_t mode, CAN_speed_t speed, dbcfile* dbcfile)
  {
  switch (bus)
    {
    case 1:
      m_can1 = (canbus*)MyPcpApp.FindDeviceByName("can1");
      m_can1->SetPowerMode(On);
      m_can1->Start(mode,speed,dbcfile);
      break;
    case 2:
      m_can2 = (canbus*)MyPcpApp.FindDeviceByName("can2");
      m_can2->SetPowerMode(On);
      m_can2->Start(mode,speed,dbcfile);
      break;
    case 3:
      m_can3 = (canbus*)MyPcpApp.FindDeviceByName("can3");
      m_can3->SetPowerMode(On);
      m_can3->Start(mode,speed,dbcfile);
      break;
    case 4:
      m_can4 = (canbus*)MyPcpApp.FindDeviceByName("can4");
      m_can4->SetPowerMode(On);
      m_can4->Start(mode,speed,dbcfile);
      break;
    default:
      break;
    }

  if (!m_registeredlistener)
    {
    m_registeredlistener = true;
    MyCan.RegisterListener(m_rxqueue);
    }
  }

bool OvmsVehicle::PinCheck(char* pin)
  {
  if (!MyConfig.IsDefined("password","pin")) return false;

  std::string vpin = MyConfig.GetParamValue("password","pin");
  return (strcmp(vpin.c_str(),pin)==0);
  }

void OvmsVehicle::VehicleTicker1(std::string event, void* data)
  {
  if (!m_ready)
    return;

  m_ticker++;

  PollerSend(true);

  Ticker1(m_ticker);
  if ((m_ticker % 10) == 0) Ticker10(m_ticker);
  if ((m_ticker % 60) == 0) Ticker60(m_ticker);
  if ((m_ticker % 300) == 0) Ticker300(m_ticker);
  if ((m_ticker % 600) == 0) Ticker600(m_ticker);
  if ((m_ticker % 3600) == 0) Ticker3600(m_ticker);

  if (StandardMetrics.ms_v_env_on->AsBool())
    {
    StandardMetrics.ms_v_env_parktime->SetValue(0);
    StandardMetrics.ms_v_env_drivetime->SetValue(StandardMetrics.ms_v_env_drivetime->AsInt() + 1);
    }
  else
    {
    StandardMetrics.ms_v_env_drivetime->SetValue(0);
    StandardMetrics.ms_v_env_parktime->SetValue(StandardMetrics.ms_v_env_parktime->AsInt() + 1);
    }

  if (StandardMetrics.ms_v_charge_inprogress->AsBool())
    StandardMetrics.ms_v_charge_time->SetValue(StandardMetrics.ms_v_charge_time->AsInt() + 1);
  else
    StandardMetrics.ms_v_charge_time->SetValue(0);

  if (m_chargestate_ticker > 0 && --m_chargestate_ticker == 0)
    NotifyChargeState();

  CalculateEfficiency();

  // 12V battery monitor:
  if (StandardMetrics.ms_v_env_charging12v->AsBool() == true)
    {
    // add two seconds calmdown per second charging, max 15 minutes:
    if (m_12v_ticker < 15*60)
      m_12v_ticker += 2;
    }
  else if (m_12v_ticker > 0)
    {
    --m_12v_ticker;
    if (m_12v_ticker == 0)
      {
      // take 12V reference voltage:
      StandardMetrics.ms_v_bat_12v_voltage_ref->SetValue(StandardMetrics.ms_v_bat_12v_voltage->AsFloat());
      }
    }

  if ((m_ticker % 60) == 0)
    {
    // check 12V voltage:
    float volt = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
    // …against the maximum of default and measured reference voltage, so alerts will also
    //  be triggered if the measured ref follows a degrading battery:
    float dref = MyConfig.GetParamValueFloat("vehicle", "12v.ref", 12.6);
    float vref = MAX(StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat(), dref);
    bool alert_on = StandardMetrics.ms_v_bat_12v_voltage_alert->AsBool();
    float alert_threshold = MyConfig.GetParamValueFloat("vehicle", "12v.alert", 1.6);
    if (!alert_on && volt > 0 && vref > 0 && vref-volt > alert_threshold)
      {
      StandardMetrics.ms_v_bat_12v_voltage_alert->SetValue(true);
      MyEvents.SignalEvent("vehicle.alert.12v.on", NULL);
      if (m_autonotifications) Notify12vCritical();
      }
    else if (alert_on && volt > 0 && vref > 0 && vref-volt < alert_threshold*0.6)
      {
      StandardMetrics.ms_v_bat_12v_voltage_alert->SetValue(false);
      MyEvents.SignalEvent("vehicle.alert.12v.off", NULL);
      if (m_autonotifications) Notify12vRecovered();
      }
    }

  if ((m_ticker % 10)==0)
    {
    // Check MINSOC
    int soc = (int)StandardMetrics.ms_v_bat_soc->AsFloat();
    m_minsoc = MyConfig.GetParamValueInt("vehicle", "minsoc", 0);
    if (m_minsoc <= 0)
      {
      m_minsoc_triggered = 0;
      }
    else if (soc >= m_minsoc+2)
      {
      m_minsoc_triggered = m_minsoc;
      }
    if ((m_minsoc_triggered > 0) && (soc <= m_minsoc_triggered))
      {
      // We have reached the minimum SOC level
      if (m_autonotifications) NotifyMinSocCritical();
      if (soc > 1)
        m_minsoc_triggered = soc - 1;
      else
        m_minsoc_triggered = 0;
      }
    }

  // BMS alerts:
  if (m_bms_valerts_new || m_bms_talerts_new)
    {
    ESP_LOGW(TAG, "BMS new alerts: %d voltages, %d temperatures", m_bms_valerts_new, m_bms_talerts_new);
    MyEvents.SignalEvent("vehicle.alert.bms", NULL);
    if (m_autonotifications && MyConfig.GetParamValueBool("vehicle", "bms.alerts.enabled", true))
      NotifyBmsAlerts();
    m_bms_valerts_new = 0;
    m_bms_talerts_new = 0;
    }

  // Idle alert:
  if (!StdMetrics.ms_v_env_awake->AsBool() || StdMetrics.ms_v_pos_speed->AsFloat() > 0)
    {
    m_idle_ticker = 15 * 60; // first alert after 15 minutes
    }
  else if (m_idle_ticker > 0 && --m_idle_ticker == 0)
    {
    NotifyVehicleIdling();
    m_idle_ticker = 60 * 60; // successive alerts every 60 minutes
    }
  } // VehicleTicker1()

void OvmsVehicle::Ticker1(uint32_t ticker)
  {
  }

void OvmsVehicle::Ticker10(uint32_t ticker)
  {
  }

void OvmsVehicle::Ticker60(uint32_t ticker)
  {
  }

void OvmsVehicle::Ticker300(uint32_t ticker)
  {
  }

void OvmsVehicle::Ticker600(uint32_t ticker)
  {
  }

void OvmsVehicle::Ticker3600(uint32_t ticker)
  {
  }

void OvmsVehicle::NotifyChargeStart()
  {
  StringWriter buf(200);
  CommandStat(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","charge.started",buf.c_str());
  }

void OvmsVehicle::NotifyHeatingStart()
  {
  StringWriter buf(200);
  CommandStat(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","heating.started",buf.c_str());
  }

void OvmsVehicle::NotifyChargeStopped()
  {
  StringWriter buf(200);
  CommandStat(COMMAND_RESULT_NORMAL, &buf);
  if (StdMetrics.ms_v_charge_substate->AsString() == "scheduledstop")
    MyNotify.NotifyString("info","charge.stopped",buf.c_str());
  else
    MyNotify.NotifyString("alert","charge.stopped",buf.c_str());
  }

void OvmsVehicle::NotifyChargeDone()
  {
  StringWriter buf(200);
  CommandStat(COMMAND_RESULT_NORMAL, &buf);
  MyNotify.NotifyString("info","charge.done",buf.c_str());
  }

void OvmsVehicle::NotifyValetEnabled()
  {
  MyNotify.NotifyString("info", "valet.enabled", "Valet mode enabled");
  }

void OvmsVehicle::NotifyValetDisabled()
  {
  MyNotify.NotifyString("info", "valet.disabled", "Valet mode disabled");
  }

void OvmsVehicle::NotifyValetHood()
  {
  MyNotify.NotifyString("alert", "valet.hood", "Vehicle hood opened while in valet mode");
  }

void OvmsVehicle::NotifyValetTrunk()
  {
  MyNotify.NotifyString("alert", "valet.trunk", "Vehicle trunk opened while in valet mode");
  }

void OvmsVehicle::NotifyAlarmSounding()
  {
  MyNotify.NotifyString("alert", "alarm.sounding", "Vehicle alarm is sounding");
  }

void OvmsVehicle::NotifyAlarmStopped()
  {
  MyNotify.NotifyString("alert", "alarm.stopped", "Vehicle alarm has stopped");
  }

void OvmsVehicle::Notify12vCritical()
  {
  float volt = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
  float dref = MyConfig.GetParamValueFloat("vehicle", "12v.ref", 12.6);
  float vref = MAX(StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat(), dref);

  MyNotify.NotifyStringf("alert", "batt.12v.alert", "12V Battery critical: %.1fV (ref=%.1fV)", volt, vref);
  }

void OvmsVehicle::Notify12vRecovered()
  {
  float volt = StandardMetrics.ms_v_bat_12v_voltage->AsFloat();
  float dref = MyConfig.GetParamValueFloat("vehicle", "12v.ref", 12.6);
  float vref = MAX(StandardMetrics.ms_v_bat_12v_voltage_ref->AsFloat(), dref);

  MyNotify.NotifyStringf("alert", "batt.12v.recovered", "12V Battery restored: %.1fV (ref=%.1fV)", volt, vref);
  }

void OvmsVehicle::NotifyMinSocCritical()
  {
  float soc = StandardMetrics.ms_v_bat_soc->AsFloat();

  MyNotify.NotifyStringf("alert", "batt.soc.alert", "Battery SOC critical: %.1f%% (alert<=%d%%)", soc, m_minsoc);
  }

void OvmsVehicle::NotifyVehicleIdling()
  {
  MyNotify.NotifyString("alert", "vehicle.idle", "Vehicle is idling / stopped turned on");
  }

void OvmsVehicle::NotifyBmsAlerts()
  {
  StringWriter buf(200);
  if (FormatBmsAlerts(COMMAND_RESULT_SMS, &buf, false))
    MyNotify.NotifyString("alert", "batt.bms.alert", buf.c_str());
  }

// Default efficiency calculation by speed & power per second, average smoothed over 5 seconds.
// Override if your vehicle provides more detail.
void OvmsVehicle::CalculateEfficiency()
  {
  float consumption = 0;
  if (StdMetrics.ms_v_pos_speed->AsFloat() >= 5)
    consumption = StdMetrics.ms_v_bat_power->AsFloat(0, Watts) / StdMetrics.ms_v_pos_speed->AsFloat();
  StdMetrics.ms_v_bat_consumption->SetValue((StdMetrics.ms_v_bat_consumption->AsFloat() * 4 + consumption) / 5);
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandSetChargeMode(vehicle_mode_t mode)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandSetChargeCurrent(uint16_t limit)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandStartCharge()
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandStopCharge()
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandSetChargeTimer(bool timeron, uint16_t timerstart)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandCooldown(bool cooldownon)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandClimateControl(bool climatecontrolon)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandWakeup()
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandLock(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandUnlock(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandActivateValet(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandDeactivateValet(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicle::CommandHomelink(int button, int durationms)
  {
  return NotImplemented;
  }

#ifdef CONFIG_OVMS_COMP_TPMS

bool OvmsVehicle::TPMSRead(std::vector<uint32_t> *tpms)
  {
  ESP_LOGE(TAG, "TPMS tyre IDs not implemented in this vehicle");
  return false;
  }

bool OvmsVehicle::TPMSWrite(std::vector<uint32_t> &tpms)
  {
  ESP_LOGE(TAG, "TPMS tyre IDs not implemented in this vehicle");
  return false;
  }

#endif // #ifdef CONFIG_OVMS_COMP_TPMS

/**
 * CommandStat: default implementation of vehicle status output
 */
OvmsVehicle::vehicle_command_t OvmsVehicle::CommandStat(int verbosity, OvmsWriter* writer)
  {
  metric_unit_t rangeUnit = (MyConfig.GetParamValue("vehicle", "units.distance") == "M") ? Miles : Kilometers;

  bool chargeport_open = StdMetrics.ms_v_door_chargeport->AsBool();
  if (chargeport_open)
    {
    std::string charge_mode = StdMetrics.ms_v_charge_mode->AsString();
    std::string charge_state = StdMetrics.ms_v_charge_state->AsString();
    bool show_details = !(charge_state == "done" || charge_state == "stopped");

    // Translate mode codes:
    if (charge_mode == "standard")
      charge_mode = "Standard";
    else if (charge_mode == "storage")
      charge_mode = "Storage";
    else if (charge_mode == "range")
      charge_mode = "Range";
    else if (charge_mode == "performance")
      charge_mode = "Performance";

    // Translate state codes:
    if (charge_state == "charging")
      charge_state = "Charging";
    else if (charge_state == "topoff")
      charge_state = "Topping off";
    else if (charge_state == "done")
      charge_state = "Charge Done";
    else if (charge_state == "preparing")
      charge_state = "Preparing";
    else if (charge_state == "heating")
      charge_state = "Charging, Heating";
    else if (charge_state == "stopped")
      charge_state = "Charge Stopped";

    writer->printf("%s - %s\n", charge_mode.c_str(), charge_state.c_str());

    if (show_details)
      {
      writer->printf("%s/%s\n",
        (char*) StdMetrics.ms_v_charge_voltage->AsUnitString("-", Native, 1).c_str(),
        (char*) StdMetrics.ms_v_charge_current->AsUnitString("-", Native, 1).c_str());

      int duration_full = StdMetrics.ms_v_charge_duration_full->AsInt();
      if (duration_full > 0)
        writer->printf("Full: %d mins\n", duration_full);

      int duration_soc = StdMetrics.ms_v_charge_duration_soc->AsInt();
      if (duration_soc > 0)
        writer->printf("%s: %d mins\n",
          (char*) StdMetrics.ms_v_charge_limit_soc->AsUnitString("SOC", Native, 0).c_str(),
          duration_soc);

      int duration_range = StdMetrics.ms_v_charge_duration_range->AsInt();
      if (duration_range > 0)
        writer->printf("%s: %d mins\n",
          (char*) StdMetrics.ms_v_charge_limit_range->AsUnitString("Range", rangeUnit, 0).c_str(),
          duration_range);
      }
    }
  else
    {
    writer->puts("Not charging");
    }

  writer->printf("SOC: %s\n", (char*) StdMetrics.ms_v_bat_soc->AsUnitString("-", Native, 1).c_str());

  const char* range_ideal = StdMetrics.ms_v_bat_range_ideal->AsUnitString("-", rangeUnit, 0).c_str();
  if (*range_ideal != '-')
    writer->printf("Ideal range: %s\n", range_ideal);

  const char* range_est = StdMetrics.ms_v_bat_range_est->AsUnitString("-", rangeUnit, 0).c_str();
  if (*range_est != '-')
    writer->printf("Est. range: %s\n", range_est);

  const char* odometer = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();
  if (*odometer != '-')
    writer->printf("ODO: %s\n", odometer);

  const char* cac = StdMetrics.ms_v_bat_cac->AsUnitString("-", Native, 1).c_str();
  if (*cac != '-')
    writer->printf("CAC: %s\n", cac);

  const char* soh = StdMetrics.ms_v_bat_soh->AsUnitString("-", Native, 0).c_str();
  if (*soh != '-')
    writer->printf("SOH: %s\n", soh);

  return Success;
  }

void OvmsVehicle::VehicleConfigChanged(std::string event, void* data)
  {
  OvmsConfigParam* param = (OvmsConfigParam*) data;

  // read vehicle framework config:
  if (!param || param->GetName() == "vehicle")
    {
    // acceleration calculation:
    m_accel_smoothing = MyConfig.GetParamValueFloat("vehicle", "accel.smoothing", 2.0);

    // brakelight battery power smoothing:
    m_batpwr_smoothing = MyConfig.GetParamValueFloat("vehicle", "batpwr.smoothing", 2.0);

    // brakelight control:
    if (m_brakelight_enable)
      {
      SetBrakelight(0);
      StdMetrics.ms_v_env_regenbrake->SetValue(false);
      }
    m_brakelight_enable = MyConfig.GetParamValueBool("vehicle", "brakelight.enable", false);
    m_brakelight_on = MyConfig.GetParamValueFloat("vehicle", "brakelight.on", 1.3);
    m_brakelight_off = MyConfig.GetParamValueFloat("vehicle", "brakelight.off", 0.7);
    m_brakelight_port = MyConfig.GetParamValueInt("vehicle", "brakelight.port", 1);
    m_brakelight_basepwr = MyConfig.GetParamValueFloat("vehicle", "brakelight.basepwr", 0);
    m_brakelight_ignftbrk = MyConfig.GetParamValueBool("vehicle", "brakelight.ignftbrk", false);
    m_brakelight_start = 0;
    }

  // read vehicle specific config:
  ConfigChanged(param);
  }

void OvmsVehicle::ConfigChanged(OvmsConfigParam* param)
  {
  }

void OvmsVehicle::MetricModified(OvmsMetric* metric)
  {
  if (metric == StandardMetrics.ms_v_env_on)
    {
    if (StandardMetrics.ms_v_env_on->AsBool())
      {
      MyEvents.SignalEvent("vehicle.on",NULL);
      NotifiedVehicleOn();
      }
    else
      {
      if (m_brakelight_enable && m_brakelight_start)
        {
        SetBrakelight(0);
        m_brakelight_start = 0;
        StdMetrics.ms_v_env_regenbrake->SetValue(false);
        }
      MyEvents.SignalEvent("vehicle.off",NULL);
      NotifiedVehicleOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_awake)
    {
    if (StandardMetrics.ms_v_env_awake->AsBool())
      {
      NotifiedVehicleAwake();
      MyEvents.SignalEvent("vehicle.awake",NULL);
      }
    else
      {
      MyEvents.SignalEvent("vehicle.asleep",NULL);
      NotifiedVehicleAsleep();
      }
    }
  else if (metric == StandardMetrics.ms_v_charge_inprogress)
    {
    if (StandardMetrics.ms_v_charge_inprogress->AsBool())
      {
      MyEvents.SignalEvent("vehicle.charge.start",NULL);
      NotifiedVehicleChargeStart();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.charge.stop",NULL);
      NotifiedVehicleChargeStop();
      }
    }
  else if (metric == StandardMetrics.ms_v_door_chargeport)
    {
    if (StandardMetrics.ms_v_door_chargeport->AsBool())
      {
      MyEvents.SignalEvent("vehicle.charge.prepare",NULL);
      NotifiedVehicleChargePrepare();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.charge.finish",NULL);
      NotifiedVehicleChargeFinish();
      }
    }
  else if (metric == StandardMetrics.ms_v_charge_pilot)
    {
    if (StandardMetrics.ms_v_charge_pilot->AsBool())
      {
      MyEvents.SignalEvent("vehicle.charge.pilot.on",NULL);
      NotifiedVehicleChargePilotOn();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.charge.pilot.off",NULL);
      NotifiedVehicleChargePilotOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_charging12v)
    {
    if (StandardMetrics.ms_v_env_charging12v->AsBool())
      {
      if (m_12v_ticker < 30)
        m_12v_ticker = 30; // min calmdown time
      MyEvents.SignalEvent("vehicle.charge.12v.start",NULL);
      NotifiedVehicleCharge12vStart();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.charge.12v.stop",NULL);
      NotifiedVehicleCharge12vStop();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_locked)
    {
    if (StandardMetrics.ms_v_env_locked->AsBool())
      {
      MyEvents.SignalEvent("vehicle.locked",NULL);
      NotifiedVehicleLocked();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.unlocked",NULL);
      NotifiedVehicleUnlocked();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_valet)
    {
    if (StandardMetrics.ms_v_env_valet->AsBool())
      {
      MyEvents.SignalEvent("vehicle.valet.on",NULL);
      if (m_autonotifications) NotifyValetEnabled();
      NotifiedVehicleValetOn();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.valet.off",NULL);
      if (m_autonotifications) NotifyValetDisabled();
      NotifiedVehicleValetOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_headlights)
    {
    if (StandardMetrics.ms_v_env_headlights->AsBool())
      {
      MyEvents.SignalEvent("vehicle.headlights.on",NULL);
      NotifiedVehicleHeadlightsOn();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.headlights.off",NULL);
      NotifiedVehicleHeadlightsOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_door_hood)
    {
    if (StandardMetrics.ms_v_door_hood->AsBool() &&
        StandardMetrics.ms_v_env_valet->AsBool())
      {
      if (m_autonotifications) NotifyValetHood();
      }
    }
  else if (metric == StandardMetrics.ms_v_door_trunk)
    {
    if (StandardMetrics.ms_v_door_trunk->AsBool() &&
        StandardMetrics.ms_v_env_valet->AsBool())
      {
      if (m_autonotifications) NotifyValetTrunk();
      }
    }
  else if (metric == StandardMetrics.ms_v_env_alarm)
    {
    if (StandardMetrics.ms_v_env_alarm->AsBool())
      {
      MyEvents.SignalEvent("vehicle.alarm.on",NULL);
      if (m_autonotifications) NotifyAlarmSounding();
      NotifiedVehicleAlarmOn();
      }
    else
      {
      MyEvents.SignalEvent("vehicle.alarm.off",NULL);
      if (m_autonotifications) NotifyAlarmStopped();
      NotifiedVehicleAlarmOff();
      }
    }
  else if (metric == StandardMetrics.ms_v_charge_mode)
    {
    const char* m = metric->AsString().c_str();
    MyEvents.SignalEvent("vehicle.charge.mode",(void*)m, strlen(m)+1);
    NotifiedVehicleChargeMode(m);
    }
  else if (metric == StandardMetrics.ms_v_charge_state)
    {
    const char* m = metric->AsString().c_str();
    MyEvents.SignalEvent("vehicle.charge.state",(void*)m, strlen(m)+1);
    if (strcmp(m,"done")==0)
      {
      StandardMetrics.ms_v_charge_duration_full->SetValue(0);
      StandardMetrics.ms_v_charge_duration_range->SetValue(0);
      StandardMetrics.ms_v_charge_duration_soc->SetValue(0);
      }
    if (m_autonotifications)
      {
      m_chargestate_ticker = GetNotifyChargeStateDelay(m);
      if (m_chargestate_ticker == 0)
        NotifyChargeState();
      }
    NotifiedVehicleChargeState(m);
    }
  else if (metric == StandardMetrics.ms_v_pos_acceleration)
    {
    if (m_brakelight_enable)
      CheckBrakelight();
    }
  else if (metric == StandardMetrics.ms_v_bat_power)
    {
    if (m_batpwr_smoothing > 0)
      m_batpwr_smoothed = (m_batpwr_smoothed + metric->AsFloat() * m_batpwr_smoothing) / (m_batpwr_smoothing + 1);
    else
      m_batpwr_smoothed = metric->AsFloat();
    }
  }

/**
 * CalculateAcceleration: derive acceleration / deceleration level from speed change
 * Note:
 *  IF you want to let the framework calculate acceleration, call this after your regular
 *  update to StdMetrics.ms_v_pos_speed. This is optional, you can set ms_v_pos_acceleration
 *  yourself if your vehicle provides this metric.
 */
void OvmsVehicle::CalculateAcceleration()
  {
  uint32_t now = esp_log_timestamp();
  if (now > m_accel_reftime)
    {
    float speed = ABS(StdMetrics.ms_v_pos_speed->AsFloat(0, Kph)) * 1000 / 3600;
    float accel = (speed - m_accel_refspeed) / (now - m_accel_reftime) * 1000;
    // smooth out road bumps & gear box backlash:
    if (m_accel_smoothing > 0)
      accel = (accel + StdMetrics.ms_v_pos_acceleration->AsFloat() * m_accel_smoothing) / (m_accel_smoothing + 1);
    StdMetrics.ms_v_pos_acceleration->SetValue(TRUNCPREC(accel, 3));
    m_accel_refspeed = speed;
    m_accel_reftime = now;
    }
  }

/**
 * CheckBrakelight: check for regenerative braking, control brakelight accordingly
 * Notes:
 *  a) This depends on a regular and frequent speed update with <= 100 ms period. If the vehicle
 *     delivers speed values at too large intervals, the trigger will still work but come
 *     too late (reducing/deactivating acceleration smoothing may help).
 *     If the vehicle raw speed is already smoothed, reducing acceleration smoothing will
 *     provide a faster trigger. Same applies for the battery power level.
 *  b) The battery power regen threshold is defined at -[brakelight.basepwr] for activation
 *     and +[brakelight.basepwr] for deactivation. The config default is 0 as that works
 *     on vehicles without the battery power metric.
 *  c) To reduce flicker the brake light has a minimum hold time of currently fixed 500 ms.
 *  d) Normal operation is "regen light XOR foot brake light", set [brakelight.ignftbrk]
 *     to true to disable this.
 * Override to customize.
 */
void OvmsVehicle::CheckBrakelight()
  {
  uint32_t now = esp_log_timestamp();
  float speed = ABS(StdMetrics.ms_v_pos_speed->AsFloat(0, Kph)) * 1000 / 3600;
  float accel = StdMetrics.ms_v_pos_acceleration->AsFloat();
  bool car_on = StdMetrics.ms_v_env_on->AsBool();
  bool footbrake = StdMetrics.ms_v_env_footbrake->AsFloat() > 0;
  const uint32_t holdtime = 500;

  // activate brake light?
  if (car_on && accel < -m_brakelight_on && speed >= 1 && m_batpwr_smoothed <= -m_brakelight_basepwr
    && (m_brakelight_ignftbrk || !footbrake))
    {
    if (!m_brakelight_start)
      {
      if (SetBrakelight(1))
        {
        ESP_LOGD(TAG, "brakelight on at speed=%.2f m/s, accel=%.2f m/s^2", speed, accel);
        m_brakelight_start = now;
        StdMetrics.ms_v_env_regenbrake->SetValue(true);
        }
      else
        ESP_LOGW(TAG, "can't activate brakelight");
      }
    else
      m_brakelight_start = now;
    }
  // deactivate brake light?
  else if (!car_on || accel >= -m_brakelight_off || speed < 1 || m_batpwr_smoothed > m_brakelight_basepwr
    || (!m_brakelight_ignftbrk && footbrake))
    {
    if (m_brakelight_start && now >= m_brakelight_start + holdtime)
      {
      if (SetBrakelight(0))
        {
        ESP_LOGD(TAG, "brakelight off at speed=%.2f m/s, accel=%.2f m/s^2", speed, accel);
        m_brakelight_start = 0;
        StdMetrics.ms_v_env_regenbrake->SetValue(false);
        }
      else
        ESP_LOGW(TAG, "can't deactivate brakelight");
      }
    }
  }

/**
 * SetBrakelight: hardware brake light control method
 * Override for custom control, e.g. CAN.
 */
bool OvmsVehicle::SetBrakelight(int on)
  {
#ifdef CONFIG_OVMS_COMP_MAX7317
  // port 2 = SN65 for esp32can
  if (m_brakelight_port == 1 || (m_brakelight_port >= 3 && m_brakelight_port <= 9))
    {
    MyPeripherals->m_max7317->Output(m_brakelight_port, on);
    return true;
    }
  else
    {
    ESP_LOGE(TAG, "SetBrakelight: invalid port configuration (valid: 1, 3..9)");
    return false;
    }
#else // CONFIG_OVMS_COMP_MAX7317
  ESP_LOGE(TAG, "SetBrakelight: OVMS_COMP_MAX7317 missing");
  return false;
#endif // CONFIG_OVMS_COMP_MAX7317
  }

void OvmsVehicle::NotifyChargeState()
  {
  const char* m = StandardMetrics.ms_v_charge_state->AsString().c_str();
  if (strcmp(m,"done")==0)
    NotifyChargeDone();
  else if (strcmp(m,"stopped")==0)
    NotifyChargeStopped();
  else if (strcmp(m,"charging")==0)
    NotifyChargeStart();
  else if (strcmp(m,"topoff")==0)
    NotifyChargeStart();
  else if (strcmp(m,"heating")==0)
    NotifyHeatingStart();
  }

OvmsVehicle::vehicle_mode_t OvmsVehicle::VehicleModeKey(const std::string code)
  {
  vehicle_mode_t key;
  if      (code == "standard")      key = Standard;
  else if (code == "storage")       key = Storage;
  else if (code == "range")         key = Range;
  else if (code == "performance")   key = Performance;
  else key = Standard;
  return key;
  }


/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicle::SetFeature(int key, const char *value)
  {
  switch (key)
    {
    case 8:
      MyConfig.SetParamValue("vehicle", "stream", value);
      return true;
    case 9:
      MyConfig.SetParamValue("vehicle", "minsoc", value);
      return true;
    case 14:
      MyConfig.SetParamValue("vehicle", "carbits", value);
      return true;
    case 15:
      MyConfig.SetParamValue("vehicle", "canwrite", value);
      return true;
    default:
      return false;
    }
  }

/**
 * GetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
const std::string OvmsVehicle::GetFeature(int key)
  {
  switch (key)
    {
    case 8:
      return MyConfig.GetParamValue("vehicle", "stream", "0");
    case 9:
      return MyConfig.GetParamValue("vehicle", "minsoc", "0");
    case 14:
      return MyConfig.GetParamValue("vehicle", "carbits", "0");
    case 15:
      return MyConfig.GetParamValue("vehicle", "canwrite", "0");
    default:
      return "0";
    }
  }

/**
 * ProcessMsgCommand: V2 compatibility protocol message command processing
 *  result: optional payload or message to return to the caller with the command response
 */
OvmsVehicle::vehicle_command_t OvmsVehicle::ProcessMsgCommand(std::string &result, int command, const char* args)
  {
  return NotImplemented;
  }


#ifdef CONFIG_OVMS_COMP_WEBSERVER
/**
 * GetDashboardConfig: template / default configuration
 *  (override with vehicle specific configuration)
 *  see https://api.highcharts.com/highcharts/yAxis for details on options
 */
void OvmsVehicle::GetDashboardConfig(DashboardConfig& cfg)
  {
  cfg.gaugeset1 =
    "yAxis: [{"
      // Speed:
      "min: 0, max: 200,"
      "plotBands: ["
        "{ from: 0, to: 120, className: 'green-band' },"
        "{ from: 120, to: 160, className: 'yellow-band' },"
        "{ from: 160, to: 200, className: 'red-band' }]"
    "},{"
      // Voltage:
      "min: 310, max: 410,"
      "plotBands: ["
        "{ from: 310, to: 325, className: 'red-band' },"
        "{ from: 325, to: 340, className: 'yellow-band' },"
        "{ from: 340, to: 410, className: 'green-band' }]"
    "},{"
      // SOC:
      "min: 0, max: 100,"
      "plotBands: ["
        "{ from: 0, to: 12.5, className: 'red-band' },"
        "{ from: 12.5, to: 25, className: 'yellow-band' },"
        "{ from: 25, to: 100, className: 'green-band' }]"
    "},{"
      // Efficiency:
      "min: 0, max: 400,"
      "plotBands: ["
        "{ from: 0, to: 200, className: 'green-band' },"
        "{ from: 200, to: 300, className: 'yellow-band' },"
        "{ from: 300, to: 400, className: 'red-band' }]"
    "},{"
      // Power:
      "min: -50, max: 200,"
      "plotBands: ["
        "{ from: -50, to: 0, className: 'violet-band' },"
        "{ from: 0, to: 100, className: 'green-band' },"
        "{ from: 100, to: 150, className: 'yellow-band' },"
        "{ from: 150, to: 200, className: 'red-band' }]"
    "},{"
      // Charger temperature:
      "min: 20, max: 80, tickInterval: 20,"
      "plotBands: ["
        "{ from: 20, to: 65, className: 'normal-band border' },"
        "{ from: 65, to: 80, className: 'red-band border' }]"
    "},{"
      // Battery temperature:
      "min: -15, max: 65, tickInterval: 25,"
      "plotBands: ["
        "{ from: -15, to: 0, className: 'red-band border' },"
        "{ from: 0, to: 50, className: 'normal-band border' },"
        "{ from: 50, to: 65, className: 'red-band border' }]"
    "},{"
      // Inverter temperature:
      "min: 20, max: 80, tickInterval: 20,"
      "plotBands: ["
        "{ from: 20, to: 70, className: 'normal-band border' },"
        "{ from: 70, to: 80, className: 'red-band border' }]"
    "},{"
      // Motor temperature:
      "min: 50, max: 125, tickInterval: 25,"
      "plotBands: ["
        "{ from: 50, to: 110, className: 'normal-band border' },"
        "{ from: 110, to: 125, className: 'red-band border' }]"
    "}]";
  }
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
