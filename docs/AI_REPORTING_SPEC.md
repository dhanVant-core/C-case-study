# AI Reporting Specification & Telemetry Protocol

## 1. Overview
The **Two-Day AI Reporting Workflow** bridges embedded real-time telemetry with high-level cognitive decision support. The embedded Arduino Mega maintains running metrics over a 48-hour window (or compressed simulation periods). At the conclusion of each period, a structured telemetry frame is emitted over UART to the Python AI reporting tool (`tools/ai_reporter.py`).

## 2. Telemetry Packet Schema

```json
{
  "period_id": 1,
  "elapsed_hours": 48.0,
  "tariff_rate_inr": 8.0,
  "budget_kwh": 30.0,
  "whole_home": {
    "total_energy_kwh": 36.79,
    "total_cost_inr": 294.32,
    "peak_demand_w": 3820.0,
    "ceiling_limit_w": 2500.0,
    "over_limit_seconds": 1840,
    "alert_count": 4,
    "budget_status": "EXCEEDED"
  },
  "appliances": [
    {
      "id": "FRIDGE_01",
      "name": "Refrigerator",
      "energy_kwh": 4.21,
      "cost_inr": 33.68,
      "run_hours": 24.2,
      "start_cycles": 48,
      "peak_current_a": 1.15,
      "avg_pf": 0.82,
      "drift_percent": 2.1,
      "status": "NORMAL"
    },
    {
      "id": "AC_01",
      "name": "Air Conditioner",
      "energy_kwh": 18.40,
      "cost_inr": 147.20,
      "run_hours": 14.1,
      "start_cycles": 7,
      "peak_current_a": 8.90,
      "avg_pf": 0.91,
      "drift_percent": 34.2,
      "status": "ATTENTION"
    }
  ]
}
```

## 3. Cognitive Recommendation Rules
1. **Peak Load Reduction:** Stagger heavy heating/cooling loads (e.g., delay Clothes Dryer while AC is active).
2. **Compressor Door Seal Alert:** If Refrigerator run hours exceed baseline by $> 30\%$, prompt occupant to inspect door magnetic gaskets.
3. **Filter Clog Warning:** If AC energy increases while cooling cycles elongate, advise air filter inspection.
