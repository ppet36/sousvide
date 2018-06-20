# DIY Sous Vide cooker

![alt](/images/all.jpg?raw=true)

This repository contains resources for my DIY WiFi Sous Vide cooker. Repo contains
responsive Angular/Bootstrap application for setting cooking parameters and manage
cooker, Eagle PCB design w/ schematic and 3D model of enclosure.

Cooker are based on submersible heater PROFESSOR PO500 and EHEIM Compact 300
aquarium pump. For measuring temperature watertight DS18B20 sensor is used. Analog
ESP PIN is used for detecting water level and protecting "dry" run of cooker.

Memory of ESP12E is divided 1M for firmware and 3M for SPIFFS. Application
(arduino/data/*) is uploaded to ESP's SPIFFS and is served via internal HTTP
server.

![alt](/images/appl.jpg?raw=true)

Some parts are printed on 3D printer, bottom part of cooker is bended aluminium
sheet.

