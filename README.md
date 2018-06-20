# DIY Sous Vide cooker

![alt](/images/all.jpg?raw=true)

This repository contains resources for my DIY WiFi Sous Vide cooker. Repo contains
responsive Angular/Bootstrap application for setting cooking parameters and manage
cooker, Eagle PCB design w/ schematic and 3D model of enclosure.

Cooker are based on ESP12E module, submersible heater PROFESSOR PO500 and EHEIM
Compact 300 aquarium pump.
For measuring temperature watertight DS18B20 sensor is used. Analog
ESP PIN is used for detecting water level and protecting "dry" run of cooker.
Some parts are printed on 3D printer, bottom part of cooker is bended aluminium
sheet.

Memory of ESP12E is divided 1M for firmware and 3M for SPIFFS. Application
(arduino/data/*) is uploaded to ESP's SPIFFS and is served via internal HTTP
server. Firmware creates AP SousVideCooker with listening HTTP server at:

```
http://192.168.4.1/
```
![alt](/images/appl.png?raw=true)

AP password is defaulted to 123456 and can be set in fw source file.

Via application can be also set PID parameters for heater. Firmware uses AutoPID
library from https://github.com/r-downing/AutoPID.

Heater as well as pump is driven via triacs. Duo MOC3041/BT136 is used for heater
and plain MOC3041 for pump (which is only 5W). For 500W heater is not needed
additional BT136 cooling.

![alt](/eagle/schematic.png?raw=true)

Module is powered via 5V/1A PSU.

![alt](/eagle/board.png?raw=true)

PCB is designed to fit on top of mounting cube.

![alt](/images/unboxed.jpg?raw=true)




