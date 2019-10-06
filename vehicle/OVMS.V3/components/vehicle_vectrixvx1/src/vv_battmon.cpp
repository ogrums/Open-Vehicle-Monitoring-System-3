/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy battery monitor
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

#include <math.h>

#include "vv_battmon.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "string_writer.h"

#include "vehicle_vectrixvx1.h"


// Battery cell/cmod deviation alert thresholds:
#define BATT_DEV_TEMP_ALERT         3       // = 3 °C
#define BATT_DEV_VOLT_ALERT         6       // = 30 mV

// ...thresholds for overall stddev:
#define BATT_STDDEV_TEMP_WATCH      2       // = 2 °C
#define BATT_STDDEV_TEMP_ALERT      3       // = 3 °C
#define BATT_STDDEV_VOLT_WATCH      3       // = 15 mV
#define BATT_STDDEV_VOLT_ALERT      5       // = 25 mV

// watch/alert flags for overall stddev:
#define BATT_STDDEV_TEMP_FLAG       31  // bit #31
#define BATT_STDDEV_VOLT_FLAG       31  // bit #31


/**
 * vehicle_vx1_batt: command wrapper for CommandBatt
 */
void vehicle_vx1_batt(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleVectrixVX1* vx1 = (OvmsVehicleVectrixVX1*) MyVehicleFactory.ActiveVehicle();
  string type = StdMetrics.ms_v_type->AsString();

  if (!vx1 || type != "VV")
  {
    writer->puts("Error: VX1 vehicle module not selected");
    return;
  }

  vx1->CommandBatt(verbosity, writer, cmd, argc, argv);
}


/**
 * BatteryInit:
 */
void OvmsVehicleVectrixVX1::BatteryInit()
{
  ESP_LOGI(TAG, "battmon subsystem init");

  vx1_batt_sensors_state = BATT_SENSORS_START;
  m_batt_doreset = false;

  // init sensor lock:

  m_batt_sensors = xSemaphoreCreateBinary();
  xSemaphoreGive(m_batt_sensors);

  // init metrics

  m_batt_use_soc_min = MyMetrics.InitFloat("xvv.b.u.soc.min", SM_STALE_HIGH, (float) vx1_soc_min / 100, Percentage);
  m_batt_use_soc_max = MyMetrics.InitFloat("xvv.b.u.soc.max", SM_STALE_HIGH, (float) vx1_soc_max / 100, Percentage);
  m_batt_use_volt_min = MyMetrics.InitFloat("xvv.b.u.volt.min", SM_STALE_HIGH, 0, Volts);
  m_batt_use_volt_max = MyMetrics.InitFloat("xvv.b.u.volt.max", SM_STALE_HIGH, 0, Volts);
  m_batt_use_temp_min = MyMetrics.InitFloat("xvv.b.u.temp.min", SM_STALE_HIGH, 0, Celcius);
  m_batt_use_temp_max = MyMetrics.InitFloat("xvv.b.u.temp.max", SM_STALE_HIGH, 0, Celcius);

  // BMS configuration:
  //    Note: layout currently fixed to 2 voltages + 1 temperature per module,
  //          this may need refinement for custom batteries
  BmsSetCellArrangementVoltage(batt_cell_count, 2);
  BmsSetCellArrangementTemperature(batt_cmod_count, 1);
  BmsSetCellLimitsVoltage(2.0, 5.0);
  BmsSetCellLimitsTemperature(-39, 200);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);

  // init commands

  cmd_batt = cmd_xvv->RegisterCommand("batt", "Battery monitor");
  {
    cmd_batt->RegisterCommand("reset", "Reset alerts & watches", vehicle_vx1_batt);
    cmd_batt->RegisterCommand("status", "Status report", vehicle_vx1_batt, "[<pack>]", 0, 1);
    cmd_batt->RegisterCommand("volt", "Show voltages", vehicle_vx1_batt);
    cmd_batt->RegisterCommand("vdev", "Show voltage deviations", vehicle_vx1_batt);
    cmd_batt->RegisterCommand("temp", "Show temperatures", vehicle_vx1_batt);
    cmd_batt->RegisterCommand("tdev", "Show temperature deviations", vehicle_vx1_batt);
    cmd_batt->RegisterCommand("data-pack", "Output pack record", vehicle_vx1_batt, "[<pack>]", 0, 1);
    cmd_batt->RegisterCommand("data-cell", "Output cell record", vehicle_vx1_batt, "<cell>", 1, 1);
  }
}


