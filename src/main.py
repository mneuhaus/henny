import time
import network
import ntptime
import gc
from machine import WDT, reset
import sys

# Import Henny modules
from lib.config import Config
from lib.spreader import Spreader
from lib.scheduler import FeedingScheduler
from lib.web_server import WebServer

class HennyFeeder:
    def __init__(self):
        print("🐔 Henny Smart Chicken Feeder Starting...")
        
        # Initialize configuration
        print("Loading configuration...")
        self.config = Config()
        
        # Initialize hardware components
        print("Initializing spreader...")
        self.spreader = Spreader(self.config)
        print("Initializing scheduler...")
        self.scheduler = FeedingScheduler(self.config)
        print("Initializing web server...")
        self.web_server = WebServer(self.config, self.spreader, self.scheduler)
        
        # State tracking
        self.wifi_connected = False
        self.ntp_synced = False
        self.ap_mode = False
        self.last_wifi_check = 0
        self.last_ntp_sync = 0
        self.last_schedule_check = 0
        
        # WiFi objects
        self.sta_if = network.WLAN(network.STA_IF)
        self.ap_if = network.WLAN(network.AP_IF)
        
        # Watchdog timer (optional - enable for production)
        # self.wdt = WDT(timeout=30000)  # 30 second watchdog
        
        print("Henny initialized successfully")
    
    def run(self):
        """Main application loop"""
        print("Starting Henny main loop...")
        
        # Initial setup
        self._setup_wifi()
        self._sync_time()
        self._start_web_server()
        
        # Status LED on to show ready
        self.spreader.set_status_led(True)
        
        print("Henny is ready! 🐔")
        
        # Main loop
        loop_counter = 0
        try:
            while True:
                current_time = time.time()
                
                # Watchdog feed (if enabled)
                # self.wdt.feed()
                
                # Check WiFi connection (every 30 seconds)
                if current_time - self.last_wifi_check > 30:
                    self._check_wifi()
                    self.last_wifi_check = current_time
                
                # Sync time via NTP (every hour)
                if current_time - self.last_ntp_sync > 3600:
                    self._sync_time()
                    self.last_ntp_sync = current_time
                
                # Check feeding schedule (every minute)
                if current_time - self.last_schedule_check > 60:
                    self._check_feeding_schedule()
                    self.last_schedule_check = current_time
                
                # Handle web server requests
                self.web_server.handle_requests()
                
                # Check manual button
                button_press = self.spreader.check_button()
                if button_press == 'short':
                    print("Manual feed button pressed")
                    self.spreader.manual_feed()
                elif button_press == 'long':
                    print("Calibration mode button pressed")
                    self.spreader.calibration_mode()
                
                # Reset daily counter at midnight
                if self.scheduler.should_reset_daily_counter():
                    print("Daily counter reset")
                
                # Periodic cleanup
                loop_counter += 1
                if loop_counter % 100 == 0:
                    gc.collect()
                    print(f"Loop #{loop_counter}, free memory: {gc.mem_free()}")
                
                # Short sleep to prevent busy waiting
                time.sleep_ms(100)
                
        except KeyboardInterrupt:
            print("Shutting down Henny...")
            self._cleanup()
        except Exception as e:
            print(f"Critical error in main loop: {e}")
            self._emergency_cleanup()
            time.sleep(5)
            reset()  # Restart on critical error
    
    def _setup_wifi(self):
        """Setup WiFi connection"""
        ssid = self.config.get('wifi.ssid', '')
        password = self.config.get('wifi.password', '')
        
        if ssid and password:
            print(f"Connecting to WiFi: {ssid}")
            self._connect_wifi(ssid, password)
        
        # Start AP mode if no WiFi or connection failed
        if not self.wifi_connected:
            self._start_ap_mode()
    
    def _connect_wifi(self, ssid, password, timeout=20):
        """Connect to WiFi network"""
        self.sta_if.active(True)
        
        if self.sta_if.isconnected():
            print("Already connected to WiFi")
            self.wifi_connected = True
            return True
        
        print(f"Connecting to {ssid}...")
        self.sta_if.connect(ssid, password)
        
        # Wait for connection
        start_time = time.time()
        while not self.sta_if.isconnected() and (time.time() - start_time) < timeout:
            time.sleep(1)
            print(".", end="")
        
        if self.sta_if.isconnected():
            print(f"\nWiFi connected! IP: {self.sta_if.ifconfig()[0]}")
            self.wifi_connected = True
            self.ap_mode = False
            return True
        else:
            print(f"\nFailed to connect to {ssid}")
            self.wifi_connected = False
            return False
    
    def _start_ap_mode(self):
        """Start Access Point mode for setup"""
        ap_ssid = self.config.get('wifi.ap_ssid', 'Henny-Setup')
        ap_password = self.config.get('wifi.ap_password', 'hennyfeeder')
        
        print(f"Starting AP mode: {ap_ssid}")
        
        self.ap_if.active(True)
        self.ap_if.config(essid=ap_ssid, password=ap_password)
        
        if self.ap_if.active():
            print(f"AP started - Connect to '{ap_ssid}' with password '{ap_password}'")
            print(f"Web interface: http://{self.ap_if.ifconfig()[0]}")
            self.ap_mode = True
        else:
            print("Failed to start AP mode")
    
    def _check_wifi(self):
        """Check and maintain WiFi connection"""
        if self.ap_mode:
            return  # Skip if in AP mode
        
        if not self.sta_if.isconnected():
            print("WiFi disconnected, attempting reconnect...")
            self.wifi_connected = False
            
            ssid = self.config.get('wifi.ssid', '')
            password = self.config.get('wifi.password', '')
            
            if ssid and password:
                self._connect_wifi(ssid, password, timeout=10)
            
            if not self.wifi_connected:
                print("WiFi reconnection failed")
                # Could switch to AP mode here if desired
    
    def _sync_time(self):
        """Synchronize time via NTP"""
        if not self.wifi_connected or self.ap_mode:
            print("Skipping NTP sync - no internet connection")
            return
        
        try:
            print("Syncing time with NTP...")
            ntptime.settime()
            self.ntp_synced = True
            current_time = time.localtime()
            print(f"Time synced: {current_time[0]}-{current_time[1]:02d}-{current_time[2]:02d} {current_time[3]:02d}:{current_time[4]:02d}")
        except Exception as e:
            print(f"NTP sync failed: {e}")
            self.ntp_synced = False
    
    def _start_web_server(self):
        """Start the web server"""
        if self.web_server.start():
            if self.wifi_connected:
                ip = self.sta_if.ifconfig()[0]
                print(f"Web interface available at: http://{ip}")
            elif self.ap_mode:
                ip = self.ap_if.ifconfig()[0]
                print(f"Web interface available at: http://{ip}")
        else:
            print("Failed to start web server")
    
    def _check_feeding_schedule(self):
        """Check if it's time to feed and execute if needed"""
        if self.scheduler.should_feed_now():
            amount = self.scheduler.calculate_feed_per_session()
            
            print(f"Scheduled feeding time! Amount: {amount}g")
            
            if self.spreader.is_calibrated():
                if self.spreader.spread_feed(amount):
                    print(f"Automatic feeding started: {amount}g")
                    self.scheduler.record_feeding(amount)
                    
                    # Blink status LED to indicate feeding
                    for _ in range(3):
                        self.spreader.set_status_led(False)
                        time.sleep_ms(200)
                        self.spreader.set_status_led(True)
                        time.sleep_ms(200)
                else:
                    print("Scheduled feeding failed - spreader busy")
            else:
                print("Scheduled feeding skipped - spreader not calibrated")
    
    def _cleanup(self):
        """Clean shutdown"""
        print("Cleaning up...")
        
        self.spreader.set_status_led(False)
        self.spreader.cleanup()
        self.web_server.stop()
        
        if self.sta_if.active():
            self.sta_if.active(False)
        if self.ap_if.active():
            self.ap_if.active(False)
        
        gc.collect()
        print("Cleanup complete")
    
    def _emergency_cleanup(self):
        """Emergency cleanup on critical error"""
        print("Emergency cleanup...")
        
        try:
            self.spreader.emergency_stop()
            self.spreader.set_status_led(False)
        except:
            pass
        
        try:
            self.web_server.stop()
        except:
            pass
        
        gc.collect()

# Global exception handler
def global_exception_handler(exctype, value, traceback):
    """Handle uncaught exceptions"""
    print(f"Uncaught exception: {exctype.__name__}: {value}")
    sys.print_exception(value)
    
    # Try emergency stop
    try:
        # This assumes we have a global spreader instance
        # In practice, you might need a different approach
        pass
    except:
        pass
    
    # Reset after delay
    print("Restarting in 10 seconds...")
    time.sleep(10)
    reset()

# Note: MicroPython doesn't have sys.excepthook, using try/catch in main instead

# Main execution
if __name__ == "__main__":
    try:
        print("Creating HennyFeeder instance...")
        henny = HennyFeeder()
        print("Starting Henny run loop...")
        henny.run()
    except Exception as e:
        import sys
        sys.print_exception(e)
        print(f"Failed to start Henny: {e}")
        time.sleep(5)
        reset()