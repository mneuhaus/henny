import time
import math
import gc

class FeedingScheduler:
    def __init__(self, config):
        self.config = config
        self.last_check_time = 0
        print("Feeding scheduler initialized")
    
    def get_season(self):
        """Determine current season based on date"""
        current_time = time.localtime()
        month = current_time[1]
        
        if month in [12, 1, 2]:
            return 'winter'
        elif month in [3, 4, 5]:
            return 'spring'
        elif month in [6, 7, 8]:
            return 'summer'
        else:  # 9, 10, 11
            return 'autumn'
    
    def calculate_daily_feed_amount(self):
        """Calculate total daily feed amount in grams"""
        adults = self.config.get('chickens.adults', 0)
        base_per_adult = self.config.get('chickens.base_feed_per_adult', 120)
        
        # Start with adult chickens
        total_grams = adults * base_per_adult
        
        # Add chicks based on age
        chick_ages = self.config.get_chick_ages()
        for group in chick_ages:
            chick_feed = self._calculate_chick_feed(group['age_days'])
            total_grams += group['count'] * chick_feed
        
        # Apply seasonal factor if enabled
        if self.config.get('feeding.season_factor', True):
            season_multiplier = self._get_season_multiplier()
            total_grams *= season_multiplier
        
        return int(total_grams)
    
    def _calculate_chick_feed(self, age_days):
        """Calculate feed amount per chick based on age in days"""
        if age_days < 7:
            return 15  # Very young chicks - starter feed
        elif age_days < 21:
            return 30  # Growing chicks
        elif age_days < 42:
            return 50  # Developing chicks
        elif age_days < 84:
            return 80  # Young chickens
        else:
            return 120  # Full adult amount
    
    def _get_season_multiplier(self):
        """Get seasonal feed adjustment multiplier"""
        season = self.get_season()
        multipliers = {
            'winter': 1.15,  # More feed in cold weather
            'spring': 1.05,  # Slightly more for egg laying season
            'summer': 0.95,  # Less in hot weather
            'autumn': 1.0    # Normal amount
        }
        return multipliers.get(season, 1.0)
    
    def get_feeding_times(self):
        """Get feeding times for current season"""
        season = self.get_season()
        num_feedings = self.config.get(f'feeding.daily_feedings.{season}', 3)
        
        # Define feeding schedules by season and number of feeds
        schedules = {
            'summer': {
                4: [6, 10, 15, 19],  # Early morning to evening
                3: [7, 13, 18]
            },
            'winter': {
                3: [8, 13, 17],      # Later start due to shorter days
                2: [9, 16]
            },
            'spring': {
                3: [7, 12, 17],
                2: [8, 16]
            },
            'autumn': {
                3: [7, 13, 18],
                2: [8, 16]
            }
        }
        
        season_schedule = schedules.get(season, schedules['spring'])
        times = season_schedule.get(num_feedings, season_schedule[3])
        
        return times
    
    def get_next_feeding_time(self):
        """Get the next scheduled feeding time"""
        current_time = time.localtime()
        current_hour = current_time[3]
        current_minute = current_time[4]
        current_minutes_since_midnight = current_hour * 60 + current_minute
        
        feeding_times = self.get_feeding_times()
        
        # Convert to minutes since midnight
        feeding_minutes = [hour * 60 for hour in feeding_times]
        
        # Find next feeding time
        for feed_time in feeding_minutes:
            if feed_time > current_minutes_since_midnight:
                hours = feed_time // 60
                minutes = feed_time % 60
                return (hours, minutes)
        
        # If no more feedings today, return first feeding tomorrow
        next_feed = feeding_minutes[0]
        hours = next_feed // 60
        minutes = next_feed % 60
        return (hours, minutes)
    
    def should_feed_now(self):
        """Check if it's time to feed now"""
        current_time = time.localtime()
        current_hour = current_time[3]
        current_minute = current_time[4]
        
        feeding_times = self.get_feeding_times()
        
        # Check if current time matches any feeding time (±2 minutes window)
        for feed_hour in feeding_times:
            if abs(current_hour - feed_hour) == 0 and current_minute <= 2:
                # Check if we haven't fed recently (prevent double feeding)
                if not self._recently_fed(30):  # 30 minute window
                    return True
        
        return False
    
    def _recently_fed(self, minutes_window):
        """Check if we fed within the last X minutes"""
        last_feed = self.config.get('schedule.last_feed_time')
        if last_feed is None:
            return False
        
        current_time = time.time()
        time_since_feed = (current_time - last_feed) / 60  # Convert to minutes
        
        return time_since_feed < minutes_window
    
    def calculate_feed_per_session(self):
        """Calculate how much to feed per session"""
        daily_amount = self.calculate_daily_feed_amount()
        num_feedings = len(self.get_feeding_times())
        
        if num_feedings == 0:
            return daily_amount
        
        return int(daily_amount / num_feedings)
    
    def record_feeding(self, amount_grams):
        """Record that feeding occurred"""
        current_time = time.time()
        self.config.set('schedule.last_feed_time', current_time)
        self.config.increment_daily_counter()
        
        print(f"Feeding recorded: {amount_grams}g at {time.localtime()}")
    
    def get_daily_progress(self):
        """Get today's feeding progress"""
        daily_target = self.calculate_daily_feed_amount()
        sessions_target = len(self.get_feeding_times())
        sessions_completed = self.config.get('schedule.daily_counter', 0)
        
        amount_fed = sessions_completed * self.calculate_feed_per_session()
        
        return {
            'target_grams': daily_target,
            'fed_grams': amount_fed,
            'sessions_target': sessions_target,
            'sessions_completed': sessions_completed,
            'progress_percent': min(100, int((amount_fed / daily_target) * 100)) if daily_target > 0 else 0
        }
    
    def get_feeding_schedule_display(self):
        """Get formatted schedule for display"""
        times = self.get_feeding_times()
        amount_per_session = self.calculate_feed_per_session()
        season = self.get_season()
        
        schedule = []
        for hour in times:
            schedule.append({
                'time': f"{hour:02d}:00",
                'amount': amount_per_session,
                'completed': False  # Will be updated by caller
            })
        
        return {
            'season': season.title(),
            'daily_total': self.calculate_daily_feed_amount(),
            'sessions': schedule
        }
    
    def get_chicken_summary(self):
        """Get summary of chickens and their feed requirements"""
        adults = self.config.get('chickens.adults', 0)
        chick_groups = self.config.get_chick_ages()
        
        total_chicks = sum(group['count'] for group in chick_groups)
        
        # Calculate age distribution
        age_categories = {
            'chicks_0_3_weeks': 0,
            'chicks_3_6_weeks': 0,
            'chicks_6_12_weeks': 0,
            'young_chickens': 0
        }
        
        for group in chick_groups:
            age_days = group['age_days']
            count = group['count']
            
            if age_days < 21:
                age_categories['chicks_0_3_weeks'] += count
            elif age_days < 42:
                age_categories['chicks_3_6_weeks'] += count
            elif age_days < 84:
                age_categories['chicks_6_12_weeks'] += count
            else:
                age_categories['young_chickens'] += count
        
        return {
            'adults': adults,
            'total_chicks': total_chicks,
            'age_distribution': age_categories,
            'total_birds': adults + total_chicks
        }
    
    def should_reset_daily_counter(self):
        """Check if daily counter should be reset"""
        return self.config.reset_daily_counter()
    
    def cleanup(self):
        """Cleanup resources"""
        gc.collect()