bool OvmsVehicleVectrixVX1::BatteryLock(int maxwait_ms)
{
  if (m_batt_sensors && xSemaphoreTake(m_batt_sensors, pdMS_TO_TICKS(maxwait_ms)) == pdTRUE)
    return true;
  return false;
}

void OvmsVehicleVectrixVX1::BatteryUnlock()
{
  if (m_batt_sensors)
    xSemaphoreGive(m_batt_sensors);
}


/**
 * BatteryUpdateMetrics: publish internal state to metrics
 */
void OvmsVehicleVectrixVX1::BatteryUpdateMetrics()
{
  *StdMetrics.ms_v_bat_temp = (float) vx1_batt[0].temp_act - 40;
  *StdMetrics.ms_v_bat_voltage = (float) vx1_batt[0].volt_act / 10;

  *m_batt_use_volt_min = (float) vx1_batt[0].volt_min / 10;
  *m_batt_use_volt_max = (float) vx1_batt[0].volt_max / 10;

  *m_batt_use_temp_min = (float) vx1_batt[0].temp_min - 40;
  *m_batt_use_temp_max = (float) vx1_batt[0].temp_max - 40;

  float vmin = MyConfig.GetParamValueFloat("xvv", "cell_volt_min", 3.165);
  float vmax = MyConfig.GetParamValueFloat("xvv", "cell_volt_max", 4.140);
  *StdMetrics.ms_v_bat_pack_level_min = (float) TRUNCPREC((((float)vx1_batt[0].cell_volt_min/200)-vmin) / (vmax-vmin) * 100, 3);
  *StdMetrics.ms_v_bat_pack_level_max = (float) TRUNCPREC((((float)vx1_batt[0].cell_volt_max/200)-vmin) / (vmax-vmin) * 100, 3);
  *StdMetrics.ms_v_bat_pack_level_avg = (float) TRUNCPREC(((vx1_batt[0].cell_volt_avg/200)-vmin) / (vmax-vmin) * 100, 3);
  *StdMetrics.ms_v_bat_pack_level_stddev = (float) TRUNCPREC((vx1_batt[0].cell_volt_stddev/200) / (vmax-vmin) * 100, 3);

  int i;
  for (i = 0; i < batt_cmod_count; i++)
    BmsSetCellTemperature(i, (float) vx1_cmod[i].temp_act - 40);
  for (i = 0; i < batt_cell_count; i++)
    BmsSetCellVoltage(i, (float) vx1_cell[i].volt_act / 200);
}


/**
 * BatteryUpdate:
 *  - executed in CAN RX task when sensor data is complete
 *  - commit sensor RX buffers
 *  - calculate voltage & temperature deviations
 *  - publish internal state to metrics
 */
void OvmsVehicleVectrixVX1::BatteryUpdate()
{
  // reset sensor fetch cycle:
  vx1_batt_sensors_state = BATT_SENSORS_START;

  // try to lock, else drop this update:
  if (!BatteryLock(0)) {
    ESP_LOGV(TAG, "BatteryUpdate: no lock, update dropped");
    return;
  }

  // copy RX buffers:
  for (battery_cell &cell : vx1_cell)
    cell.volt_act = cell.volt_new;
  for (battery_cmod &cmod : vx1_cmod)
    cmod.temp_act = cmod.temp_new;
  for (battery_pack &pack : vx1_batt)
    pack.volt_act = pack.volt_new;

  // reset min/max if due:
  if (m_batt_doreset) {
    BatteryReset();
    m_batt_doreset = false;
  }

  // calculate voltage & temperature deviations:
  BatteryCheckDeviations();

  // publish internal state to metrics:
  BatteryUpdateMetrics();

  // done, start next fetch cycle:
  BatteryUnlock();
}


/**
 * BatteryReset: reset deviations, alerts & watches
 */
