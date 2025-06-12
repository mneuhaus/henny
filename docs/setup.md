# Henny Smart Chicken Feeder - Setup Guide

## Hardware Requirements

### Components Needed
- ESP32-S3 Development Board
- 5V Relay Module (for motor control)
- 12V/24V DC Motor with spreader attachment
- 12V/24V Power Supply (match motor voltage)
- 5V Buck Converter (for relay/ESP32)
- Push Button (normally open)
- LED + 330Ω Resistor
- Flyback Diode (1N4007 or equivalent)
- Jumper wires and breadboard/PCB
- Optional: Small OLED display (SSD1306)

### Circuit Connections

#### ESP32-S3 Connections
```
ESP32-S3 Pin  →  Component
GPIO 5        →  Relay Module Signal
GPIO 2        →  Status LED (+ resistor to GND)
GPIO 0        →  Push Button (other end to GND)
3.3V          →  Relay Module VCC
GND           →  Common Ground
```

#### Power Supply
```
12V/24V PSU   →  Motor (+) and Buck Converter Input
Buck Converter →  5V output to Relay Module VCC
ESP32-S3      →  USB or separate 3.3V supply
Motor (-)     →  Relay NO contact
Relay COM     →  Power Supply (-)
```

#### Safety Components
- Flyback diode across motor terminals (cathode to +)
- Fuse in motor power line (recommended)

## Software Installation

### 1. Install Development Tools

```bash
# Install required Python packages
pip install esptool adafruit-ampy

# Download MicroPython firmware for ESP32-S3
wget https://micropython.org/resources/firmware/esp32s3-20240222-v1.22.2.bin
```

### 2. Prepare ESP32-S3

```bash
# Clone Henny repository
git clone <repository-url>
cd henny

# Erase ESP32 flash
make erase

# Flash MicroPython firmware
make flash-firmware

# Upload Henny code
make upload
```

### 3. First Time Setup

1. **Power on the ESP32-S3**
   - Status LED should blink during boot
   - Device will create "Henny-Setup" WiFi network

2. **Connect to Setup Network**
   - WiFi: `Henny-Setup`
   - Password: `hennyfeeder`

3. **Configure via Web Interface**
   - Navigate to `http://192.168.4.1`
   - Set your WiFi credentials
   - Configure number of chickens
   - Save configuration

4. **Calibrate Spreader**
   - Click "Run Calibration (10s)"
   - Measure spread amount in grams
   - Enter measurement and save

## Initial Configuration

### Basic Settings
- **Adult Chickens**: Number of full-grown chickens
- **Base Feed per Adult**: Grams per day (typically 100-150g)
- **Season Factor**: Enable automatic seasonal adjustments

### Chick Management
- Add groups of chicks with birth dates
- System calculates age-appropriate feed amounts
- Remove groups when chicks mature to adults

### Feeding Schedule
- Automatic based on season:
  - Summer: 4 feedings (6AM, 10AM, 3PM, 7PM)
  - Winter: 3 feedings (8AM, 1PM, 5PM)
  - Spring/Autumn: 3 feedings (7AM, 12PM, 5PM)

## Hardware Assembly

### 1. Motor and Spreader
- Mount motor securely above feed area
- Attach circular spreader disc to motor shaft
- Ensure 360° clearance for feed spread
- Mount 2-3 feet above ground for optimal spread

### 2. Feed Hopper
- Position hopper above motor/spreader
- Ensure gravity feed to spreader
- Add weatherproof lid
- Capacity: 5-10kg recommended

### 3. Electronics Enclosure
- Weatherproof enclosure for ESP32 and relay
- Mount near motor but protect from moisture
- External access for push button
- Status LED visible from outside

### 4. Power Installation
- Use appropriate gauge wire for motor current
- Secure all connections
- Add inline fuse for motor circuit
- Strain relief for all cables

## Testing

### 1. Basic Function Test
```bash
# Connect to serial console
make serial

# Test commands in REPL:
>>> from lib.spreader import Spreader
>>> from lib.config import Config
>>> config = Config()
>>> spreader = Spreader(config)
>>> spreader.calibrate_run(5)  # 5 second test
```

### 2. Web Interface Test
- Access web interface
- Test manual feed buttons
- Verify status updates
- Check configuration saving

### 3. Schedule Test
- Set system time correctly
- Wait for scheduled feeding time
- Verify automatic operation

## Troubleshooting

### WiFi Issues
- Check SSID/password in config
- Verify signal strength
- Reset to AP mode if needed

### Motor Not Running
- Check relay wiring
- Verify power supply voltage
- Test relay with multimeter
- Check motor connections

### Inaccurate Feed Amounts
- Re-run calibration procedure
- Check for motor binding
- Verify consistent feed flow
- Clean spreader mechanism

### Web Interface Not Loading
- Check IP address
- Verify ESP32 is connected to network
- Try AP mode for configuration

## Safety Considerations

### Electrical Safety
- Use appropriate fuses
- GFCI protection recommended
- Proper grounding
- Weather protection

### Mechanical Safety
- Secure motor mounting
- Guard moving parts
- Smooth spreader edges
- Regular maintenance checks

### Operational Safety
- Monitor initial operation
- Check feed flow regularly
- Backup feeding method available
- Emergency stop accessible

## Maintenance

### Daily
- Check feed hopper level
- Verify status LED operation
- Monitor chicken behavior

### Weekly
- Clean spreader mechanism
- Check all connections
- Verify timing accuracy
- Test manual controls

### Monthly
- Inspect weatherproofing
- Check motor operation
- Verify calibration accuracy
- Update software if needed

## Support

For issues or questions:
1. Check this documentation
2. Review calibration procedures
3. Check hardware connections
4. Test with manual controls
5. Monitor serial console for errors