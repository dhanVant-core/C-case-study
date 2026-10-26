import time
import sys

def stream_telemetry():
    print("Initializing Virtual Home Telemetry Stream (Arduino Mega 2560 @ 115200 baud)...")
    print("Sensing: 9 Appliance ACS712 channels + 1 Mains ZMPT101B voltage channel")
    print("Time Scale: 1440 (1 minute real = 24 hours virtual home time)\n")
    
    time.sleep(0.5)
    print("LIVE DEMAND: Fridge 180W | AC 1420W | Dryer 2200W --> TOTAL 3800W")
    print("========== SMART PEAK LOAD ALERT ==========")
    print("Aggregate real power: 3800 W | configured ceiling: 2500 W")
    print("SUGGEST POSTPONING: Clothes dryer | current draw 2200 W | demand if postponed ~1600 W")
    print("Illustrative energy/cost for 1 hour postponed: 2.20 kWh / Rs 17.60")
    print("===========================================\n")
    time.sleep(0.5)
    print("[48h forecast] Current 18.50 kWh; projected 37.00 kWh; budget 30.00 kWh -> OVER BUDGET\n")
    print("Telemetry stream finished.")

if __name__ == '__main__':
    stream_telemetry()