void OvmsVehicleVectrixVX1::BatteryReset()
{
  ESP_LOGD(TAG, "battmon reset");

  for (battery_cell &cell : vx1_cell)
  {
    cell.volt_max = cell.volt_act;
    cell.volt_min = cell.volt_act;
    cell.volt_maxdev = 0;
  }

  for (battery_cmod &cmod : vx1_cmod)
  {
    cmod.temp_max = cmod.temp_act;
    cmod.temp_min = cmod.temp_act;
    cmod.temp_maxdev = 0;
  }

  for (battery_pack &pack : vx1_batt)
  {
    pack.volt_min = pack.volt_act;
    pack.volt_max = pack.volt_act;
    pack.cell_volt_stddev_max = 0;
    pack.temp_min = pack.temp_act;
    pack.temp_max = pack.temp_act;
    pack.cmod_temp_stddev_max = 0;
    pack.temp_watches = 0;
    pack.temp_alerts = 0;
    pack.volt_watches = 0;
    pack.volt_alerts = 0;
    pack.last_temp_alerts = 0;
    pack.last_volt_alerts = 0;
  }

  BmsResetCellStats();
  BatteryUpdateMetrics();
}


/**
 * BatteryCheckDeviations:
 *  - calculate voltage & temperature deviations
 *  - set watch/alert flags accordingly
 */

