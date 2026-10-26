# Mathematical Foundations & Numerical Algorithms

## 1. Instantaneous Waveform Sampling Theory
To accurately measure alternating current (AC) power in non-linear and inductive household appliances, scalar multiplication of RMS voltage and current ($S = V_{RMS} \cdot I_{RMS}$) is insufficient. Real power requires computing the average of instantaneous power over an integer number of AC line cycles.

For a 50 Hz mains frequency:
$$T_{\text{cycle}} = \frac{1}{f} = \frac{1}{50\text{ Hz}} = 20\text{ ms} = 20,000\ \mu\text{s}$$

The Arduino Mega samples $N = 40$ discrete equidistant points per cycle:
$$\Delta t = \frac{20,000\ \mu\text{s}}{40} = 500\ \mu\text{s}$$

## 2. Real Power, RMS, and Power Factor Calculations

### 2.1 Instantaneous Voltage & Current Re-centering
Analog sensor readings are converted from 10-bit ADC raw integers ($0 - 1023$) to engineering units:
$$v[k] = (ADC_{V}[k] - ADC_{V,\text{mid}}) \times \left( \frac{5.0}{1023.0} \right) \times K_{\text{ZMPT}}$$
$$i[k] = (ADC_{I}[k] - ADC_{I,\text{mid}}) \times \left( \frac{5.0}{1023.0} \right) \times \frac{1}{\text{Sensitivity}}$$

### 2.2 Active (Real) Power
$$\text{Real Power } P = \frac{1}{N} \sum_{k=0}^{N-1} v[k] \cdot i[k]$$

### 2.3 Root-Mean-Square (RMS) Values
$$I_{RMS} = \sqrt{\frac{1}{N} \sum_{k=0}^{N-1} i[k]^2}$$
$$V_{RMS} = \sqrt{\frac{1}{N} \sum_{k=0}^{N-1} v[k]^2}$$

### 2.4 Apparent Power and Displacement Power Factor
$$\text{Apparent Power } S = V_{RMS} \times I_{RMS}$$
$$\text{Power Factor } PF = \frac{P}{S} = \frac{P}{V_{RMS} \cdot I_{RMS}}$$

## 3. Energy Integration & Usage Rate Forecasting

### 3.1 Energy Accumulation
$$\text{Energy (kWh)} = \frac{1}{1000 \times 3600} \sum_{t} P(t) \cdot \Delta t$$

### 3.2 Usage-Based Rate Forecasting
Given an elapsed period duration $T_{\text{elapsed}}$ and a target reporting window $T_{\text{period}} = 48\text{ hours}$:
$$E_{\text{projected}} = E_{\text{consumed}} \times \left( \frac{T_{\text{period}}}{T_{\text{elapsed}}} \right)$$

If $E_{\text{projected}} > \text{Budget}$, the projected overrun timestamp $t_{\text{overrun}}$ is evaluated:
$$t_{\text{overrun}} = \text{Budget} \times \left( \frac{T_{\text{elapsed}}}{E_{\text{consumed}}} \right)$$
