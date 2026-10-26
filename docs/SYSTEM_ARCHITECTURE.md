# System Architecture & Hardware Specification

## 1. Overview
The **AI-Based Appliance Load Profiling and Energy Monitoring System** is engineered around an **Arduino Mega 2560** microcontroller simulated inside the **Wokwi** simulation environment. The system models a virtual multi-room residential home with 9 distinct appliance load channels and a dedicated whole-home mains voltage sensor.

```
                  +----------------------------------------------+
                  |         VIRTUAL RESIDENTIAL DWELLING         |
                  +----------------------------------------------+
                    |           |            |           |
            +-------+    +------+-----+   +--+----+   +--+----------+
            |Kitchen|    |Living Room |   |Bedroom|   |Utility Room |
            +-------+    +------------+   +-------+   +-------------+
            |Fridge |    |Television  |   |Air Con|   |Washing Mach |
            |Microwv|    |Ceiling Fan |   +-------+   |Vacuum Clean |
            |Dishwsh|    +------------+               |Clothes Dryer|
            +-------+                                 +-------------+
                |              |              |              |
                +--------------+--------------+--------------+
                                       |
                   9 x ACS712 Current Sensor Outputs (A0 - A8)
                   1 x ZMPT101B Voltage Transformer (A9)
                                       |
                                       v
                     +-----------------------------------+
                     |         ARDUINO MEGA 2560         |
                     |  - ATmega2560 8-bit MCU @ 16 MHz  |
                     |  - 10-bit Successive Approx ADC   |
                     |  - 40 Samples / 20 ms Cycle       |
                     +-----------------+-----------------+
                                       |
                   +-------------------+-------------------+
                   |                                       |
                   v                                       v
         +--------------------+                 +---------------------+
         | 20x4 I2C LCD (0x27)|                 | USB / UART Telemetry|
         | Live UI Dashboard  |                 | 115200 Baud Stream  |
         +--------------------+                 +----------+----------+
                                                           |
                                                           v
                                                +---------------------+
                                                | Python AI Analytics |
                                                | 48-Hour Report Gen  |
                                                +---------------------+
```

## 2. Hardware Specifications

### 2.1 Microcontroller: Arduino Mega 2560
* **Core:** ATmega2560 operating at 16.0 MHz
* **Flash Memory:** 256 KB (8 KB used by bootloader)
* **SRAM:** 8 KB (optimized dynamic memory allocation for multi-appliance time-series)
* **ADC Resolution:** 10-bit (1024 discrete quantization steps, 4.887 mV/LSB at 5.0 V reference)
* **Sampling Rate:** Continuous dual-buffer acquisition capturing 40 discrete points per 20 ms (50 Hz mains period)

### 2.2 Current Sensing: ACS712 Hall-Effect Sensors
* **Operating Principle:** Fully integrated Hall-effect current sensor IC producing a proportional analog voltage centered at $V_{CC} / 2 = 2.50\text{ V}$.
* **Sensor Allocation:**
  * **ACS712-05B (185 mV/A):** Low-current channels (Television A3, Ceiling Fan A4).
  * **ACS712-20A (100 mV/A):** Medium-current channels (Refrigerator A0, Microwave A1, Dishwasher A2, Washing Machine A6, Vacuum A7, Dryer A8).
  * **ACS712-30A (66 mV/A):** High-current inductive loads (Air Conditioner A5).

### 2.3 Voltage Sensing: ZMPT101B
* **Operating Principle:** High-precision micro voltage transformer with op-amp signal conditioning.
* **Peak Scaling:** 325.27 V peak corresponds to 230 V RMS mains voltage.
* **Analog Pin:** A9 on the Arduino Mega.

### 2.4 User Interface: 20x4 I2C LCD Display
* **Controller:** HD44780 with PCF8574 I2C backpack.
* **I2C Address:** 0x27 (Mega pins 20 for SDA, 21 for SCL).
* **Display Format:** 4 rows of 20 alphanumeric characters cycling through 4 operational pages.
