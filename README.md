# 🐔 Henny - Smart Chicken Feeder

Henny is an intelligent ESP32-S3 based automatic chicken feeder that spreads feed in a circular pattern using a relay-controlled motor. It automatically dispenses feed based on time schedules, chicken age, and seasonal factors, with a mobile-friendly web interface for configuration and monitoring.

## Features

### Intelligent Feeding
- **Age-based feeding**: Automatic adjustments for chicks of different ages
- **Seasonal scheduling**: More feedings in winter, optimized for each season
- **Accurate portions**: Calibrated spreader ensures precise feed amounts
- **Anti-double-feed**: Prevention of accidental duplicate feedings

### Smart Scheduling
- **Summer schedule**: 4 feedings (6AM, 10AM, 3PM, 7PM)
- **Winter schedule**: 3 feedings (8AM, 1PM, 5PM) 
- **Spring/Autumn**: 3 feedings (7AM, 12PM, 5PM)
- **Automatic timing**: Based on real-time clock with NTP sync

### Web Interface
- **Mobile-friendly**: Responsive design works on phones and tablets
- **Real-time status**: Current time, next feeding, daily progress
- **Configuration**: Chicken counts, feed amounts, schedules
- **Manual controls**: On-demand feeding, calibration tools
- **Chick management**: Add/remove groups with birth dates

### Hardware Control
- **Relay-controlled motor**: Reliable 12V/24V motor operation
- **Safety timeouts**: Maximum 30-second run time prevents motor damage
- **Manual button**: Quick feed (short press) or calibration mode (long press)
- **Status LED**: Visual feedback for system state and activity
- **Circular spread**: Even distribution pattern for multiple chickens

## Quick Start

### Hardware Setup
1. Connect ESP32-S3, relay module, and motor per wiring diagram
2. Mount motor and spreader disc above feeding area
3. Install feed hopper and electronics enclosure

### Software Installation
```bash
# Install tools
make install-tools

# Flash firmware and upload code
make all

# Connect to serial console
make serial
```

### First Configuration
1. Connect to "Henny-Setup" WiFi (password: hennyfeeder)
2. Navigate to http://192.168.4.1
3. Configure WiFi, chicken count, and feed amounts
4. Run calibration procedure

## Project Structure

```
henny/
├── src/
│   ├── main.py              # Main application
│   ├── boot.py              # Boot configuration  
│   └── lib/
│       ├── config.py        # Configuration management
│       ├── spreader.py      # Motor/relay control & calibration
│       ├── scheduler.py     # Feeding schedule logic
│       └── web_server.py    # Web interface
├── docs/
│   ├── setup.md             # Hardware setup instructions
│   └── calibration.md       # Calibration procedures
├── Makefile                 # Build/flash automation
└── config.json.example      # Example configuration
```

## Hardware Requirements

### Core Components
- ESP32-S3 Development Board
- 5V Relay Module  
- 12V/24V DC Motor with spreader attachment
- 12V/24V Power Supply
- Push Button
- Status LED + 330Ω Resistor
- Flyback Diode (1N4007)

### Connections
```
ESP32-S3 Pin  →  Component
GPIO 5        →  Relay Signal
GPIO 2        →  Status LED  
GPIO 0        →  Push Button
3.3V/GND      →  Power/Ground
```

## Key Features in Detail

### Intelligent Feed Calculation
- **Adult chickens**: 120g base amount per day
- **Chicks by age**:
  - 0-7 days: 15g per day
  - 7-21 days: 30g per day  
  - 21-42 days: 50g per day
  - 42-84 days: 80g per day
  - 84+ days: Adult amount (120g)

### Seasonal Adjustments
- **Winter**: +15% feed (cold weather)
- **Spring**: +5% feed (laying season)
- **Summer**: -5% feed (hot weather)
- **Autumn**: Normal amount

### Calibration System
- 10-second calibration runs
- Measure and record actual spread amount
- Automatic calculation of grams per 10 seconds
- History tracking of calibration values
- One-click recalibration via web interface

### Safety Features
- Motor timeout protection (30 seconds max)
- Anti-double-feed logic
- Emergency stop functionality
- Watchdog timer for system reliability
- Power-loss recovery

## Web Interface Highlights

### Dashboard
- Real-time clock display
- Next feeding countdown
- Daily progress indicator
- Total bird count summary

### Configuration
- Slider for feed amounts
- Checkbox for seasonal adjustments
- Chicken and chick group management
- WiFi network settings

### Manual Controls
- Instant feed buttons (25g, 50g)
- Custom amount feeding
- Calibration runner
- Daily counter reset

### Monitoring
- Today's feeding schedule
- Completion status indicators
- Calibration history
- System status information

## Makefile Commands

```bash
make install-tools    # Install esptool, ampy
make erase           # Erase ESP32 flash
make flash-firmware  # Flash MicroPython
make upload          # Upload Python files
make serial          # Connect to console
make backup-config   # Download config
make restore-config  # Upload config
make clean           # Clean build files
make all            # Complete setup
```

## Configuration

### Basic Settings
```json
{
  "chickens": {
    "adults": 6,
    "base_feed_per_adult": 120
  },
  "feeding": {
    "season_factor": true
  },
  "wifi": {
    "ssid": "YourNetwork",
    "password": "YourPassword"
  }
}
```

### Adding Chicks
Use the web interface to add chick groups:
- Enter number of chicks
- Set birth date
- System calculates age-appropriate feeding

## Advanced Usage

### Serial Console Commands
```python
# Manual feeding
spreader.spread_feed(50)  # 50 grams

# Calibration
spreader.calibrate_run(10)  # 10 second run

# Check schedule
scheduler.should_feed_now()

# Get status
spreader.get_status()
```

### Customization
- Modify feeding schedules in `scheduler.py`
- Adjust web interface styling in `web_server.py`
- Add new hardware controls in `spreader.py`
- Extend configuration in `config.py`

## Troubleshooting

### WiFi Issues
- Check credentials in config
- Try AP mode for initial setup
- Verify signal strength

### Motor Problems
- Check relay wiring and power supply
- Verify motor voltage matches PSU
- Test relay operation manually

### Feeding Inaccuracy
- Recalibrate spreader mechanism
- Check for motor binding or feed clogs
- Verify consistent feed flow

### Web Interface
- Check ESP32 network connection
- Try different browser or clear cache
- Use AP mode if network issues persist

## Safety Considerations

- Use appropriate fuses and GFCI protection
- Secure all mechanical components
- Regular maintenance and inspection
- Weather protection for electronics
- Emergency stop accessible

## Contributing

This is an open-source project designed for the chicken-keeping community. Contributions welcome for:
- Hardware improvements
- Software features
- Documentation updates
- Bug fixes and optimizations

## License

Open source - feel free to modify and distribute for personal and educational use.

---

**Henny makes chicken feeding smart, simple, and reliable!** 🐔✨