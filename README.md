# solar-display

A Display to change the way you use and monitor your solar energy production and your energy consumption.


![Solar Display example](/ReadmeImages/solardisplay1.jpg)
![Solar Display example](/ReadmeImages/solardisplay2.jpg)
![Solar Display example](/ReadmeImages/solardisplay3.jpg)
![Solar Display example](/ReadmeImages/solardisplay4.jpg)
![Solar Display example](/ReadmeImages/solardisplay5.jpg)
![Solar Display example](/ReadmeImages/solardisplay6.jpg)
![Solar Display example](/ReadmeImages/solardisplay7.jpg)
![Solar Display example](/ReadmeImages/solardisplay8.jpg)

I have never done a git project before, this is the first time i share one of my projects as the amount of time i put into it feels pointless if not one other person can enjoy it with me.
I just wanted a pretty power flow Display, this idea then snowbaled into, but could i add (insert random graph or value)
There will be issues so please let me know, i will try to resolve or help where i can.



Inverter Compatibility:
Currently there is a version that works with the sigenergy inverter, this is made to work with a inverter with a battery.
Hacs Integration:
Sigenergy ESS

there is also a version for solar-edge.
Hacs Integration:
SolarEdge Modbus Multi

Software:
HOME ASSISTANT:
The backbone of this project.
Files needed in home assistant: 
Energy.yaml : A file for making energy meters, for Making usable sensors and allot of helpers and other needed entities.
energydevices.yaml : This file is to add entities from home assistant into the device, on the Home usage page there is the option to display powerflow within the house meaning energy flows from home to the first subpanel and to different devices after that.
                     we add the files power entities to this file to display this power flow. once devices are added you can select them in the settings of the display, there is a couple of images you can choose.

Sigenergy:
once you have the integration installed make sure the naming is standerd, like this sensor.sigen_inverter... as we will need it to be standard for the energy file

solar-edge:
as i have 2 inverters i needed to use the modbus multi integration for this, and i needed a energy file to do many of the extra calculations for this, i have the naming like this: B1 (battery), I1 and I2 (inverters), M1 (Meter). I will try to redo the energy file so this will also be possible for single inverter systems.

ESPHOME:
Recommended version: 2025.10.0 (i had issues with newer version) I recommend building this using a Command line version of esphome builder on a powerfull computer, as building this on a raspberry pi is very demanding. 



Hardware:
- 3D printed Case: 
- Display: Waveshare -S3 7inch Display Development Board Type B,
           ESP32 With Display, 1024×600, Optional For Touch Function, 32-bit LX7 Dual-core Processor,
           to 240MHz Frequency, Supports WiFi & Bluetooth
- 8 Heatset inserts M3
- 12 small scews M3
- MMWave Sensor https://de.aliexpress.com/item/1005007103251173.html?gatewayAdapt=glo2deu Using 3.3v/ground and OT2 on the gpio of the display (the display will work without, best to go into the settings and change the motion sensor option)
- Power plug https://de.aliexpress.com/item/1005007524400372.html?gatewayAdapt=glo2deu buy the 6 pin version as this will work with both direction of a usb c cable and the display is very difficult with power.



The case can be 3d printed, however my design for the case is not ideal as this is one of the first models i have made.
To screw the display to the case you will need a bendable screwing attachement like in the ifixit kit, If someone wants to redesign please PM for the CAD files.





