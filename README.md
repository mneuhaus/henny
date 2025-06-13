# Henny - Smart Chicken Feeder v2.0

Intelligent ESP32-S3 based automatic chicken feeder with modern web interface, configurable schedules, and over-the-air updates.

## Key Features

- **Configurable Feeding**: 1-8 times per day with sunrise/sunset awareness
- **Mobile Web Interface**: Responsive design with real-time status
- **OTA Updates**: Wireless firmware deployment via web or command line
- **Smart Scheduling**: Dynamic timing based on daylight hours
- **Feed Tracking**: Daily/monthly consumption monitoring
- **Safety Features**: Motor timeouts, error handling, persistent storage

## Quick Start

### Installation
```bash
# USB upload
make upload monitor

# Wireless deployment (after setup)
make ip                    # Find device
make flash IP=192.168.1.100
```

### Setup
1. Connect to "Henny-Setup" WiFi (password: hennyfeeder)
2. Navigate to http://192.168.4.1
3. Configure WiFi, feeding settings, and calibration

## Hardware

**Components:**
- ESP32-S3 XIAO Seeed
- 5V Relay Module
- 12V/24V DC Motor + Spreader
- Push Button + Power Supply

**Connections:**
```
GPIO 1  → Relay Signal
GPIO 2  → Push Button  
GPIO 48 → Built-in LED
```

## Configuration

### Web Interface Settings
- **Chickens**: 0-30 count, 80-200g per day
- **Schedule**: 1-8 feedings, sunrise/sunset offsets (1-4h)
- **System**: WiFi, timezone, calibration, OTA updates

### Make Commands
```bash
make help              # Show all commands
make upload            # USB upload
make flash IP=x        # Wireless upload
make ip                # Find devices
make monitor           # Serial console
```

## API Endpoints

- `GET /` - Dashboard
- `GET /test-motor` - 3s motor test  
- `GET /calibrate` - 10s calibration
- `POST /config` - Update settings
- `GET /update` - Firmware upload interface

## Troubleshooting

**Network Issues:**
```bash
make ip  # Check connectivity
# Hold button during boot for AP mode reset
```

**Motor/Feeding:**
- Use web calibration tools
- Check power supply voltage
- Verify mechanical operation

**Updates:**
- Web: `/update` interface
- CLI: `make flash IP=device_ip`
- Recovery: USB upload if wireless fails

## Security

- Password-protected OTA (`hennyfeeder`)
- Local network only (no internet required)
- WPA2/WPA3 WiFi encryption
- Physical emergency stop button

## Project Structure

```
├── src/main.cpp           # Complete application
├── platformio.ini         # Build config with OTA
├── Makefile              # Deployment automation
└── design-test.html      # UI development
```

## Contributing

Open source project for the chicken-keeping community. Areas for contribution:
- Hardware improvements and 3D models
- Software features and UI enhancements  
- Testing and reliability improvements

Built with C++/Arduino, PlatformIO, and modern web technologies.

---

**Henny v2.0 - Intelligent, configurable, and connected chicken feeding!**