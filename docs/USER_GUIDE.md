# User Guide & Simulation Manual

## 1. Quick Start with Wokwi Online Simulator
1. Open the project URL: [https://wokwi.com/projects/478059293315011585](https://wokwi.com/projects/478059293315011585)
2. Click the green **Play** button at the top of the simulation canvas.
3. Watch the virtual LCD 2004 screen and open the **Serial Monitor** tab (set to 115200 baud).

## 2. Interactive Fault Injection
Click on any appliance chip on the canvas to open its control popup:
* **Fault = 0:** Healthy baseline operation.
* **Fault = 1:** Valve leak or continuous compressor run (extended runtime).
* **Fault = 2:** Heating element scaling or degradation (longer cycle durations).
* **Fault = 3:** Mechanical bearing friction (elevated current and starting surge).

## 3. Serial CLI Command Set
You can send real-time configuration commands to the Arduino Mega via the Serial Monitor:

* `L <watts>`: Configure peak load ceiling (e.g. `L 3000` sets limit to 3000 W).
* `B <kWh>`: Set the 48-hour energy budget (e.g. `B 35` sets budget to 35 kWh).
* `T <rate>`: Configure the electricity tariff in Rupees (e.g. `T 8.50`).
* `S`: Request immediate status report of live demand and settings.
* `H` or `?`: Display command syntax help.

## 4. Multi-Page LCD Display Guide
The LCD screen automatically cycles every 3 seconds across 4 pages:
* **Page 0:** Whole-home instantaneous power, active count, and peak status.
* **Page 1:** High-demand appliances currently operating and their wattage.
* **Page 2:** Accumulated period energy (kWh), rupee cost, and budget projection.
* **Page 3:** Appliance anomaly and drift status summary.
