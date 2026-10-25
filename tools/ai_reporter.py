import json
import sys

SAMPLE_REPORT_DATA = {
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
        {"id": "FRIDGE_01", "name": "Refrigerator", "energy_kwh": 4.21, "cost_inr": 33.68, "run_h": 24.2, "starts": 48, "drift_pct": 2.1, "status": "NORMAL"},
        {"id": "MICRO_01",  "name": "Microwave",    "energy_kwh": 1.12, "cost_inr": 8.96,  "run_h": 0.45, "starts": 8,  "drift_pct": -1.5, "status": "NORMAL"},
        {"id": "DISH_01",   "name": "Dishwasher",   "energy_kwh": 2.85, "cost_inr": 22.80, "run_h": 1.80, "starts": 2,  "drift_pct": 3.4, "status": "NORMAL"},
        {"id": "TV_01",     "name": "Television",   "energy_kwh": 0.94, "cost_inr": 7.52,  "run_h": 8.20, "starts": 4,  "drift_pct": 0.8, "status": "NORMAL"},
        {"id": "FAN_01",    "name": "Ceiling Fan",  "energy_kwh": 0.62, "cost_inr": 4.96,  "run_h": 16.40, "starts": 3, "drift_pct": 1.2, "status": "NORMAL"},
        {"id": "AC_01",     "name": "Air Conditioner", "energy_kwh": 18.40, "cost_inr": 147.20, "run_h": 14.10, "starts": 7, "drift_pct": 34.2, "status": "ATTENTION"},
        {"id": "WASH_01",   "name": "Washing Machine", "energy_kwh": 1.65, "cost_inr": 13.20, "run_h": 1.20, "starts": 1, "drift_pct": -0.5, "status": "NORMAL"},
        {"id": "VAC_01",    "name": "Vacuum Cleaner",  "energy_kwh": 0.80, "cost_inr": 6.40,  "run_h": 0.50, "starts": 2, "drift_pct": 4.0, "status": "NORMAL"},
        {"id": "DRYER_01",  "name": "Clothes Dryer",   "energy_kwh": 6.20, "cost_inr": 49.60, "run_h": 2.10, "starts": 2, "drift_pct": 5.1, "status": "NORMAL"}
    ]
}

def generate_ai_report(data):
    wh = data["whole_home"]
    out = []
    out.append("=======================================================================")
    out.append("           AI-ASSISTED TWO-DAY LOAD PROFILING & ENERGY REPORT         ")
    out.append("=======================================================================")
    out.append(f"Period Duration   : {data['elapsed_hours']:.1f} Hours")
    out.append(f"Electricity Tariff: Rs {data['tariff_rate_inr']:.2f} / kWh")
    out.append(f"Configured Budget : {data['budget_kwh']:.2f} kWh")
    out.append(f"Total Consumed    : {wh['total_energy_kwh']:.2f} kWh  --> Budget {wh['budget_status']}")
    out.append(f"Estimated Cost    : Rs {wh['total_cost_inr']:.2f}")
    out.append(f"Peak Demand Draw  : {wh['peak_demand_w']:.0f} W (Ceiling: {wh['ceiling_limit_w']:.0f} W)")
    out.append(f"Threshold Breaches: {wh['alert_count']} episodes ({wh['over_limit_seconds'] // 60} mins above ceiling)")
    out.append("-----------------------------------------------------------------------")
    out.append(f"{'Appliance':<18} {'Energy(kWh)':<12} {'Cost(Rs)':<10} {'Run(h)':<8} {'Starts':<8} {'Drift %':<10} {'Status'}")
    out.append("-----------------------------------------------------------------------")
    for app in data["appliances"]:
        out.append(f"{app['name']:<18} {app['energy_kwh']:<12.2f} {app['cost_inr']:<10.2f} {app['run_h']:<8.2f} {app['starts']:<8} {app['drift_pct']:<+10.1f} {app['status']}")
    out.append("=======================================================================")
    out.append("                      INTELLIGENT DECISION SUPPORT                     ")
    out.append("=======================================================================")
    
    # Anomaly checks
    attention_items = [a for a in data["appliances"] if a["status"] == "ATTENTION"]
    if attention_items:
        for item in attention_items:
            out.append(f"[*] ATTENTION REQUIRED: {item['name']}")
            out.append(f"    - Energy consumption surged by {item['drift_pct']:+.1f}% above healthy baseline.")
            out.append(f"    - Recommended Action: Check air filters, door seals, or mechanical friction.")
    else:
        out.append("[+] All monitored appliances are operating within normal baseline boundaries.")
        
    if wh["budget_status"] == "EXCEEDED":
        out.append("[*] BUDGET MITIGATION ADVICE:")
        out.append("    - Household demand exceeded target 48h budget by "
                   f"{wh['total_energy_kwh'] - data['budget_kwh']:.2f} kWh.")
        out.append("    - Postpone high-power flexible loads (Clothes Dryer / Dishwasher) to non-peak hours.")
        
    out.append("=======================================================================")
    return "\n".join(out)

if __name__ == '__main__':
    report = generate_ai_report(SAMPLE_REPORT_DATA)
    print(report)
