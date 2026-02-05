# solar-display

A tabletop display to change the way you use and monitor your solar energy production and energy consumption.

---

## Image

Example images of the Solar Display can be found in the `ReadmeImages` folder:


![Solar Display example](/ReadmeImages/solardisplay3.jpg)


---

## About This Project

I have never done a GitHub project before. This is the first time I am sharing one of my projects, and it felt pointless to keep it to myself after putting so much time into it if no one else could enjoy or use it.

I originally just wanted a **pretty power flow display**. That idea quickly snowballed into:

> “But could I also add this graph… or that value?”

There **will be issues**, so please let me know if you find any. I will try to resolve them or help where I can.

---

## Inverter Compatibility

### Sigenergy
- Currently there is a version that works with the **Sigenergy inverter**
- This version is made for systems **with a battery**
- Home Assistant Integration (HACS):
  - **Sigenergy ESS**

### SolarEdge
- There is also a version for **SolarEdge**
- Home Assistant Integration (HACS):
  - **SolarEdge Modbus Multi**

---

## Software

### Home Assistant
Home Assistant is the **backbone** of this project.

#### Files needed in Home Assistant

Place these files in the packages folder (config/packages/)

- **`energy.yaml`**  
  Used for creating energy meters, usable sensors, helpers, and other required entities.

- **`energydevices.yaml`**  
  This file is used to add Home Assistant entities as devices.  
  On the *Home Usage* page, there is an option to display power flow within the house. This allows energy to flow from the home to the first subpanel and then to individual devices.

  The power entities are added here so they can be displayed in the power flow view. Once devices are added, they can be selected in the device usage settings. There are several images you can choose from for device icons, and you have to set the name of the device on the display.

---

### Sigenergy Notes

Once the integration is installed, make sure the entity naming is the **standard** sigenergy naming.

sensor.sigen_inverter_...

This naming is required for the energy file to work correctly.

---

### SolarEdge Notes

Because I have **two inverters**, I needed to use the *SolarEdge Modbus Multi* integration.

I also needed an energy file to handle many of the additional calculations.

My current naming convention is (this is the standard provided by the integration do not change):
- **B1** – Battery
- **I1 / I2** – Inverters
- **M1** – Meter

I will try to redo the energy file so this setup will also work for **single inverter systems**.

---

## ESPHome

- **Recommended version:** `2025.10.0`  
  I had issues with newer versions.

I recommend building this using the **command-line version of ESPHome** on a powerful computer. Building on a Raspberry Pi is very demanding and not recommended.

---

## Hardware

- **3D Printed Case**

- **Display:**  
  Waveshare S3 7-inch Display Development Board (Type B)  
  ESP32 with Display  
  Resolution: 1024×600  
  Optional touch function  
  32-bit LX7 dual-core processor (up to 240 MHz)  
  WiFi & Bluetooth support
  [Waveshhare Display Link](https://www.waveshare.com/esp32-s3-lcd-7b.htm?sku=31726)

- **8× M3 heat-set inserts**
- **12× M3 small screws**

- **MMWave Sensor**  
  Uses 3.3V, GND, and OT2 on the display GPIO.  
  (The display works without this sensor. If not used, go into the settings and disable the motion sensor option.)
  [Example Link](https://de.aliexpress.com/item/1005007103251173.html?gatewayAdapt=glo2deu)

- **USB-C Power Plug**  
  Buy the **6-pin version** so it works regardless of USB-C cable orientation.  
  The display is very sensitive to power issues.
  [Example Link](https://de.aliexpress.com/item/1005007524400372.html?gatewayAdapt=glo2deu)

- **Fabric Covering**  
  Fabric is glued and stretched around the case to give it a finished look.  
  The fabric used here was from an old polo shirt.  
  Glue used: *Tesa permanent spray glue*.

  I recommend using a stretchable fabric — this step was very difficult.

---

## Case Notes

The case can be 3D printed, but the design is **not ideal**. This was one of the first models I ever made.

To screw the display into the case, you will need a **bendable screwdriver attachment** (such as one from an iFixit kit), as access is very tight.

If anyone wants to redesign the case, please PM me for the CAD files.


---

## More pictures

![Solar Display example](/ReadmeImages/solardisplay2.jpg)
![Solar Display example](/ReadmeImages/solardisplay3.jpg)
![Solar Display example](/ReadmeImages/solardisplay4.jpg)
![Solar Display example](/ReadmeImages/solardisplay5.jpg)
![Solar Display example](/ReadmeImages/solardisplay6.jpg)
![Solar Display example](/ReadmeImages/solardisplay7.jpg)
![Solar Display example](/ReadmeImages/solardisplay8.jpg)
![Solar Display example](/ReadmeImages/solardisplay9.jpg)
![Solar Display example](/ReadmeImages/solardisplay10.jpg)
![Solar Display example](/ReadmeImages/solardisplay11.jpg)
![Solar Display example](/ReadmeImages/solardisplay12.jpg)