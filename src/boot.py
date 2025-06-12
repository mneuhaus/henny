# Henny Smart Chicken Feeder - Boot Configuration
# This file runs on every boot (before main.py)

import gc
import time
from machine import freq

# Set CPU frequency for optimal performance/power balance
# 160MHz is a good balance for ESP32-S3
freq(160000000)

# Enable garbage collection
gc.enable()

# Initial memory cleanup
gc.collect()

print("🐔 Henny Boot Configuration")
print(f"CPU Frequency: {freq() // 1000000}MHz")
print(f"Free Memory: {gc.mem_free()} bytes")

# Enable WebREPL for remote access (helps with uploads)
try:
    import webrepl
    webrepl.start()
    print("WebREPL started")
except:
    print("WebREPL not available")

# Delay to ensure hardware is ready
time.sleep_ms(100)

print("Boot configuration complete")