PORT ?= /dev/cu.usbmodem31101
BAUD ?= 115200
FIRMWARE ?= ESP32_GENERIC_S3-20241025-v1.24.0.bin

.PHONY: all install-tools erase flash-firmware upload serial clean backup-config restore-config calibrate

all: erase flash-firmware upload

install-tools:
	uv add --dev esptool adafruit-ampy pyserial

erase:
	esptool.py --chip esp32s3 --port $(PORT) erase_flash

flash-firmware:
	esptool.py --chip esp32s3 --port $(PORT) write_flash -z 0x0 $(FIRMWARE)

upload:
	@echo "Soft resetting ESP32..."
	uv run python -c "import serial; s=serial.Serial('$(PORT)', $(BAUD), timeout=1); s.write(b'\x03\x04'); s.close()" || true
	@sleep 3
	ampy --port $(PORT) --baud $(BAUD) put src/boot.py
	ampy --port $(PORT) --baud $(BAUD) put src/main.py
	ampy --port $(PORT) --baud $(BAUD) mkdir lib || true
	ampy --port $(PORT) --baud $(BAUD) put src/lib/config.py lib/config.py
	ampy --port $(PORT) --baud $(BAUD) put src/lib/spreader.py lib/spreader.py
	ampy --port $(PORT) --baud $(BAUD) put src/lib/scheduler.py lib/scheduler.py
	ampy --port $(PORT) --baud $(BAUD) put src/lib/web_server.py lib/web_server.py

serial:
	screen $(PORT) $(BAUD)

soft-reset:
	uv run python -c "import serial; s=serial.Serial('$(PORT)', $(BAUD), timeout=1); s.write(b'\x03\x04'); s.close()"

backup-config:
	ampy --port $(PORT) --baud $(BAUD) get config.json > config.backup.json
	ampy --port $(PORT) --baud $(BAUD) get calibration.json > calibration.backup.json

restore-config:
	ampy --port $(PORT) --baud $(BAUD) put config.backup.json config.json
	ampy --port $(PORT) --baud $(BAUD) put calibration.backup.json calibration.json

calibrate:
	@echo "Starting Henny calibration mode..."
	@echo "Run 'make serial' in another terminal to interact"
	ampy --port $(PORT) --baud $(BAUD) run -n src/tools/calibrate.py

clean:
	find . -name "*.pyc" -delete
	find . -name "__pycache__" -delete
	rm -f *.backup.json