void OvmsVehicleVectrixVX1::BatteryCheckDeviations(void)
{
  int i;
  int stddev, absdev;
  int dev;
  double sd, m;
  long sum, sqrsum;
  int min, max;


  // *********** Temperatures: ************

  // build mean value & standard deviation:
  sum = 0;
  sqrsum = 0;
  min = 240;
  max = 0;

  for (i = 0; i < batt_cmod_count; i++)
  {
    // Validate:
    if ((vx1_cmod[i].temp_act == 0) || (vx1_cmod[i].temp_act >= 0x0f0))
      break;

    // Remember cell min/max:
    if ((vx1_cmod[i].temp_min == 0) || (vx1_cmod[i].temp_act < vx1_cmod[i].temp_min))
      vx1_cmod[i].temp_min = vx1_cmod[i].temp_act;
    if ((vx1_cmod[i].temp_max == 0) || (vx1_cmod[i].temp_act > vx1_cmod[i].temp_max))
      vx1_cmod[i].temp_max = vx1_cmod[i].temp_act;

    // Set pack min/max:
    if (vx1_cmod[i].temp_act < min)
      min = vx1_cmod[i].temp_act;
    if (vx1_cmod[i].temp_act > max)
      max = vx1_cmod[i].temp_act;

    // build sums:
    sum += vx1_cmod[i].temp_act;
    sqrsum += SQR(vx1_cmod[i].temp_act);
  }

  if (i == batt_cmod_count)
  {
    // All values valid, process:

    m = (double) sum / batt_cmod_count;

    vx1_batt[0].temp_act = m;

    // Battery pack usage cycle min/max:
    if ((vx1_batt[0].temp_min == 0) || (vx1_batt[0].temp_act < vx1_batt[0].temp_min))
      vx1_batt[0].temp_min = vx1_batt[0].temp_act;
    if ((vx1_batt[0].temp_max == 0) || (vx1_batt[0].temp_act > vx1_batt[0].temp_max))
      vx1_batt[0].temp_max = vx1_batt[0].temp_act;

    sd = sqrt( ((double)sqrsum/batt_cmod_count) - SQR((double)sum/batt_cmod_count) );
    stddev = sd + 0.5;
    if (stddev == 0)
      stddev = 1; // not enough precision to allow stddev 0

    vx1_batt[0].cmod_temp_min = min;
    vx1_batt[0].cmod_temp_max = max;
    vx1_batt[0].cmod_temp_avg = m;
    vx1_batt[0].cmod_temp_stddev = sd;

    // check max stddev:
    if (stddev > vx1_batt[0].cmod_temp_stddev_max)
    {
      vx1_batt[0].cmod_temp_stddev_max = stddev;
      vx1_batt[0].temp_alerts.reset();
      vx1_batt[0].temp_watches.reset();

      // switch to overall stddev alert mode?
      if (stddev >= BATT_STDDEV_TEMP_ALERT)
        vx1_batt[0].temp_alerts.set(BATT_STDDEV_TEMP_FLAG);
      else if (stddev >= BATT_STDDEV_TEMP_WATCH)
        vx1_batt[0].temp_watches.set(BATT_STDDEV_TEMP_FLAG);
    }

    // check cmod deviations:
    for (i = 0; i < batt_cmod_count; i++)
    {
      // deviation:
      dev = (vx1_cmod[i].temp_act - m)
              + ((vx1_cmod[i].temp_act >= m) ? 0.5 : -0.5);
      absdev = ABS(dev);

      // Set watch/alert flags:
      // (applying overall thresholds only in stddev alert mode)
      if ((vx1_batt[0].temp_alerts[BATT_STDDEV_TEMP_FLAG]) && (absdev >= BATT_STDDEV_TEMP_ALERT))
        vx1_batt[0].temp_alerts.set(i);
      else if (absdev >= BATT_DEV_TEMP_ALERT)
        vx1_batt[0].temp_alerts.set(i);
      else if ((vx1_batt[0].temp_watches[BATT_STDDEV_TEMP_FLAG]) && (absdev >= BATT_STDDEV_TEMP_WATCH))
        vx1_batt[0].temp_watches.set(i);
      else if (absdev > stddev)
        vx1_batt[0].temp_watches.set(i);

      // Remember max deviation:
      if (absdev > ABS(vx1_cmod[i].temp_maxdev))
        vx1_cmod[i].temp_maxdev = dev;
    }

  } // if( i == batt_cmod_count )


  // ********** Voltages: ************

  // Battery pack usage cycle min/max:

  if ((vx1_batt[0].volt_min == 0) || (vx1_batt[0].volt_act < vx1_batt[0].volt_min))
    vx1_batt[0].volt_min = vx1_batt[0].volt_act;
  if ((vx1_batt[0].volt_max == 0) || (vx1_batt[0].volt_act > vx1_batt[0].volt_max))
    vx1_batt[0].volt_max = vx1_batt[0].volt_act;

  // Cells: build mean value & standard deviation:
  sum = 0;
  sqrsum = 0;
  min = 2000;
  max = 0;

  for (i = 0; i < batt_cell_count; i++)
  {
    // Validate:
    if ((vx1_cell[i].volt_act == 0) || (vx1_cell[i].volt_act >= 0x0f00))
      break;

    // Remember cell min/max:
    if ((vx1_cell[i].volt_min == 0) || (vx1_cell[i].volt_act < vx1_cell[i].volt_min))
      vx1_cell[i].volt_min = vx1_cell[i].volt_act;
    if ((vx1_cell[i].volt_max == 0) || (vx1_cell[i].volt_act > vx1_cell[i].volt_max))
      vx1_cell[i].volt_max = vx1_cell[i].volt_act;

    // Set pack min/max:
    if (vx1_cell[i].volt_act < min)
      min = vx1_cell[i].volt_act;
    if (vx1_cell[i].volt_act > max)
      max = vx1_cell[i].volt_act;

    // build sums:
    sum += vx1_cell[i].volt_act;
    sqrsum += SQR(vx1_cell[i].volt_act);
  }

  if (i == batt_cell_count)
  {
    // All values valid, process:

    m = (double) sum / batt_cell_count;

    sd = sqrt( ((double)sqrsum/batt_cell_count) - SQR((double)sum/batt_cell_count) );
    stddev = sd + 0.5;
    if (stddev == 0)
      stddev = 1; // not enough precision to allow stddev 0

    vx1_batt[0].cell_volt_min = min;
    vx1_batt[0].cell_volt_max = max;
    vx1_batt[0].cell_volt_avg = m;
    vx1_batt[0].cell_volt_stddev = sd;

    // check max stddev:
    if (stddev > vx1_batt[0].cell_volt_stddev_max)
    {
      vx1_batt[0].cell_volt_stddev_max = stddev;
      vx1_batt[0].volt_alerts.reset();
      vx1_batt[0].volt_watches.reset();

      // switch to overall stddev alert mode?
      if (stddev >= BATT_STDDEV_VOLT_ALERT)
        vx1_batt[0].volt_alerts.set(BATT_STDDEV_VOLT_FLAG);
      else if (stddev >= BATT_STDDEV_VOLT_WATCH)
        vx1_batt[0].volt_watches.set(BATT_STDDEV_VOLT_FLAG);
    }

    // check cell deviations:
    for (i = 0; i < batt_cell_count; i++)
    {
      // deviation:
      dev = (vx1_cell[i].volt_act - m)
              + ((vx1_cell[i].volt_act >= m) ? 0.5 : -0.5);
      absdev = ABS(dev);

      // Set watch/alert flags:
      // (applying overall thresholds only in stddev alert mode)
      if ((vx1_batt[0].volt_alerts[BATT_STDDEV_VOLT_FLAG]) && (absdev >= BATT_STDDEV_VOLT_ALERT))
        vx1_batt[0].volt_alerts.set(i);
      else if (absdev >= BATT_DEV_VOLT_ALERT)
        vx1_batt[0].volt_alerts.set(i);
      else if ((vx1_batt[0].volt_watches[BATT_STDDEV_VOLT_FLAG]) && (absdev >= BATT_STDDEV_VOLT_WATCH))
        vx1_batt[0].volt_watches.set(i);
      else if (absdev > stddev)
        vx1_batt[0].volt_watches.set(i);

      // Remember max deviation:
      if (absdev > ABS(vx1_cell[i].volt_maxdev))
        vx1_cell[i].volt_maxdev = dev;
    }

  } // if( i == batt_cell_count )


  // Battery monitor update/alert:
  if ((vx1_batt[0].volt_alerts != vx1_batt[0].last_volt_alerts)
    || (vx1_batt[0].temp_alerts != vx1_batt[0].last_temp_alerts))
  {
    RequestNotify(SEND_BatteryAlert | SEND_BatteryStats);
    vx1_batt[0].last_volt_alerts = vx1_batt[0].volt_alerts;
    vx1_batt[0].last_temp_alerts = vx1_batt[0].temp_alerts;
  }
}


