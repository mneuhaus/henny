#!/usr/bin/env python3
import serial
import time
import sys

def test_connection():
    try:
        ser = serial.Serial('/dev/cu.usbmodem31101', 115200, timeout=2)
        print("Connected to XIAO ESP32-S3")
        
        # Send Ctrl+C to interrupt any running program
        ser.write(b'\x03')
        time.sleep(0.1)
        
        # Send Ctrl+B to exit raw REPL
        ser.write(b'\x02')
        time.sleep(0.1)
        
        # Clear any pending data
        ser.flushInput()
        
        # Send a simple command
        ser.write(b'print("Hello from Henny!")\r\n')
        time.sleep(0.5)
        
        response = ser.read_all().decode('utf-8', errors='ignore')
        print("Response from device:")
        print(response)
        
        if 'Hello from Henny!' in response or '>>>' in response:
            print("✅ MicroPython is running and responding!")
            return True
        else:
            print("❌ Device not responding to Python commands")
            return False
            
    except Exception as e:
        print(f"❌ Connection failed: {e}")
        return False
    finally:
        try:
            ser.close()
        except:
            pass

if __name__ == "__main__":
    test_connection()