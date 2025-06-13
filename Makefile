PORT ?= /dev/cu.usbmodem31101
BAUD ?= 115200

.PHONY: all install build upload upload-ota flash monitor clean help ip

# Default target
help:
	@echo "Henny Chicken Feeder - Available Commands:"
	@echo ""
	@echo "  build          - Build firmware"
	@echo "  upload         - Upload via USB cable"
	@echo "  upload-ota IP  - Upload via WiFi to IP address"
	@echo "  flash IP       - Build and upload via WiFi"
	@echo "  monitor        - Open serial monitor"
	@echo "  clean          - Clean build files"
	@echo "  ip             - Scan for Henny devices on network"
	@echo ""
	@echo "Examples:"
	@echo "  make upload-ota IP=192.168.1.100"
	@echo "  make flash IP=192.168.1.100"

all: build upload

install:
	pip install platformio

build:
	@echo "Building Henny firmware..."
	pio run

upload:
	@echo "Uploading via USB..."
	pio run -t upload

# Upload via OTA (requires IP parameter)
upload-ota:
	@if [ -z "$(IP)" ]; then \
		echo "Error: IP address required. Usage: make upload-ota IP=192.168.1.100"; \
		exit 1; \
	fi
	@echo "Uploading to $(IP) via OTA..."
	@export HENNY_IP=$(IP) && pio run -e seeed_xiao_esp32s3_ota --target upload

# Build and upload via OTA in one command
flash:
	@if [ -z "$(IP)" ]; then \
		echo "Error: IP address required. Usage: make flash IP=192.168.1.100"; \
		exit 1; \
	fi
	@echo "Building and uploading to $(IP)..."
	@export HENNY_IP=$(IP) && pio run -e seeed_xiao_esp32s3_ota --target upload

monitor:
	@echo "Opening serial monitor (Ctrl+C to exit)..."
	pio device monitor -b $(BAUD)

clean:
	@echo "Cleaning build files..."
	pio run -t clean
	rm -rf .pio/

# Scan for Henny devices on network
ip:
	@echo "Scanning for Henny devices (OTA port 3232)..."
	@nmap -p 3232 192.168.1.0/24 2>/dev/null | grep -B 2 "3232/tcp open" | grep "Nmap scan report" | cut -d' ' -f5 || echo "No devices found. Make sure device is connected to WiFi and OTA is enabled."