/**
 * CommandBatt: batt reset|status|volt|vdev|temp|tdev|data-pack|data-cell
 */
OvmsVehicleVectrixVX1::vehicle_command_t OvmsVehicleVectrixVX1::CommandBatt(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  const char *subcmd = cmd->GetName();

  ESP_LOGV(TAG, "command batt %s, verbosity=%d", subcmd, verbosity);

  if (!BatteryLock(100))
  {
    writer->puts("ERROR: can't lock battery state, please retry");
    return Fail;
  }

  OvmsVehicleVectrixVX1::vehicle_command_t res = Success;

  if (strcmp(subcmd, "reset") == 0)
  {
    BatteryReset();
    writer->puts("Battery monitor reset.");
  }

  else if (strcmp(subcmd, "status") == 0)
  {
    int pack = (argc > 0) ? atoi(argv[0]) : 1;
    if (pack < 1 || pack > batt_pack_count) {
      writer->printf("ERROR: pack number out of range [1-%d]\n", batt_pack_count);
      res = Fail;
    } else {
      FormatBatteryStatus(verbosity, writer, pack-1);
    }
  }

  else if (subcmd[0] == 'v')
  {
    // "volt"=absolute values, "vdev"=deviations
    FormatBatteryVolts(verbosity, writer, (subcmd[1]=='d') ? true : false);
  }

  else if (subcmd[0] == 't')
  {
    // "temp"=absolute values, "tdev"=deviations
    FormatBatteryTemps(verbosity, writer, (subcmd[1]=='d') ? true : false);
  }

  else if (strcmp(subcmd, "data-pack") == 0)
  {
    int pack = (argc > 0) ? atoi(argv[0]) : 1;
    if (pack < 1 || pack > batt_pack_count) {
      writer->printf("ERROR: pack number out of range [1-%d]\n", batt_pack_count);
      res = Fail;
    } else {
      FormatPackData(verbosity, writer, pack-1);
    }
  }

  else if (strcmp(subcmd, "data-cell") == 0)
  {
    int cell = (argc > 0) ? atoi(argv[0]) : 1;
    if (cell < 1 || cell > batt_cell_count) {
      writer->printf("ERROR: cell number out of range [1-%d]\n", batt_cell_count);
      res = Fail;
    } else {
      FormatCellData(verbosity, writer, cell-1);
    }
  }

  BatteryUnlock();

  return res;
}


