PORT ?= /dev/cu.usbmodem31101
BAUD ?= 115200

.PHONY: all install build upload monitor clean

all: build upload

install:
	pip install platformio

build:
	pio run

upload:
	pio run -t upload

monitor:
	pio device monitor -b $(BAUD)

clean:
	pio run -t clean
	rm -rf .pio/