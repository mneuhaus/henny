from machine import Pin, Timer
import time
import gc

class Spreader:
    def __init__(self, config):
        self.config = config
        self.relay_pin = Pin(config.get('hardware.relay_pin', 5), Pin.OUT)
        self.led_pin = Pin(config.get('hardware.led_pin', 2), Pin.OUT)
        self.button_pin = Pin(config.get('hardware.button_pin', 0), Pin.IN, Pin.PULL_UP)
        
        self.motor_timeout = config.get('hardware.motor_timeout', 30)
        self.is_spreading = False
        self.spread_timer = None
        self.led_timer = None
        
        # Button debouncing
        self.button_last_state = 1
        self.button_pressed_time = 0
        self.button_debounce_ms = 50
        
        # Safety
        self.relay_pin.value(0)  # Ensure relay is off
        self.led_pin.value(0)    # LED off
        
        print("Spreader initialized")
    
    def spread_feed(self, grams):
        """Spread specified amount of feed in grams"""
        if self.is_spreading:
            print("Already spreading, ignoring request")
            return False
        
        rate = self.config.get('spreader.grams_per_10s', 50.0)
        if rate <= 0:
            print("Invalid calibration rate, please calibrate spreader")
            return False
        
        # Calculate run time in seconds
        run_time_s = (grams / rate) * 10.0
        run_time_s = min(run_time_s, self.motor_timeout)  # Safety limit
        
        print(f"Spreading {grams}g (rate: {rate}g/10s, time: {run_time_s:.1f}s)")
        
        self._start_motor(int(run_time_s * 1000))  # Convert to ms
        return True
    
    def calibrate_run(self, duration_s=10):
        """Run motor for calibration (default 10 seconds)"""
        if self.is_spreading:
            print("Already spreading, cannot calibrate")
            return False
        
        duration_s = min(duration_s, self.motor_timeout)
        print(f"Calibration run: {duration_s} seconds")
        
        self._start_motor(duration_s * 1000)
        return True
    
    def _start_motor(self, duration_ms):
        """Start motor for specified duration in milliseconds"""
        if self.is_spreading:
            return False
        
        self.is_spreading = True
        self.relay_pin.value(1)  # Turn on relay
        
        # Start blinking LED to show activity
        self._start_led_blink()
        
        # Set timer to stop motor
        self.spread_timer = Timer(-1)
        self.spread_timer.init(
            mode=Timer.ONE_SHOT,
            period=duration_ms,
            callback=self._stop_motor_callback
        )
        
        print(f"Motor started for {duration_ms}ms")
        return True
    
    def _stop_motor_callback(self, timer):
        """Callback to stop motor"""
        self._stop_motor()
    
    def _stop_motor(self):
        """Stop motor and cleanup"""
        self.relay_pin.value(0)  # Turn off relay
        self.is_spreading = False
        
        if self.spread_timer:
            self.spread_timer.deinit()
            self.spread_timer = None
        
        self._stop_led_blink()
        print("Motor stopped")
        gc.collect()
    
    def emergency_stop(self):
        """Emergency stop motor"""
        print("EMERGENCY STOP")
        self._stop_motor()
    
    def _start_led_blink(self):
        """Start LED blinking to indicate activity"""
        if self.led_timer:
            self.led_timer.deinit()
        
        self.led_timer = Timer(-1)
        self.led_timer.init(
            mode=Timer.PERIODIC,
            period=250,  # 250ms blink
            callback=self._led_blink_callback
        )
    
    def _stop_led_blink(self):
        """Stop LED blinking"""
        if self.led_timer:
            self.led_timer.deinit()
            self.led_timer = None
        self.led_pin.value(0)  # LED off
    
    def _led_blink_callback(self, timer):
        """Toggle LED for blinking"""
        self.led_pin.value(not self.led_pin.value())
    
    def set_status_led(self, state):
        """Set status LED (solid on/off when not blinking)"""
        if not self.is_spreading:
            self.led_pin.value(1 if state else 0)
    
    def check_button(self):
        """Check button press with debouncing. Returns press type or None"""
        current_state = self.button_pin.value()
        current_time = time.ticks_ms()
        
        # Button pressed (goes low)
        if self.button_last_state == 1 and current_state == 0:
            self.button_pressed_time = current_time
            self.button_last_state = current_state
            return None
        
        # Button released (goes high)
        elif self.button_last_state == 0 and current_state == 1:
            press_duration = time.ticks_diff(current_time, self.button_pressed_time)
            self.button_last_state = current_state
            
            if press_duration > self.button_debounce_ms:
                if press_duration > 3000:  # 3 second long press
                    return 'long'
                else:
                    return 'short'
        
        return None
    
    def manual_feed(self, amount_g=25):
        """Manual feed button pressed (short press)"""
        print(f"Manual feed: {amount_g}g")
        return self.spread_feed(amount_g)
    
    def calibration_mode(self):
        """Enter calibration mode (long press)"""
        print("Entering calibration mode...")
        print("Run calibration via web interface or serial")
        
        # Blink LED pattern for calibration mode
        for _ in range(6):  # 3 quick blinks
            self.led_pin.value(1)
            time.sleep_ms(100)
            self.led_pin.value(0)
            time.sleep_ms(100)
    
    def get_status(self):
        """Get current spreader status"""
        return {
            'is_spreading': self.is_spreading,
            'calibration_rate': self.config.get('spreader.grams_per_10s', 0),
            'last_calibration': self.config.get('spreader.last_calibration'),
            'motor_timeout': self.motor_timeout
        }
    
    def calculate_spread_time(self, grams):
        """Calculate how long motor needs to run for given grams"""
        rate = self.config.get('spreader.grams_per_10s', 50.0)
        if rate <= 0:
            return 0
        return min((grams / rate) * 10.0, self.motor_timeout)
    
    def is_calibrated(self):
        """Check if spreader has been calibrated"""
        rate = self.config.get('spreader.grams_per_10s', 0)
        return rate > 0
    
    def cleanup(self):
        """Cleanup resources"""
        self._stop_motor()
        if self.led_timer:
            self.led_timer.deinit()
        if self.spread_timer:
            self.spread_timer.deinit()
        gc.collect()