/**
 * FormatBatteryStatus: output status report (alerts & watches)
 */
void OvmsVehicleVectrixVX1::FormatBatteryStatus(int verbosity, OvmsWriter* writer, int pack)
{
  int capacity = verbosity;
  const char *em;
  int c, val;

  // Voltage deviations:
  capacity -= writer->printf("Volts: ");

  // standard deviation:
  val = CONV_CellVolt(vx1_batt[pack].cell_volt_stddev_max);
  if (vx1_batt[pack].volt_alerts.test(BATT_STDDEV_VOLT_FLAG))
    em = "!";
  else if (vx1_batt[pack].volt_watches.test(BATT_STDDEV_VOLT_FLAG))
    em = "?";
  else
    em = "";
  capacity -= writer->printf("%sSD:%dmV ", em, val);

  if ((vx1_batt[pack].volt_alerts.none()) && (vx1_batt[pack].volt_watches.none()))
  {
    capacity -= writer->printf("OK ");
  }
  else
  {
    for (c = 0; c < batt_cell_count; c++)
    {
      // check length:
      if (capacity < 12)
      {
        writer->puts("...");
        return;
      }

      // Alert / Watch?
      if (vx1_batt[pack].volt_alerts.test(c))
        em = "!";
      else if (vx1_batt[pack].volt_watches.test(c))
        em = "?";
      else
        continue;

      val = CONV_CellVoltS(vx1_cell[c].volt_maxdev);

      capacity -= writer->printf("%sC%d:%+dmV ", em, c+1, val);
    }
  }

  // check length:
  if (capacity < 20)
  {
    writer->puts("...");
    return;
  }

  // Temperature deviations:
  capacity -= writer->printf("Temps: ");

  // standard deviation:
  val = vx1_batt[pack].cmod_temp_stddev_max;
  if (vx1_batt[pack].temp_alerts.test(BATT_STDDEV_TEMP_FLAG))
    em = "!";
  else if (vx1_batt[pack].temp_watches.test(BATT_STDDEV_TEMP_FLAG))
    em = "?";
  else
    em = "";
  capacity -= writer->printf("%sSD:%dC ", em, val);

  if ((vx1_batt[pack].temp_alerts.none()) && (vx1_batt[pack].temp_watches.none()))
  {
    capacity -= writer->printf("OK ");
  }
  else
  {
    for (c = 0; c < batt_cmod_count; c++)
    {
      // check length:
      if (capacity < 8)
      {
        writer->puts("...");
        return;
      }

      // Alert / Watch?
      if (vx1_batt[pack].temp_alerts.test(c))
        em = "!";
      else if (vx1_batt[pack].temp_watches.test(c))
        em = "?";
      else
        continue;

      val = vx1_cmod[c].temp_maxdev;

      capacity -= writer->printf("%sM%d:%+dC ", em, c+1, val);
    }
  }

  writer->puts("");
}


/**
 * FormatBatteryVolts: output voltage report (absolute / deviations)
 */
void OvmsVehicleVectrixVX1::FormatBatteryVolts(int verbosity, OvmsWriter* writer, bool show_deviations)
{
  int capacity = verbosity;
  const char *em;

  // Output pack status:
  for (int p = 0; p < batt_pack_count; p++)
  {
    // output capacity reached?
    if (capacity < 13)
      break;

    if (show_deviations)
    {
      if (vx1_batt[p].volt_alerts.test(BATT_STDDEV_VOLT_FLAG))
        em = "!";
      else if (vx1_batt[p].volt_watches.test(BATT_STDDEV_VOLT_FLAG))
        em = "?";
      else
        em = "";
      capacity -= writer->printf("%sSD:%dmV ", em, CONV_CellVolt(vx1_batt[p].cell_volt_stddev_max));
    }
    else
    {
      capacity -= writer->printf("P:%.2fV ", (float) CONV_PackVolt(vx1_batt[p].volt_act) / 100);
    }
  }

  // Output cell status:
  for (int c = 0; c < batt_cell_count; c++)
  {
    int p = 0; // fixed for now…

    // output capacity reached?
    if (capacity < 13)
      break;

    // Alert?
    if (vx1_batt[p].volt_alerts.test(c))
      em = "!";
    else if (vx1_batt[p].volt_watches.test(c))
      em = "?";
    else
      em = "";

    if (show_deviations)
      capacity -= writer->printf("%s%d:%+dmV ", em, c+1, CONV_CellVoltS(vx1_cell[c].volt_maxdev));
    else
      capacity -= writer->printf("%s%d:%.3fV ", em, c+1, (float) CONV_CellVolt(vx1_cell[c].volt_act) / 1000);
  }

  writer->puts("");
}


