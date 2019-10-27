.. csv-table:: Standard Metric
   :header: Metric Name,Metric Type,Metric variable Name,Unit,Example Value,Metric Description
   :widths: auto, auto, auto, auto, auto, auto
   :stub-columns: 1

   Module,,,,,
   m.version,OvmsMetricString,ms_m_version,,,,
   m.hardware,OvmsMetricString,ms_m_hardware,,,,
   m.serial,OvmsMetricString,ms_m_serial,,,,
   m.tasks,OvmsMetricInt,ms_m_tasks,,,,
   m.freeram,OvmsMetricInt,ms_m_freeram,,,,
   m.monotonic,OvmsMetricInt,ms_m_monotonic,,,,
   m.time.utc,OvmsMetricInt,ms_m_timeutc,,,,
   Network,,,,,
   m.net.type,OvmsMetricString,ms_m_net_type,,,none, wifi, modem
   m.net.sq,OvmsMetricInt,ms_m_net_sq,[dbm],,Network signal quality
   m.net.provider,OvmsMetricString,ms_m_net_provider,,,Network provider name
   m.net.wifi.network,OvmsMetricString,ms_m_net_wifi_network,,,Wifi network SSID
   m.net.wifi.sq,OvmsMetricFloat,ms_m_net_wifi_sq,[dbm],,Wifi network signal quality
   m.net.mdm.network,OvmsMetricString,ms_m_net_mdm_network,,,Modem network operator
   m.net.mdm.sq,OvmsMetricFloat,ms_m_net_mdm_sq,[dbm],,Modem network signal quality
   m.net.mdm.iccid,OvmsMetricString,ms_m_net_mdm_iccid,,,,
   m.net.mdm.model,OvmsMetricString,ms_m_net_mdm_model,,,,
   Server,,,,,
   s.v2.connected,OvmsMetricBool,ms_s_v2_connected,,,True = V2 server connected [1]
   s.v2.peers,OvmsMetricInt,ms_s_v2_peers,,,V2 clients connected [1]
   s.v3.connected,OvmsMetricBool,ms_s_v3_connected,,,True = V3 server connected [1]
   s.v3.peers,OvmsMetricInt,ms_s_v3_peers,,,V3 clients connected [1]
   Vehicle Identity,,,,,
   v.type,OvmsMetricString,ms_v_type,,,Vehicle type code
   v.vin,OvmsMetricString,ms_v_vin,,,Vehicle identification number
   Battery Status,,,,,
   v.b.soc,OvmsMetricFloat,ms_v_bat_soc, [%],,State of charge
   v.b.soh,OvmsMetricFloat,ms_v_bat_soh, [%],,State of health
   v.b.cac,OvmsMetricFloat,ms_v_bat_cac, [Ah],,Calculated capacity
   v.b.health,OvmsMetricString,ms_v_bat_health,,,General textual description of battery health
   v.b.voltage,OvmsMetricFloat,ms_v_bat_voltage, [V],,Main battery momentary voltage
   v.b.current,OvmsMetricFloat,ms_v_bat_current, [A],,Main battery momentary current
   v.b.coulomb.used,OvmsMetricFloat,ms_v_bat_coulomb_used, [Ah],,Main battery coulomb used on trip
   v.b.coulomb.recd,OvmsMetricFloat,ms_v_bat_coulomb_recd, [Ah],,Main battery coulomb recovered on trip
   v.b.power,OvmsMetricFloat,ms_v_bat_power, [kW],,Main battery momentary power
   v.b.consumption,OvmsMetricFloat,ms_v_bat_consumption, [Wh/km],,Main battery momentary consumption
   v.b.energy.used,OvmsMetricFloat,ms_v_bat_energy_used, [kWh],,Main battery energy used on trip
   v.b.energy.recd,OvmsMetricFloat,ms_v_bat_energy_recd, [kWh],,Main battery energy recovered on trip
   v.b.range.full,OvmsMetricFloat,ms_v_bat_range_full, [km],,Ideal range at 100% SOC & current conditions
   v.b.range.ideal,OvmsMetricFloat,ms_v_bat_range_ideal, [km],,Ideal range
   v.b.range.est,OvmsMetricFloat,ms_v_bat_range_est, [km],,Estimated range
   v.b.12v.voltage,OvmsMetricFloat,ms_v_bat_12v_voltage, [V],,Auxiliary 12V battery momentary voltage
   v.b.12v.current,OvmsMetricFloat,ms_v_bat_12v_current, [A],,Auxiliary 12V battery momentary current
   v.b.12v.voltage.ref,OvmsMetricFloat,ms_v_bat_12v_voltage_ref, [V],,Auxiliary 12V battery reference voltage
   v.b.12v.voltage.alert,OvmsMetricBool,ms_v_bat_12v_voltage_alert,,,True = auxiliary battery under voltage alert
   v.b.temp,OvmsMetricFloat,ms_v_bat_temp, [�C],,Battery temperature
   Battery Cells [%],,,,,
   v.b.p.level.min,OvmsMetricFloat,ms_v_bat_pack_level_min, [%],,Cell level - weakest cell in pack
   v.b.p.level.max,OvmsMetricFloat,ms_v_bat_pack_level_max, [%],,Cell level - strongest cell in pack
   v.b.p.level.avg,OvmsMetricFloat,ms_v_bat_pack_level_avg, [%],,Cell level - pack average
   v.b.p.level.stddev,OvmsMetricFloat,ms_v_bat_pack_level_stddev, [%],,Cell level - pack standard deviation
   ,,,,,Note: level = voltage levels normalized to percent
   Battery Cells [V],,,,,
   v.b.p.voltage.min,OvmsMetricFloat,ms_v_bat_pack_vmin, [V],,Cell voltage - weakest cell in pack
   v.b.p.voltage.max,OvmsMetricFloat,ms_v_bat_pack_vmax, [V],,Cell voltage - strongest cell in pack
   v.b.p.voltage.avg,OvmsMetricFloat,ms_v_bat_pack_vavg, [V],,Cell voltage - pack average
   v.b.p.voltage.stddev,OvmsMetricFloat,ms_v_bat_pack_vstddev, [V],,Cell voltage - current standard deviation
   v.b.p.voltage.stddev.max,OvmsMetricFloat,ms_v_bat_pack_vstddev_max, [V],,Cell voltage - maximum standard deviation observed
   Battery Cells Temperature [�C],,,,,
   v.b.p.temp.min,OvmsMetricFloat,ms_v_bat_pack_tmin, [�C],,Cell temperature - coldest cell in pack
   v.b.p.temp.max,OvmsMetricFloat,ms_v_bat_pack_tmax, [�C],,Cell temperature - warmest cell in pack
   v.b.p.temp.avg,OvmsMetricFloat,ms_v_bat_pack_tavg, [�C],,Cell temperature - pack average
   v.b.p.temp.stddev,OvmsMetricFloat,ms_v_bat_pack_tstddev, [�C],,Cell temperature - current standard deviation
   v.b.p.temp.stddev.max,OvmsMetricFloat,ms_v_bat_pack_tstddev_max, [�C],,Cell temperature - maximum standard deviation observed
   Battery Cells Stats Voltage [V],,,,,
   v.b.c.voltage,OvmsMetricVector<float>,ms_v_bat_cell_voltage, [V],,Cell voltages
   v.b.c.voltage.min,OvmsMetricVector<float>,ms_v_bat_cell_vmin, [V],,Cell minimum voltages
   v.b.c.voltage.max,OvmsMetricVector<float>,ms_v_bat_cell_vmax, [V],,Cell maximum voltages
   v.b.c.voltage.dev.max,OvmsMetricVector<float>,ms_v_bat_cell_vdevmax, [V],,Cell maximum voltage deviation observed
   v.b.c.voltage.alert,OvmsMetricVector<short>,ms_v_bat_cell_valert,,,Cell voltage deviation alert level [0=normal, 1=warning, 2=alert]
   Battery Cells Stats Temperature [�C],,,,,
   v.b.c.temp,OvmsMetricVector<float>,ms_v_bat_cell_temp, [�C],,Cell temperatures
   v.b.c.temp.min,OvmsMetricVector<float>,ms_v_bat_cell_tmin, [�C],,Cell minimum temperatures
   v.b.c.temp.max,OvmsMetricVector<float>,ms_v_bat_cell_tmax, [�C],,Cell maximum temperatures
   v.b.c.temp.dev.max,OvmsMetricVector<float>,ms_v_bat_cell_tdevmax, [�C],,Cell maximum temperature deviation observed
   v.b.c.temp.alert,OvmsMetricVector<short>,ms_v_bat_cell_talert,,,Cell temperature deviation alert level [0=normal, 1=warning, 2=alert]
   Charger Status,,,,,
   v.c.voltage,OvmsMetricFloat,ms_v_charge_voltage, [V],,Momentary charger supply voltage
   v.c.current,OvmsMetricFloat,ms_v_charge_current, [A],,Momentary charger output current
   v.c.climit,OvmsMetricFloat,ms_v_charge_climit, [A],,Maximum charger output current
   v.c.time,OvmsMetricInt,ms_v_charge_time, [sec],,Duration of running charge
   v.c.kwh,OvmsMetricFloat,ms_v_charge_kwh, [kWh],,Energy sum for running charge
   v.c.mode,OvmsMetricString,ms_v_charge_mode,,,standard, range, performance, storage
   v.c.timermode,OvmsMetricBool,ms_v_charge_timermode,,,True if timer enabled
   v.c.timerstart,OvmsMetricInt,ms_v_charge_timerstart,,,Time timer is due to start
   v.c.state,OvmsMetricString,ms_v_charge_state,,,charging, topoff, done, prepare, timerwait, heating, stopped
   v.c.substate,OvmsMetricString,ms_v_charge_substate,,,scheduledstop, scheduledstart, onrequest, timerwait, powerwait, stopped, interrupted
   v.c.type,OvmsMetricString,ms_v_charge_type,,,undefined, type1, type2, chademo, roadster, teslaus, supercharger, ccs
   v.c.pilot,OvmsMetricBool,ms_v_charge_pilot,,,Pilot signal present
   v.c.charging,OvmsMetricBool,ms_v_charge_inprogress,,,True = currently charging
   v.c.limit.range,OvmsMetricFloat,ms_v_charge_limit_range, [km],,Sufficient range limit for current charge
   v.c.limit.soc,OvmsMetricFloat,ms_v_charge_limit_soc, [%],,Sufficient SOC limit for current charge
   v.c.duration.full,OvmsMetricInt,ms_v_charge_duration_full, [min],,Estimated time remaing for full charge
   v.c.duration.range,OvmsMetricInt,ms_v_charge_duration_range, [min],,� for sufficient range
   v.c.duration.soc,OvmsMetricInt,ms_v_charge_duration_soc, [min],,� for sufficient SOC
   v.c.temp,OvmsMetricFloat,ms_v_charge_temp, [�C],,Charger temperature
   Inverter Status,,,,,
   v.i.temp,OvmsMetricFloat,ms_v_inv_temp, [�C],,Inverter temperature
   Motor Status,,,,,
   v.m.rpm,OvmsMetricInt,ms_v_mot_rpm,[RPM],,Motor speed
   v.m.temp,OvmsMetricFloat,ms_v_mot_temp, [�C],,Motor temperature
   Doors Status,,,,,
   v.d.fl,OvmsMetricBool,ms_v_door_fl,,,,Front Left Door Open/Close
   v.d.fr,OvmsMetricBool,ms_v_door_fr,,,,Front Right Door Open/Close
   v.d.rl,OvmsMetricBool,ms_v_door_rl,,,,Rear Left Door Open/Close
   v.d.rr,OvmsMetricBool,ms_v_door_rr,,,,Rear Right Door Open/Close
   v.d.cp,OvmsMetricBool,ms_v_door_chargeport,,,,Chargeport Open/Close
   v.d.hood,OvmsMetricBool,ms_v_door_hood,,,,Hood Open/Close
   v.d.trunk,OvmsMetricBool,ms_v_door_trunk,,,,Trunk Open/Close
   Vehicle Status,,,,,
   v.e.drivemode,OvmsMetricInt,ms_v_env_drivemode,,,Active drive profile number [1]
   v.e.gear,OvmsMetricInt,ms_v_env_gear,,,Gear/direction, negative=reverse, 0=neutral [1]
   v.e.throttle,OvmsMetricFloat,ms_v_env_throttle, [%],,Drive pedal state
   v.e.footbrake,OvmsMetricFloat,ms_v_env_footbrake, [%],,Brake pedal state
   v.e.handbrake,OvmsMetricBool,ms_v_env_handbrake,,,Handbrake state
   v.e.regenbrake,OvmsMetricBool,ms_v_env_regenbrake,,,Regenerative braking state
   v.e.awake,OvmsMetricBool,ms_v_env_awake,,,Vehicle/bus awake (switched on)
   v.e.charging12v,OvmsMetricBool,ms_v_env_charging12v,,,12V battery charging
   v.e.cooling,OvmsMetricBool,ms_v_env_cooling,,,,Cooling
   v.e.heating,OvmsMetricBool,ms_v_env_heating,,,,Heating
   v.e.hvac,OvmsMetricBool,ms_v_env_hvac,,,Climate control system state
   v.e.on,OvmsMetricBool,ms_v_env_on,,,Ignition state (drivable)
   v.e.locked,OvmsMetricBool,ms_v_env_locked,,,Vehicle locked
   v.e.valet,OvmsMetricBool,ms_v_env_valet,,,Vehicle in valet mode
   v.e.headlights,OvmsMetricBool,ms_v_env_headlights,,,,Head Lights
   v.e.alarm,OvmsMetricBool,ms_v_env_alarm,,,,Alarm
   v.e.parktime,OvmsMetricInt,ms_v_env_parktime,,,,Park Time
   v.e.drivetime,OvmsMetricInt,ms_v_env_drivetime,,,,Drive Time
   v.e.c.login,OvmsMetricBool,ms_v_env_ctrl_login,,,Module logged in at ECU/controller
   v.e.c.config,OvmsMetricBool,ms_v_env_ctrl_config,,,ECU/controller in configuration state
   v.e.temp,OvmsMetricFloat,ms_v_env_temp,[�C],,Ambient temperature
   v.e.cabintemp,OvmsMetricFloat,ms_v_env_cabintemp,[�C],,Cabin temperature
   GPS Status,,,,,
   v.p.gpslock,OvmsMetricBool,ms_v_pos_gpslock,,,,GPS lock status
   v.p.gpsmode,OvmsMetricString,ms_v_pos_gpsmode,,,<GPS><GLONASS>, N/A/D/E (None/Autonomous/Differential/Estimated)
   v.p.gpshdop,OvmsMetricFloat,ms_v_pos_gpshdop,,,Horizontal dilution of precision (smaller=better)
   v.p.satcount,OvmsMetricInt,ms_v_pos_satcount,,,,Number of Satellite
   v.p.latitude,OvmsMetricFloat,ms_v_pos_latitude,,,,Latitude
   v.p.longitude,OvmsMetricFloat,ms_v_pos_longitude,,,,Longitude
   v.p.direction,OvmsMetricFloat,ms_v_pos_direction,,,,Direction
   v.p.altitude,OvmsMetricFloat,ms_v_pos_altitude,,,,Altitude
   v.p.speed,OvmsMetricFloat,ms_v_pos_speed,[kph],,Vehicle speed
   v.p.acceleration,OvmsMetricFloat,ms_v_pos_acceleration, [m/s�],,Vehicle acceleration
   v.p.gpsspeed,OvmsMetricFloat,ms_v_pos_gpsspeed, [kph],,GPS speed over ground
   v.p.odometer,OvmsMetricFloat,ms_v_pos_odometer,,[Km],,Odometer
   v.p.trip,OvmsMetricFloat,ms_v_pos_trip,,,,Trip
   Tire Pressure,,,,,
   v.tp.fl.t,OvmsMetricFloat,ms_v_tpms_fl_t,,[�C],,Tire temperature Front Left
   v.tp.fr.t,OvmsMetricFloat,ms_v_tpms_fr_t,,[�C],,Tire temperature Front Right
   v.tp.rr.t,OvmsMetricFloat,ms_v_tpms_rr_t,,[�C],,Tire temperature Rear Right
   v.tp.rl.t,OvmsMetricFloat,ms_v_tpms_rl_t,,[�C],,Tire temperature Rear Left
   v.tp.fl.p,OvmsMetricFloat,ms_v_tpms_fl_p,,[psi],,Tire pressure Front Left
   v.tp.fr.p,OvmsMetricFloat,ms_v_tpms_fr_p,,[psi],,Tire pressure Front Right
   v.tp.rr.p,OvmsMetricFloat,ms_v_tpms_rr_p,,[psi],,Tire pressure Rear Right
   v.tp.rl.p,OvmsMetricFloat,ms_v_tpms_rl_p,,[psi],,Tire pressure Rear Left
