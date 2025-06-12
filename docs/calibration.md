# Henny Spreader Calibration Guide

## Overview

Proper calibration is essential for accurate feed dispensing. The calibration process determines how many grams of feed the spreader distributes per 10 seconds of motor operation.

## When to Calibrate

### Initial Setup
- After hardware assembly
- Before first use
- When changing feed type

### Regular Maintenance
- Monthly accuracy check
- After cleaning spreader
- If feed amounts seem incorrect
- When replacing motor or spreader disc

### Troubleshooting
- Inconsistent feed amounts
- Over/under feeding
- After mechanical adjustments

## Calibration Procedure

### Equipment Needed
- Digital scale (accuracy: ±1g minimum)
- Collection container (bowl/tray)
- Stopwatch (for verification)
- Feed (same type as normal use)

### Step-by-Step Process

#### 1. Preparation
```bash
# Ensure hopper is filled with normal feed
# Place collection container under spreader
# Access web interface or serial console
```

#### 2. Run Calibration
- Click "Run Calibration (10s)" in web interface
- OR use serial command: `spreader.calibrate_run(10)`
- Motor will run for exactly 10 seconds
- Collect all spread feed in container

#### 3. Measure and Record
- Weigh collected feed accurately
- Enter measurement in web interface
- System calculates grams per 10 seconds
- Calibration is automatically saved

#### 4. Verification
- Run test with known amount
- Compare actual vs. expected spread
- Repeat calibration if needed

### Example Calibration Session

```
Run 1: 48.5g in 10 seconds
Run 2: 51.2g in 10 seconds  
Run 3: 49.8g in 10 seconds

Average: 49.8g per 10 seconds
Enter: 49.8 in calibration form
```

## Calibration Accuracy

### Acceptable Tolerance
- ±10% accuracy is typical
- ±5% accuracy is excellent
- >15% error indicates problems

### Factors Affecting Accuracy
- Feed type and size
- Hopper fill level
- Motor speed consistency
- Spreader disc condition
- Environmental factors (humidity, temperature)

## Calibration History

The system maintains a history of calibrations:
- Last 10 calibration values
- Timestamps for each calibration
- Average rate calculation
- Trend analysis

### Interpreting History
- Consistent values = good setup
- Declining values = possible wear
- Erratic values = mechanical issues

## Feed Type Considerations

### Different Feed Types
Each feed type may require separate calibration:
- **Starter Feed** (fine): Higher flow rate
- **Grower Feed** (medium): Standard flow rate  
- **Layer Feed** (pellets): Lower flow rate
- **Scratch Grains** (large): Varying flow rate

### Seasonal Changes
- Humidity affects feed flow
- Temperature affects motor performance
- Re-calibrate seasonally

## Troubleshooting Calibration Issues

### Inconsistent Results
**Symptoms**: Large variation between runs
**Causes**:
- Uneven feed flow from hopper
- Motor speed variations
- Spreader disc obstruction
- Feed bridging in hopper

**Solutions**:
- Check hopper design and feed flow
- Inspect motor and power supply
- Clean spreader mechanism
- Tap hopper gently during calibration

### Low Feed Flow
**Symptoms**: Less feed than expected
**Causes**:
- Clogged feed path
- Motor running slow
- Spreader disc worn
- Feed sticking/bridging

**Solutions**:
- Clean all feed paths
- Check motor voltage
- Inspect spreader disc
- Use free-flowing feed

### High Feed Flow
**Symptoms**: More feed than expected
**Causes**:
- Motor running fast
- Oversized spreader openings
- Fine feed flowing too freely
- Vibration affecting flow

**Solutions**:
- Check motor voltage/speed
- Adjust spreader design
- Consider feed change
- Reduce vibration sources

### Erratic Feed Pattern
**Symptoms**: Uneven spread distribution
**Causes**:
- Bent spreader disc
- Off-center mounting
- Variable motor speed
- Obstruction in spread path

**Solutions**:
- Check spreader disc alignment
- Verify motor mounting
- Test motor performance
- Clear spread area

## Advanced Calibration

### Multiple Feed Types
For operations using different feed types:

1. Create separate calibration profiles
2. Switch profiles when changing feed
3. Label feed types clearly
4. Document calibration values

### Environmental Compensation
For varying conditions:

1. Calibrate in different weather
2. Note temperature/humidity effects
3. Adjust values seasonally
4. Monitor long-term trends

### Precision Calibration
For critical applications:

1. Use multiple small test runs
2. Weigh individual portions
3. Statistical analysis of results
4. Regular verification testing

## Calibration Schedule

### Recommended Timeline
- **Weekly**: Quick accuracy check
- **Monthly**: Full re-calibration
- **Seasonally**: Environmental adjustment
- **Annually**: Complete system review

### Quick Check Procedure
1. Run 5-second test
2. Weigh result
3. Compare to expected (rate ÷ 2)
4. Re-calibrate if >10% error

## Maintenance During Calibration

### Cleaning Before Calibration
- Remove old feed residue
- Clean spreader disc
- Check for obstructions
- Verify smooth rotation

### Post-Calibration Checks
- Document calibration value
- Test with actual feeding
- Monitor for several days
- Adjust if needed

## Safety During Calibration

### Electrical Safety
- Ensure dry conditions
- Check all connections
- Use proper tools
- Avoid contact with moving parts

### Mechanical Safety
- Keep hands clear of spreader
- Secure collection container
- Watch for loose parts
- Stop immediately if issues arise

## Record Keeping

### Documentation
Maintain calibration log:
- Date and time
- Feed type used
- Measured amount
- Environmental conditions
- Any issues noted
- Next calibration due

### Example Log Entry
```
Date: 2024-12-06
Time: 14:30
Feed: Layer Pellets
Measured: 49.2g/10s
Temp: 22°C
Humidity: 65%
Notes: Clean spread pattern
Next: 2025-01-06
```

## Integration with Henny System

### Automatic Calculations
Once calibrated, Henny automatically:
- Calculates run times for desired amounts
- Adjusts for different portion sizes
- Monitors feeding accuracy
- Alerts for recalibration needs

### Web Interface Integration
- Real-time calibration status
- Historical data display
- One-click calibration start
- Trend analysis charts

This calibration system ensures Henny delivers accurate, consistent feed amounts for optimal chicken health and nutrition.