/**
 * FormatBatteryTemps: output temperature report (absolute / deviations)
 */
void OvmsVehicleVectrixVX1::FormatBatteryTemps(int verbosity, OvmsWriter* writer, bool show_deviations)
{
  int capacity = verbosity;
  const char *em;

  // Output pack status:
  for (int p = 0; p < batt_pack_count; p++)
  {
    if (capacity < 17)
      break;

    if (show_deviations)
    {
      if (vx1_batt[p].temp_alerts.test(BATT_STDDEV_TEMP_FLAG))
        em = "!";
      else if (vx1_batt[p].temp_watches.test(BATT_STDDEV_TEMP_FLAG))
        em = "?";
      else
        em = "";
      capacity -= writer->printf("%sSD:%dC ", em, vx1_batt[0].cmod_temp_stddev_max);
    }
    else
    {
      capacity -= writer->printf("P:%dC (%dC..%dC) ",
        CONV_Temp(vx1_batt[p].temp_act),
        CONV_Temp(vx1_batt[p].temp_min),
        CONV_Temp(vx1_batt[p].temp_max));
    }
  }

  // Output cmod status:
  for (int c = 0; c < batt_cmod_count; c++)
  {
    int p = 0; // fixed for now…

    if (capacity < 8)
      break;

    // Alert?
    if (vx1_batt[p].temp_alerts.test(c))
      em = "!";
    else if (vx1_batt[p].temp_watches.test(c))
      em = "?";
    else
      em = "";

    if (show_deviations)
      capacity -= writer->printf("%s%d:%+dC ", em, c+1, vx1_cmod[c].temp_maxdev);
    else
      capacity -= writer->printf("%s%d:%dC ", em, c+1, CONV_Temp(vx1_cmod[c].temp_act));
  }

  writer->puts("");
}


/**
 * FormatPackData: output RT-BAT-P record
 */
void OvmsVehicleVectrixVX1::FormatPackData(int verbosity, OvmsWriter* writer, int pack)
{
  if (verbosity < 200)
    return;

  int volt_alert, temp_alert;

  if (vx1_batt[pack].volt_alerts.any())
    volt_alert = 3;
  else if (vx1_batt[pack].volt_watches.any())
    volt_alert = 2;
  else
    volt_alert = 1;

  if (vx1_batt[pack].temp_alerts.any())
    temp_alert = 3;
  else if (vx1_batt[pack].temp_watches.any())
    temp_alert = 2;
  else
    temp_alert = 1;

  // Output pack status:
  //  RT-BAT-P,0,86400
  //  ,<volt_alertstatus>,<temp_alertstatus>
  //  ,<soc>,<soc_min>,<soc_max>
  //  ,<volt_act>,<volt_min>,<volt_max>
  //  ,<temp_act>,<temp_min>,<temp_max>
  //  ,<cell_volt_stddev_max>,<cmod_temp_stddev_max>
  //  ,<max_drive_pwr>,<max_recup_pwr>

  writer->printf(
    "RT-BAT-P,%d,86400"
    ",%d,%d"
    ",%d,%d,%d"
    ",%d,%d,%d"
    ",%d,%d,%d"
    ",%d,%d"
    ",%d,%d",
    pack+1,
    volt_alert, temp_alert,
    vx1_soc, vx1_soc_min, vx1_soc_max,
    vx1_batt[pack].volt_act,
    vx1_batt[pack].volt_min,
    vx1_batt[pack].volt_max,
    CONV_Temp(vx1_batt[pack].temp_act),
    CONV_Temp(vx1_batt[pack].temp_min),
    CONV_Temp(vx1_batt[pack].temp_max),
    CONV_CellVolt(vx1_batt[pack].cell_volt_stddev_max),
    (int) (vx1_batt[pack].cmod_temp_stddev_max + 0.5),
    (int) vx1_batt[pack].max_drive_pwr * 5,
    (int) vx1_batt[pack].max_recup_pwr * 5);

}


