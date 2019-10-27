=============
Vectrix VX-1
=============

Vehicle Type: **VV**

At present, support for the Vectrix VX-1 in OVMS is experimental and under development. This vehicle type should not be used by anyone other than those actively involved in development of support for this vehicle.

----------------
Support Overview
----------------

=========================== ==============
Function                    Support Status
=========================== ==============
Hardware                    No specific requirements
Vehicle Cable               DB9 to DB9 Data Cable for OVMS
GSM Antenna                 Open Vehicles OVMS GSM Antenna (or any compatible antenna)
GPS Antenna                 Universal GPS Antenna (SMA Connector) (or any compatible antenna)
SOC Display                 Not currently supported
Range Display               Not currently supported
GPS Location                Yes (provide by OVMS)
Speed Display               Yes
Temperature Display         Not currently supported
BMS v+t Display             Yes (Only with Li battery)
TPMS Display                No
Charge Status Display       Not currently supported
Charge Interruption Alerts  No
Charge Control              No
Cabin Pre-heat/cool Control No
Lock/Unlock Vehicle         No
Valet Mode Control          No
Others                      tba
=========================== ==============

------------
Vehicle cable
------------

The Vectrix have one CAN-bus available on the DB9 port. You can use a simple straight through Cable DB9 F/F.

In case you want to build your own cable, here’s the pinout:

======= ======= ========
DB9-F   DB9-F   Signal
1       1       Not Connected
2       2       CAN-0L (can Low)
3       3       Chassis / Power GND
4       4       Not Connected
5       5       Shield
6       6       Not Connected
7       7       CAN-0H (can High)
8       8       Not Connected
9       9       +5V Vehicle Power
======= ======= ========

--------
Contents
--------

.. toctree::
   :maxdepth: 1

   metrics_std
