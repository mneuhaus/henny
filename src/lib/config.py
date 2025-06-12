import json
import gc
from time import localtime

class Config:
    def __init__(self, filename='config.json'):
        self.filename = filename
        self.data = self._load_defaults()
        self.load()
    
    def _load_defaults(self):
        return {
            'chickens': {
                'adults': 6,
                'chicks': [],  # [{'count': int, 'birth_date': 'YYYY-MM-DD'}, ...]
                'base_feed_per_adult': 120  # grams per day
            },
            'feeding': {
                'season_factor': True,
                'daily_feedings': {
                    'summer': 4,
                    'winter': 3,
                    'spring': 3,
                    'autumn': 3
                }
            },
            'hardware': {
                'relay_pin': 5,
                'led_pin': 2,
                'button_pin': 0,
                'motor_timeout': 30
            },
            'spreader': {
                'grams_per_10s': 50.0,  # Calibrated rate
                'last_calibration': None,
                'calibration_history': []
            },
            'wifi': {
                'ssid': '',
                'password': '',
                'ap_mode': True,
                'ap_ssid': 'Henny-Setup',
                'ap_password': 'hennyfeeder'
            },
            'schedule': {
                'last_feed_time': None,
                'daily_counter': 0,
                'last_reset_day': None
            }
        }
    
    def load(self):
        try:
            with open(self.filename, 'r') as f:
                loaded_data = json.load(f)
                self._merge_config(loaded_data)
            print(f"Config loaded from {self.filename}")
        except (OSError, ValueError) as e:
            print(f"Config file not found or invalid, using defaults: {e}")
            self.save()
        gc.collect()
    
    def _merge_config(self, loaded_data):
        """Merge loaded config with defaults to handle missing keys"""
        def merge_dict(default, loaded):
            for key, value in loaded.items():
                if key in default:
                    if isinstance(value, dict) and isinstance(default[key], dict):
                        merge_dict(default[key], value)
                    else:
                        default[key] = value
        
        merge_dict(self.data, loaded_data)
    
    def save(self):
        try:
            with open(self.filename, 'w') as f:
                json.dump(self.data, f, indent=2)
            print(f"Config saved to {self.filename}")
        except OSError as e:
            print(f"Failed to save config: {e}")
        gc.collect()
    
    def get(self, path, default=None):
        """Get config value using dot notation: 'chickens.adults'"""
        keys = path.split('.')
        value = self.data
        try:
            for key in keys:
                value = value[key]
            return value
        except (KeyError, TypeError):
            return default
    
    def set(self, path, value):
        """Set config value using dot notation"""
        keys = path.split('.')
        data = self.data
        for key in keys[:-1]:
            if key not in data:
                data[key] = {}
            data = data[key]
        data[keys[-1]] = value
        self.save()
    
    def add_chick_group(self, count, birth_date):
        """Add a group of chicks born on the same date"""
        if not self.data['chickens']['chicks']:
            self.data['chickens']['chicks'] = []
        
        self.data['chickens']['chicks'].append({
            'count': count,
            'birth_date': birth_date
        })
        self.save()
    
    def remove_chick_group(self, index):
        """Remove a chick group by index"""
        if 0 <= index < len(self.data['chickens']['chicks']):
            self.data['chickens']['chicks'].pop(index)
            self.save()
            return True
        return False
    
    def get_chick_ages(self):
        """Calculate current age in days for each chick group"""
        from time import time
        current_time = localtime()
        current_day = current_time[7]  # Day of year
        current_year = current_time[0]
        
        ages = []
        for group in self.data['chickens']['chicks']:
            birth_parts = group['birth_date'].split('-')
            birth_year = int(birth_parts[0])
            birth_month = int(birth_parts[1])
            birth_day = int(birth_parts[2])
            
            # Simple age calculation in days
            days_since_birth = (current_year - birth_year) * 365
            days_since_birth += current_day - self._day_of_year(birth_year, birth_month, birth_day)
            
            ages.append({
                'count': group['count'],
                'age_days': max(0, days_since_birth)
            })
        
        return ages
    
    def _day_of_year(self, year, month, day):
        """Calculate day of year from month/day"""
        days_in_month = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
        if year % 4 == 0 and (year % 100 != 0 or year % 400 == 0):
            days_in_month[1] = 29
        
        day_of_year = sum(days_in_month[:month-1]) + day
        return day_of_year
    
    def reset_daily_counter(self):
        """Reset daily feed counter"""
        current_time = localtime()
        current_day = current_time[2]  # Day of month
        
        if self.data['schedule']['last_reset_day'] != current_day:
            self.data['schedule']['daily_counter'] = 0
            self.data['schedule']['last_reset_day'] = current_day
            self.save()
            return True
        return False
    
    def increment_daily_counter(self):
        """Increment daily feed counter"""
        self.data['schedule']['daily_counter'] += 1
        self.save()
    
    def update_calibration(self, grams_per_10s):
        """Update spreader calibration"""
        from time import time
        
        self.data['spreader']['grams_per_10s'] = grams_per_10s
        self.data['spreader']['last_calibration'] = time()
        
        # Add to history (keep last 10)
        if len(self.data['spreader']['calibration_history']) >= 10:
            self.data['spreader']['calibration_history'].pop(0)
        
        self.data['spreader']['calibration_history'].append({
            'rate': grams_per_10s,
            'timestamp': time()
        })
        
        self.save()