/**
 * FormatCellData: output RT-BAT-C record
 */
void OvmsVehicleVectrixVX1::FormatCellData(int verbosity, OvmsWriter* writer, int cell)
{
  if (verbosity < 200)
    return;

  int pack = 0; // currently fixed, TODO for addon packs: determine pack index for cell
  int volt_alert, temp_alert;

  if (vx1_batt[pack].volt_alerts.test(cell))
    volt_alert = 3;
  else if (vx1_batt[pack].volt_watches.test(cell))
    volt_alert = 2;
  else
    volt_alert = 1;

  if (vx1_batt[pack].temp_alerts.test(cell >> 1))
    temp_alert = 3;
  else if (vx1_batt[pack].temp_watches.test(cell >> 1))
    temp_alert = 2;
  else
    temp_alert = 1;

  // Output cell status:
  //  RT-BAT-C,<cellnr>,86400
  //  ,<volt_alertstatus>,<temp_alertstatus>,
  //  ,<volt_act>,<volt_min>,<volt_max>,<volt_maxdev>
  //  ,<temp_act>,<temp_min>,<temp_max>,<temp_maxdev>

  writer->printf(
    "RT-BAT-C,%d,86400"
    ",%d,%d"
    ",%d,%d,%d,%d"
    ",%d,%d,%d,%d",
    cell+1,
    volt_alert, temp_alert,
    CONV_CellVolt(vx1_cell[cell].volt_act),
    CONV_CellVolt(vx1_cell[cell].volt_min),
    CONV_CellVolt(vx1_cell[cell].volt_max),
    CONV_CellVoltS(vx1_cell[cell].volt_maxdev),
    CONV_Temp(vx1_cmod[cell >> 1].temp_act),
    CONV_Temp(vx1_cmod[cell >> 1].temp_min),
    CONV_Temp(vx1_cmod[cell >> 1].temp_max),
    (int) (vx1_cmod[cell >> 1].temp_maxdev + 0.5));

}


/**
 * BatterySendDataUpdate: send data notifications for modified packs & cells
 */
void OvmsVehicleVectrixVX1::BatterySendDataUpdate(bool force)
{
  bool modified = force |
    StdMetrics.ms_v_bat_soc->IsModifiedAndClear(m_modifier) |
    m_batt_use_soc_min->IsModifiedAndClear(m_modifier) |
    m_batt_use_soc_max->IsModifiedAndClear(m_modifier) |
    m_batt_use_volt_min->IsModifiedAndClear(m_modifier) |
    m_batt_use_volt_max->IsModifiedAndClear(m_modifier) |
    m_batt_use_temp_min->IsModifiedAndClear(m_modifier) |
    m_batt_use_temp_max->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_voltage->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_voltage->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_vdevmax->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_valert->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_temp->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_temp->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_tdevmax->IsModifiedAndClear(m_modifier) |
    StdMetrics.ms_v_bat_cell_talert->IsModifiedAndClear(m_modifier);

  if (!modified)
    return;

  StringWriter buf(100);

  for (int pack=0; pack < batt_pack_count; pack++) {
    buf.clear();
    FormatPackData(COMMAND_RESULT_NORMAL, &buf, pack);
    MyNotify.NotifyString("data", "xvv.battery.log", buf.c_str());
  }

  for (int cell=0; cell < batt_cell_count; cell++) {
    buf.clear();
    FormatCellData(COMMAND_RESULT_NORMAL, &buf, cell);
    MyNotify.NotifyString("data", "xvv.battery.log", buf.c_str());
  }
}
