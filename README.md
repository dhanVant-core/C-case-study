# AI-Based Appliance Load Profiling and Energy Monitoring

[![Wokwi Simulation](https://img.shields.io/badge/Wokwi-Simulation%20Live-brightgreen?logo=arduino)](https://wokwi.com/projects/478059293315011585)
[![Hardware](https://img.shields.io/badge/Microcontroller-Arduino%20Mega%202560-00979D?logo=arduino)](https://docs.arduino.cc/hardware/mega-2560)
[![Sensing](https://img.shields.io/badge/Sensors-ACS712%20%2B%20ZMPT101B-blue)](#sensor-mapping)
[![C/C++ Standard](https://img.shields.io/badge/Language-C%2FC%2B%2B%20%7C%20Python-orange)](#system-architecture)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

> **Simulation-based sensing • Real-power analysis • Intelligent energy decisions**

---

## 🔗 Project Links

* **Live Interactive Simulation (Wokwi):** [https://wokwi.com/projects/478059293315011585](https://wokwi.com/projects/478059293315011585)
* **GitHub Repository:** [https://github.com/dhanVant-core/C-case-study](https://github.com/dhanVant-core/C-case-study)

---

## 👥 Project Team (Team 9)

| Name | Registration Number | Official Email | Role & Core Responsibilities | Commits |
| :--- | :---: | :--- | :--- | :---: |
| **Dhanvant Menon** | `CB.SC.U4CSE25613` | `dhanvantmenon@gmail.com` | **Lead & System Architecture** — Dual-buffer sampling, real power calculation, baseline learning & drift analysis | **14** |
| **P. Shasank** | `CB.SC.U4CSE25645` | `paruchurishasank04@gmail.com` | **Sensor & Physics Modeling** — Custom C Wokwi chip (`appliance.chip.c`), harmonics (h3, h5), fault injection | **12** |
| **Gowshik Raam** | `CB.SC.U4CSE25622` | `rtgraam06@gmail.com` | **Demand Management & Analytics** — Peak alerts, load postponement engine, rate-based energy budget forecasting | **12** |
| **Ramakkrishnayya BV** | `CB.SC.U4CSE25608` | `bvram8033@gmail.com` | **Hardware Integration & UI** — Wokwi schematic wiring (`diagram.json`), cyclic 20x4 I2C LCD state machine, ₹ cost engine | **12** |
| **TOTAL** | — | — | **End-to-End Smart Energy Management Platform** | **50** |

---

## 📌 Executive Summary & Problem Statement

Conventional single-point utility meters and monthly electricity bills report only a single aggregated kilowatt-hour (kWh) value. This obscures critical appliance-level consumption dynamics:
1. **Hidden Consumption:** High standby loads and prolonged appliance cycles remain unnoticed until the monthly bill arrives.
2. **Early Degradation Warning:** Mechanical wear (compressor valve leak, motor friction) or electrical faults (heating element scaling) cause insidious current drift and cycling changes long before complete failure.
3. **Peak Demand Charges:** Concurrent operation of heavy discretionary loads (dryer, air conditioner, dishwasher) triggers excessive peak demands.

### Solution Overview
This project designs and implements a **Virtual Home Energy Management & Monitoring System** using an **Arduino Mega 2560** simulated in **Wokwi**:
* **High-Fidelity Multi-Channel Sensing:** Dedicated ACS712 Hall-effect current sensors across 9 household appliances plus a ZMPT101B mains voltage transformer channel.
* **Instantaneous Waveform Sampling:** 40 samples per 20 ms AC cycle (50 Hz mains) computing true **Real Power ($P$)**, **RMS Current ($I_{RMS}$)**, and **Power Factor ($PF$)**.
* **Intelligent Peak-Load Alerting:** Real-time demand monitoring against a configurable threshold (e.g., 2.5 kW / 4.0 kW) with automated discretionary load postponement recommendations.
* **Usage-Based Energy Budget Forecasting:** Linear rate-based projection predicting budget exceedance days before the billing cycle completes.
* **Appliance-Level Costing in Rupees (₹):** Transparent per-appliance and period cost estimation using configurable tariffs (₹8 / kWh default).
* **Two-Day AI Reporting Workflow:** Statistical baseline establishment and automated 48-hour anomaly detection flagging electrical and mechanical drift.

---

## 🏗️ System Architecture

```
+-----------------------------------------------------------------------------------------+
|                                    VIRTUAL HOME                                         |
|                                                                                         |
|   +--------------------+     +---------------------+     +--------------------------+   |
|   | Kitchen Loads      |     | Living Room Loads   |     | Bedroom & Utility Loads  |   |
|   | - Fridge   (A0)    |     | - TV        (A3)    |     | - AC           (A5)      |   |
|   | - Microwave (A1)   |     | - Fan       (A4)    |     | - Washing Mach (A6)      |   |
|   | - Dishwash (A2)    |     | - Vacuum    (A7)    |     | - Clothes Dryer(A8)      |   |
|   +---------+----------+     +----------+----------+     +------------+-------------+   |
|             |                           |                             |                 |
|             +---------------------------+-----------------------------+                 |
|                                         | 9 x ACS712 Analog Out                         |
|   +-------------------------------+     |                                               |
|   | Mains Voltage Sensor          |     |                                               |
|   | - ZMPT101B Sensor (A9)        |-----+                                               |
+---|-------------------------------|-----------------------------------------------------+
    |                               v
+---|-------------------------------------------------------------------------------------+
|   |                  ARDUINO MEGA 2560 (EMBEDDED FIRMWARE)                              |
|   |                                                                                     |
|   |  1. Fast ADC Sampling: 40 samples / 20 ms cycle (50 Hz)                             |
|   |  2. Real Power Engine: P = (1/N) * sum(v[k] * i[k])                                 |
|   |  3. Root-Mean-Square:  I_RMS = sqrt((1/N) * sum(i[k]^2))                            |
|   |  4. Power Factor:      PF = P / (V_RMS * I_RMS)                                     |
|   |  5. Energy Accumulator: kWh = sum(P * dt) / (1000 * 3600)                           |
|   |  6. Baseline Memory:   Tracks 48-hour baseline energy, runtime, & starts            |
|   +-------------------+-----------------------------+-------------------------------+---+
                        |                             |
                        v                             v
        +-------------------------------+   +------------------------------------+
        |  SMART FEATURES ENGINE        |   |  MULTI-PAGE I2C LCD (20x4)         |
        |  (smart_features.h)           |   |                                    |
        |  - Peak Alert (Total > Limit) |   |  Page 0: Whole-home kW & Peak Stat |
        |  - Postponement Advisor       |   |  Page 1: Active Appliance Breakdown|
        |  - 48h Budget Forecast        |   |  Page 2: Period kWh & Cost in Rs   |
        |  - Rs 8/kWh Tariff Engine     |   |  Page 3: Anomaly & Drift Status    |
        +---------------+---------------+   +------------------------------------+
                        |
                        v
        +------------------------------------------------------------------------+
        |                 AI-ASSISTED ANALYTICS & TWO-DAY REPORTING              |
        |  Serial Telemetry -> Python Analytics Engine (`tools/ai_reporter.py`)  |
        |  - Historical Drift % = ((Current - Baseline) / Baseline) * 100        |
        |  - Classification: NORMAL | HIGH USAGE | ATTENTION                     |
        |  - Actionable load shifting & physical inspection guidance             |
        +------------------------------------------------------------------------+
```

---

## 🔌 Sensor Mapping & Pin Connections

| Arduino Pin | Channel ID | Monitored Appliance | Sensor Model | Sensitivity | Rated Current | Run Threshold | Heavy Threshold | Room |
| :---: | :---: | :--- | :--- | :---: | :---: | :---: | :---: | :--- |
| **A0** | `FRIDGE_01` | Refrigerator | ACS712-20A | 100 mV/A | 0.9 A | 0.30 A | — | Kitchen |
| **A1** | `MICRO_01` | Microwave | ACS712-20A | 100 mV/A | 5.3 A | 1.00 A | — | Kitchen |
| **A2** | `DISH_01` | Dishwasher | ACS712-20A | 100 mV/A | 9.3 A | 0.20 A | 4.00 A | Kitchen |
| **A3** | `TV_01` | Television | ACS712-5A | 185 mV/A | 0.5 A | 0.20 A | — | Living Room |
| **A4** | `FAN_01` | Ceiling Fan | ACS712-5A | 185 mV/A | 0.3 A | 0.08 A | — | Living Room |
| **A5** | `AC_01` | Air Conditioner | ACS712-30A | 66 mV/A | 6.2 A | 1.50 A | — | Bedroom |
| **A6** | `WASH_01` | Washing Machine | ACS712-20A | 100 mV/A | 8.4 A | 0.15 A | 4.00 A | Utility |
| **A7** | `VAC_01` | Vacuum Cleaner | ACS712-20A | 100 mV/A | 6.0 A | 1.00 A | — | Living Room |
| **A8** | `DRYER_01` | Clothes Dryer | ACS712-20A | 100 mV/A | 10.5 A | 0.30 A | 7.00 A | Utility |
| **A9** | `MAINS_V` | Mains Voltage | ZMPT101B | 325.27 V pk | 230 V RMS | — | — | Main Board |
| **Pin 20** | `SDA` | 2004 LCD Display | I2C (Address 0x27) | — | — | — | — | Dashboard |
| **Pin 21** | `SCL` | 2004 LCD Display | I2C (Address 0x27) | — | — | — | — | Dashboard |

---

## 📐 Mathematical Foundations

### 1. True Real Power vs. Apparent Power Approximation
In Review 2, power was estimated using scalar multiplication:
$$S = V_{RMS} \\times I_{RMS}$$
This approximation fails for inductive loads (compressors, washing machine motors, fan induction coils) because it ignores the phase shift $\\phi$ and harmonic distortions.

The updated system samples instantaneous voltage $v[k]$ and current $i[k]$ simultaneously across $N = 40$ points per mains cycle:
$$\\text{Real Power } P = \\frac{1}{N} \\sum_{k=0}^{N-1} v[k] \\cdot i[k]$$

$$\\text{RMS Current } I_{RMS} = \\sqrt{\\frac{1}{N} \\sum_{k=0}^{N-1} i[k]^2}$$

$$\\text{RMS Voltage } V_{RMS} = \\sqrt{\\frac{1}{N} \\sum_{k=0}^{N-1} v[k]^2}$$

$$\\text{Displacement Power Factor } PF = \\frac{P}{V_{RMS} \\cdot I_{RMS}}$$

### 2. Energy Accumulation & Cost Calculation
Active energy consumed over period $T$ is computed by numerical integration:
$$\\text{Energy (kWh)} = \\frac{1}{1000} \\sum_{t} P(t) \\cdot \\Delta t$$
$$\\text{Estimated Cost (\\u20b9)} = \\text{Energy (kWh)} \\times \\text{Tariff (\\u20b9/kWh)}$$
*(Default tariff: \\u20b98.00 / kWh).*

### 3. Usage-Based Energy Budget Forecasting
For a simulated billing cycle of $T_{\\text{period}} = 48\\text{ hours}$ (or 30 days):
$$\\text{Projected Consumption } (\\text{kWh}) = \\text{Energy Used So Far} \\times \\left( \\frac{T_{\\text{period}}}{T_{\\text{elapsed}}} \\right)$$
If $\\text{Projected Consumption} > \\text{Configured Budget}$, an alert is logged indicating the estimated day/hour of budget exhaustion.

---

## ⚡ Key Intelligent Improvements

### 1. Intelligent Peak-Load Alerts & Load Postponement
* **Scenario:** Refrigerator (180 W) + Air Conditioner (1400 W) + Clothes Dryer (2200 W) running simultaneously produces a peak demand of **3.78 kW**, violating a 2.5 kW / 4.0 kW limit.
* **Action:** The system scans active deferrable loads (`smCandidates[] = {Dryer, Dishwasher, Washer, Vacuum, Microwave, AC}`).
* **Preservation Rule:** The Refrigerator is **strictly excluded** from postponement to prevent food spoilage.
* **Advisory:** Suggests postponing the Clothes Dryer to immediately drop load by 2.2 kW while presenting estimated savings/deferrals in rupees.

### 2. Time Compression Scaling
To validate two-day (48-hour) cycles inside Wokwi without waiting days:
* **Time Scale:** `TIME_SCALE 1440`
* **Ratio:** 1 minute of real time = 1 simulated home day (24 hours).
* A full **two-day baseline evaluation completes in ~2 minutes**.

### 3. Interactive Fault Injection
The custom Wokwi C chip (`appliance.chip.c`) provides interactive sliders during live simulation:
* `Fault 0`: **Healthy baseline operation**.
* `Fault 1`: **Compressor valve leakage / continuous run** (extended duty cycles).
* `Fault 2`: **Heater scaling / element degradation** (reduced thermal efficiency, prolonged cycles).
* `Fault 3`: **Mechanical bearing friction / rotor drag** (high startup surge and elevated running current).

---

## 📊 Two-Day AI Reporting & Anomaly Classification

Every 48 simulated hours, the embedded firmware compiles a structured telemetry summary:
```
==================== 48-HOUR AI LOAD PROFILING REPORT ====================
Appliance         Energy(kWh)  Cost(Rs)   Run(h)   Starts   Status
--------------------------------------------------------------------------
Refrigerator            4.21      33.68    24.20       48   NORMAL
Microwave               1.12       8.96     0.45        8   NORMAL
Dishwasher              2.85      22.80     1.80        2   NORMAL
Television              0.94       7.52     8.20        4   NORMAL
Ceiling Fan             0.62       4.96    16.40        3   NORMAL
Air Conditioner        18.40     147.20    14.10        7   ATTENTION (+34% drift)
Washing Machine         1.65      13.20     1.20        1   NORMAL
Vacuum Cleaner          0.80       6.40     0.50        2   NORMAL
Clothes Dryer           6.20      49.60     2.10        2   NORMAL
--------------------------------------------------------------------------
WHOLE HOME TOTAL:      36.79     294.32    68.95       76   PEAK: 3820 W
==========================================================================
```

### Anomaly Status Taxonomy:
* **`NORMAL`:** Deviations within $\\pm 10\\%$ of established healthy baseline.
* **`HIGH USAGE`:** Energy or run hours between $+15\\%$ and $+25\\%$ (higher occupant usage).
* **`ATTENTION / FAULT RISK`:** Deviation $> +25\\%$ or anomalous current spike indicating mechanical or electrical deterioration.

---

## 💻 Serial Command Reference

Open the Wokwi Serial Monitor (115200 baud) to inspect live reports or configure thresholds dynamically:

| Command | Syntax | Description | Example |
| :---: | :--- | :--- | :--- |
| **`H`** or **`?`** | `H` | Display command help and usage syntax | `H` |
| **`S`** | `S` | Print current smart features status and active load | `S` |
| **`L`** | `L <watts>` | Set peak demand ceiling threshold | `L 3500` |
| **`B`** | `B <kWh>` | Set 48-hour energy budget | `B 35.0` |
| **`T`** | `T <tariff>` | Set electricity tariff in Rupees per kWh | `T 8.50` |

---

## 📁 Repository Structure

```
.
├── .github/
│   └── workflows/
│       └── ci.yml                # CI automation workflow
├── docs/
│   ├── SYSTEM_ARCHITECTURE.md    # Detailed hardware and electrical specifications
│   ├── ALGORITHMS_AND_MATH.md    # Waveform sampling and numerical integration math
│   ├── AI_REPORTING_SPEC.md      # Specification for AI telemetry formatting
│   └── USER_GUIDE.md             # Wokwi simulation and serial interaction manual
├── simulation/
│   ├── appliance.chip.c          # Custom Wokwi C chip: multi-appliance + ACS712
│   ├── appliance.chip.json       # Wokwi chip metadata and fault slider controls
│   ├── diagram.json              # Wokwi netlist and schematic layout
│   └── libraries.txt             # Wokwi Arduino library dependencies
├── src/
│   ├── sketch.ino                # Arduino Mega main firmware and sampling loop
│   └── smart_features.h          # Peak alerts, budget forecasting & postponement
├── tools/
│   ├── ai_reporter.py            # Python telemetry parser and AI report generator
│   ├── simulate_telemetry.py     # Fast-time serial telemetry streamer
│   └── test_power_sampling.py    # Unit tests verifying discrete power mathematics
├── diagram.json                  # Root Wokwi schematic for direct simulator import
├── libraries.txt                 # Root library declarations
├── appliance.chip.c              # Root chip source for Wokwi compiler
├── appliance.chip.json           # Root chip descriptor
├── sketch.ino                    # Root firmware for Wokwi compiler
├── smart_features.h              # Root smart demand header
├── Makefile                      # Build and test orchestration
├── LICENSE                       # MIT License
└── README.md                     # Comprehensive project documentation
```

---

## 🚀 Getting Started

### 1. Run in Browser (Zero Installation)
Simply visit [Wokwi Project 478059293315011585](https://wokwi.com/projects/478059293315011585) and click the **Play** button.

### 2. Local Simulation & Verification
Clone the repository and run unit tests for the discrete sampling engine:
```bash
git clone https://github.com/dhanVant-core/C-case-study.git
cd C-case-study
python tools/test_power_sampling.py
```

### 3. Generate Sample AI Telemetry Report
```bash
python tools/ai_reporter.py
```

---



---

## 📑 Formal Case Study Report

A complete academic case study report is published in [`docs/CASE_STUDY_REPORT.md`](docs/CASE_STUDY_REPORT.md), covering:
* **Section 1–5:** Title, Team Members, Scope, and Objectives.
* **Section 6–7:** Review of Related Work (Separate Sensors vs. Single-Sensor NILM vs. Hybrid) and Proposed Sense–Compute–Analyze–Explain Architecture.
* **Section 8–9:** Mathematical formulations, sensor mapping tables, and system boundary specifications.
* **Section 10–11:** Simulation results, validation evidence, and demonstration screenshots.
* **Section 12–14:** Academic conclusions, future research directions, and references.

### Demonstration Evidence & Results

| Figure | Description | Preview |
| :---: | :--- | :---: |
| **Figure 1** | System Architecture Pipeline | ![Figure 1](docs/figures/figure1_system_architecture.png) |
| **Figure 2** | Implementation Method & Boundary | ![Figure 2](docs/figures/figure2_sense_compute_analyze_explain.png) |
| **Figure 3** | Virtual 9-Appliance Home Simulation | ![Figure 3](docs/figures/figure3_wokwi_simulation_live.png) |
| **Figure 4** | Smart Peak-Load Alert Output | ![Figure 4](docs/figures/figure4_peak_load_alert.png) |
| **Figure 5** | Intelligent Energy Management AI Summary | ![Figure 5](docs/figures/figure5_intelligent_energy_management.png) |

## 📜 Academic Integrity & Disclaimer
This project is developed as part of the **C Case Study / Microcontroller System Design Coursework (Team 9)**. All current and voltage waveforms are modeled using physics-based differential equations and stochastic behavior models inside the Wokwi simulation environment. No automatic physical mains switching is performed; recommendations serve as intelligent decision support for home occupants.

*Copyright (c) 2026 Team 9. Released under the MIT License.*
