#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <time.h>
#include <Preferences.h>
#include <math.h>

#define RELAY_PIN 1     // D0/GPIO1 on XIAO ESP32-S3
#define LED_PIN 48      // Built-in RGB LED on XIAO ESP32-S3  
#define BUTTON_PIN 2    // D1/GPIO2 on XIAO ESP32-S3

#define MOTOR_TIMEOUT_MS 30000
#define CALIBRATION_DURATION_MS 10000
#define BUTTON_LONG_PRESS_MS 3000

WebServer server(80);
Preferences preferences;

class Spreader {
private:
    unsigned long motorStartTime = 0;
    bool motorRunning = false;
    float gramsPerSecond = 0.5;
    
public:
    void begin() {
        pinMode(RELAY_PIN, OUTPUT);
        pinMode(LED_PIN, OUTPUT);
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
    }
    
    void startMotor() {
        if (!motorRunning) {
            digitalWrite(RELAY_PIN, HIGH);
            digitalWrite(LED_PIN, HIGH);
            motorStartTime = millis();
            motorRunning = true;
            Serial.println("Motor started");
        }
    }
    
    void stopMotor() {
        if (motorRunning) {
            digitalWrite(RELAY_PIN, LOW);
            digitalWrite(LED_PIN, LOW);
            motorRunning = false;
            Serial.println("Motor stopped");
        }
    }
    
    void spreadFeed(float grams) {
        float duration = (grams / gramsPerSecond) * 1000;
        Serial.printf("Spreading %.1fg for %.1f seconds\n", grams, duration/1000.0);
        
        startMotor();
        unsigned long startTime = millis();
        
        while (millis() - startTime < duration && millis() - motorStartTime < MOTOR_TIMEOUT_MS) {
            delay(10);
        }
        
        stopMotor();
    }
    
    void calibrationRun() {
        Serial.println("Starting 10 second calibration run");
        startMotor();
        delay(CALIBRATION_DURATION_MS);
        stopMotor();
        Serial.println("Calibration complete - measure dispensed amount");
    }
    
    void testRun() {
        Serial.println("Starting 3 second motor test");
        startMotor();
        delay(3000); // 3 seconds
        stopMotor();
        Serial.println("Motor test complete");
    }
    
    void setCalibration(float gramsPerTenSeconds) {
        gramsPerSecond = gramsPerTenSeconds / 10.0;
        preferences.putFloat("cal", gramsPerTenSeconds);
        Serial.printf("Calibration set: %.2fg per second\n", gramsPerSecond);
    }
    
    float getCalibration() {
        return gramsPerSecond * 10.0;
    }
    
    void update() {
        if (motorRunning && millis() - motorStartTime > MOTOR_TIMEOUT_MS) {
            stopMotor();
            Serial.println("Motor timeout!");
        }
    }
};

class Scheduler {
private:
    struct FeedingTime {
        int hour;
        int minute;
    };
    
    FeedingTime summerSchedule[4] = {{6,0}, {10,0}, {15,0}, {19,0}};
    FeedingTime winterSchedule[3] = {{8,0}, {13,0}, {17,0}};
    FeedingTime springSchedule[3] = {{7,0}, {12,0}, {17,0}};
    
    bool fedToday[8] = {false, false, false, false, false, false, false, false};
    int lastFeedDay = -1;
    
    // Calculate sunset time based on day of year (approximate for Central Europe)
    int getSunsetHour(int dayOfYear) {
        // Simplified sunset calculation for latitude ~50°N (Germany)
        // Summer solstice (day 172): sunset ~21:30
        // Winter solstice (day 355): sunset ~16:30
        float angle = (dayOfYear - 172) * 2.0 * M_PI / 365.0;
        float sunsetDecimal = 19.0 + 2.5 * cos(angle); // Between 16.5 and 21.5
        return (int)sunsetDecimal;
    }
    
    // Calculate sunrise time based on day of year (approximate for Central Europe)
    int getSunriseHour(int dayOfYear) {
        // Simplified sunrise calculation for latitude ~50°N (Germany)
        // Summer solstice (day 172): sunrise ~5:30
        // Winter solstice (day 355): sunrise ~8:30
        float angle = (dayOfYear - 172) * 2.0 * M_PI / 365.0;
        float sunriseDecimal = 7.0 - 1.5 * cos(angle); // Between 5.5 and 8.5
        return (int)sunriseDecimal;
    }
    
public:
    float getDailyFeedAmount(int adultChickens, int gramsPerChicken) {
        float total = adultChickens * gramsPerChicken;
        
        int month = 0;
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            month = timeinfo.tm_mon + 1;
        }
        
        if (month == 12 || month <= 2) total *= 1.15;
        else if (month >= 3 && month <= 5) total *= 1.05;
        else if (month >= 6 && month <= 8) total *= 0.95;
        
        return total;
    }
    
    FeedingTime* getCurrentSchedule(int &count, int frequency, int sunriseOff, int sunsetOff) {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            count = frequency;
            // Generate fallback schedule
            static FeedingTime fallbackSchedule[8];
            for (int i = 0; i < frequency; i++) {
                fallbackSchedule[i].hour = 8 + (i * 10 / frequency);
                fallbackSchedule[i].minute = 0;
            }
            return fallbackSchedule;
        }
        
        int dayOfYear = timeinfo.tm_yday;
        int sunriseHour = getSunriseHour(dayOfYear);
        int sunsetHour = getSunsetHour(dayOfYear);
        
        // Generate schedule based on frequency
        static FeedingTime dynamicSchedule[8];
        count = frequency;
        
        // Calculate feeding times between sunrise+offset and sunset-offset
        int startHour = sunriseHour + sunriseOff;
        int endHour = sunsetHour - sunsetOff;
        int totalHours = endHour - startHour;
        
        // Ensure we have at least 1 hour window
        if (totalHours < 1) {
            startHour = sunriseHour + 1;
            endHour = sunsetHour - 1;
            totalHours = endHour - startHour;
        }
        
        if (frequency == 1) {
            dynamicSchedule[0].hour = startHour + (totalHours / 2);
            dynamicSchedule[0].minute = 0;
        } else {
            for (int i = 0; i < frequency; i++) {
                float position = (float)i / (frequency - 1);
                dynamicSchedule[i].hour = startHour + (int)(totalHours * position);
                dynamicSchedule[i].minute = 0;
            }
        }
        
        return dynamicSchedule;
    }
    
    bool shouldFeedNow(float &feedAmount, int adultChickens, int gramsPerChicken, int frequency, int sunriseOff, int sunsetOff) {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return false;
        }
        
        int currentDay = timeinfo.tm_yday;
        if (currentDay != lastFeedDay) {
            for (int i = 0; i < 8; i++) fedToday[i] = false; // Clear all slots
            lastFeedDay = currentDay;
        }
        
        int scheduleCount;
        FeedingTime* schedule = getCurrentSchedule(scheduleCount, frequency, sunriseOff, sunsetOff);
        float dailyTotal = getDailyFeedAmount(adultChickens, gramsPerChicken);
        float perFeeding = dailyTotal / scheduleCount;
        
        for (int i = 0; i < scheduleCount; i++) {
            if (!fedToday[i] && 
                timeinfo.tm_hour == schedule[i].hour && 
                timeinfo.tm_min >= schedule[i].minute &&
                timeinfo.tm_min < schedule[i].minute + 5) {
                fedToday[i] = true;
                feedAmount = perFeeding;
                return true;
            }
        }
        
        return false;
    }
    
    String getSunriseTime() {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return "---";
        }
        int hour = getSunriseHour(timeinfo.tm_yday);
        return String(hour) + ":30";
    }
    
    String getSunsetTime() {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return "---";
        }
        int hour = getSunsetHour(timeinfo.tm_yday);
        return String(hour) + ":30";
    }
};

Spreader spreader;
Scheduler scheduler;

int adultChickens = 6;
int feedAmountPerChicken = 120; // grams per day
int feedFrequency = 3; // times per day
int sunriseOffset = 2; // hours after sunrise
int sunsetOffset = 2; // hours before sunset
String language = "de"; // "de" or "en"

unsigned long buttonPressStart = 0;
bool buttonPressed = false;
bool lastButtonState = HIGH;

String getTranslation(String key, String lang) {
    // German translations
    if (lang == "de") {
        if (key == "subtitle") return "Intelligente H&uuml;hnerf&uuml;tterung";
        if (key == "feeding_schedule_title") return "Heutige F&uuml;tterungszeiten";
        if (key == "sunrise_text") return "Sonnenaufgang";
        if (key == "sunset_text") return "Sonnenuntergang";
        if (key == "system_status_title") return "System-Status";
        if (key == "adult_chickens_text") return "Erwachsene H&uuml;hner";
        if (key == "calibration_text") return "Kalibrierung";
        if (key == "time_text") return "Zeit";
        if (key == "daily_feed_text") return "Futter pro Tag";
        if (key == "monthly_feed_text") return "Futter pro Monat";
        if (key == "chicken_config_title") return "H&uuml;hner-Konfiguration";
        if (key == "adult_chickens_label") return "Erwachsene H&uuml;hner";
        if (key == "feed_per_chicken_label") return "Futter pro Huhn/Tag";
        if (key == "feedings_per_day_label") return "F&uuml;tterungen pro Tag";
        if (key == "first_feeding_label") return "Erste F&uuml;tterung";
        if (key == "last_feeding_label") return "Letzte F&uuml;tterung";
        if (key == "after_sunrise_text") return "nach Sonnenaufgang";
        if (key == "before_sunset_text") return "vor Sonnenuntergang";
        if (key == "update_button_text") return "Aktualisieren";
        if (key == "calibration_title") return "Kalibrierung";
        if (key == "calibration_instruction") return "F&uuml;hren Sie einen 10-Sekunden-Kalibrierungstest durch, messen Sie dann die tats&auml;chlich ausgegebene Menge und geben Sie diese ein.";
        if (key == "measured_amount_label") return "Gemessene Menge (g)";
        if (key == "dispensed_grams_placeholder") return "Ausgegebene Gramm";
        if (key == "start_test_button") return "Test starten";
        if (key == "save_calibration_button") return "Kalibrierung speichern";
    }
    // English translations
    else if (lang == "en") {
        if (key == "subtitle") return "Intelligent Chicken Feeding";
        if (key == "feeding_schedule_title") return "Today's Feeding Schedule";
        if (key == "sunrise_text") return "Sunrise";
        if (key == "sunset_text") return "Sunset";
        if (key == "system_status_title") return "System Status";
        if (key == "adult_chickens_text") return "Adult Chickens";
        if (key == "calibration_text") return "Calibration";
        if (key == "time_text") return "Time";
        if (key == "daily_feed_text") return "Daily Feed";
        if (key == "monthly_feed_text") return "Monthly Feed";
        if (key == "chicken_config_title") return "Chicken Configuration";
        if (key == "adult_chickens_label") return "Adult Chickens";
        if (key == "feed_per_chicken_label") return "Feed per Chicken/Day";
        if (key == "feedings_per_day_label") return "Feedings per Day";
        if (key == "first_feeding_label") return "First Feeding";
        if (key == "last_feeding_label") return "Last Feeding";
        if (key == "after_sunrise_text") return "after sunrise";
        if (key == "before_sunset_text") return "before sunset";
        if (key == "update_button_text") return "Update";
        if (key == "calibration_title") return "Calibration";
        if (key == "calibration_instruction") return "Run a 10-second calibration test, then measure the actual dispensed amount and enter it below.";
        if (key == "measured_amount_label") return "Measured Amount (g)";
        if (key == "dispensed_grams_placeholder") return "Dispensed Grams";
        if (key == "start_test_button") return "Start Test";
        if (key == "save_calibration_button") return "Save Calibration";
    }
    return key; // Fallback
}

String generateHTML() {
    String html = R"HTML(
<!DOCTYPE html>
<html lang="{LANG}">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Henny</title>
    <link rel="manifest" href="/manifest.json">
    <meta name="theme-color" content="#059669">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-status-bar-style" content="default">
    <meta name="apple-mobile-web-app-title" content="Henny">
    <link rel="apple-touch-icon" href="/icon-192.png">
    <script src="https://cdn.tailwindcss.com"></script>
    <script src="https://unpkg.com/lucide@latest/dist/umd/lucide.js"></script>
    <style>
        .slider::-webkit-slider-thumb {
            appearance: none;
            height: 20px;
            width: 20px;
            border-radius: 50%;
            background: #4ade80;
            cursor: pointer;
            box-shadow: 0 0 2px 0 #555;
        }
        .slider::-moz-range-thumb {
            height: 20px;
            width: 20px;
            border-radius: 50%;
            background: #4ade80;
            cursor: pointer;
            border: none;
            box-shadow: 0 0 2px 0 #555;
        }
    </style>
    <script>
        tailwind.config = {
            theme: {
                extend: {
                    colors: {
                        primary: '#4ade80',
                        secondary: '#22c55e',
                        accent: '#86efac',
                        sage: '#94a3b8',
                        'green-soft': '#ecfdf5',
                        'green-card': '#6ee7b7'
                    }
                }
            }
        }
    </script>
</head>
<body class="min-h-screen" style="background: #415554;">
    <div class="container mx-auto px-4 py-8 max-w-4xl">
        <!-- Header -->
        <div class="bg-gradient-to-br from-emerald-600 to-teal-700 rounded-2xl shadow-xl p-6 mb-6 relative overflow-hidden">
            <!-- Decorative elements -->
            <div class="absolute top-0 right-0 w-32 h-32 bg-white/10 rounded-full -translate-y-16 translate-x-16"></div>
            <div class="absolute bottom-0 left-0 w-24 h-24 bg-white/5 rounded-full translate-y-12 -translate-x-12"></div>
            <div class="absolute top-1/2 right-1/4 w-6 h-6 bg-white/20 rounded-full"></div>
            <div class="absolute top-1/4 right-1/3 w-3 h-3 bg-white/15 rounded-full"></div>
            
            <div class="flex items-center justify-between relative z-10">
                <div>
                    <h1 class="text-3xl font-bold text-white flex items-center gap-3">
                        <i data-lucide="bird" class="w-8 h-8 text-emerald-200"></i>
                        Henny
                    </h1>
                    <p class="text-emerald-100 text-sm mt-1">{SUBTITLE}</p>
                </div>
                <div class="flex gap-3">
                    <button id="test-motor-btn" class="bg-white/20 hover:bg-white/30 backdrop-blur-sm text-white p-3 rounded-xl transition-all shadow-lg hover:shadow-xl border border-white/20" title="Motor Test (3s)">
                        <i data-lucide="zap" class="w-5 h-5"></i>
                    </button>
                    <button id="language-btn" class="bg-white/20 hover:bg-white/30 backdrop-blur-sm text-white p-3 rounded-xl transition-all shadow-lg hover:shadow-xl border border-white/20" title="Language / Sprache">
                        <span class="text-sm font-medium">{LANGUAGE_DISPLAY}</span>
                    </button>
                    <button id="install-btn" class="bg-white/20 hover:bg-white/30 backdrop-blur-sm text-white p-3 rounded-xl transition-all shadow-lg hover:shadow-xl border border-white/20 hidden" title="Install App">
                        <i data-lucide="download" class="w-5 h-5"></i>
                    </button>
                    <button id="settings-btn" class="bg-white/20 hover:bg-white/30 backdrop-blur-sm text-white p-3 rounded-xl transition-all shadow-lg hover:shadow-xl border border-white/20" title="Settings / Einstellungen">
                        <i data-lucide="settings" class="w-5 h-5"></i>
                    </button>
                </div>
            </div>
        </div>

        <!-- Dashboard Grid -->
        <div id="dashboard-grid" class="grid md:grid-cols-2 gap-6 mb-8">
            <!-- Today's Feeding Schedule -->
            <div class="bg-gradient-to-br from-white to-emerald-50 rounded-2xl shadow-xl border border-emerald-200/30 p-6 md:order-2">
                <div class="flex items-center justify-between mb-4">
                    <h3 class="text-lg font-semibold text-gray-800">{FEEDING_SCHEDULE_TITLE}</h3>
                    <i data-lucide="calendar" class="w-6 h-6 text-gray-500"></i>
                </div>
                <table class="w-full">
                    <tbody>
                        <tr class="border-b border-gray-100">
                            <td class="text-gray-500 text-sm text-right py-2 pr-3">{SUNRISE}</td>
                            <td class="text-gray-500 text-sm py-2 px-3">
                                <span class="flex items-center gap-1">
                                    <i data-lucide="sunrise" class="w-4 h-4"></i>
                                    {SUNRISE_TEXT}
                                </span>
                            </td>
                            <td class="py-2 px-3"></td>
                            <td class="py-2 pl-3"></td>
                        </tr>
                        <tbody id="feeding-schedule">
                            <!-- This will be populated by JavaScript -->
                        </tbody>
                        <tr class="border-t border-gray-100">
                            <td class="text-gray-500 text-sm text-right py-2 pr-3">{SUNSET}</td>
                            <td class="text-gray-500 text-sm py-2 px-3">
                                <span class="flex items-center gap-1">
                                    <i data-lucide="sunset" class="w-4 h-4"></i>
                                    {SUNSET_TEXT}
                                </span>
                            </td>
                            <td class="py-2 px-3"></td>
                            <td class="py-2 pl-3"></td>
                        </tr>
                    </tbody>
                </table>
            </div>

            <!-- System Status Card -->
            <div class="bg-gradient-to-br from-white to-green-soft rounded-2xl shadow-xl border border-green-200/30 p-6 md:order-1">
                <div class="flex items-center justify-between mb-4">
                    <h3 class="text-lg font-semibold text-gray-800">{SYSTEM_STATUS_TITLE}</h3>
                    <i data-lucide="activity" class="w-6 h-6 text-gray-500"></i>
                </div>
                <div class="space-y-3">
                    <div class="flex justify-between">
                        <span class="text-gray-600">{ADULT_CHICKENS_TEXT}</span>
                        <span class="font-medium" id="adults">{ADULTS}</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">{CALIBRATION_TEXT}</span>
                        <span class="font-medium" id="calibration">{CALIBRATION}g/10s</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">WiFi</span>
                        <span class="font-medium text-green-600" id="wifi-status">{WIFI_NETWORK}</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">{TIME_TEXT}</span>
                        <span class="font-medium" id="current-time">{CURRENT_TIME}</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">{DAILY_FEED_TEXT}</span>
                        <span class="font-medium">{DAILY_FEED}g</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">{MONTHLY_FEED_TEXT}</span>
                        <span class="font-medium">{MONTHLY_FEED}kg</span>
                    </div>
                </div>
            </div>
        </div>


        <!-- Settings Panel (Initially Hidden) -->
        <div id="settings-panel" class="hidden space-y-6">
            <!-- Configuration -->
            <div class="bg-gradient-to-br from-white to-green-soft rounded-2xl shadow-xl border border-green-200/30 p-6">
                <h3 class="text-xl font-semibold text-gray-800 mb-4 flex items-center gap-2">
                    <i data-lucide="bird" class="w-6 h-6 text-gray-500"></i>
                    {CHICKEN_CONFIG_TITLE}
                </h3>
                <div class="space-y-4">
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">{ADULT_CHICKENS_LABEL}: <span id="chickenCountDisplay">{ADULTS}</span></label>
                        <input type="range" id="adultCount" min="0" max="30" value="{ADULTS}"
                               class="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer slider"
                               oninput="updateChickenDisplay(this.value)">
                    </div>
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">{FEED_PER_CHICKEN_LABEL}: <span id="feedAmountDisplay">{FEED_AMOUNT}</span>g</label>
                        <input type="range" id="feedAmount" min="80" max="200" value="{FEED_AMOUNT}"
                               class="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer slider"
                               oninput="updateFeedAmountDisplay(this.value)">
                    </div>
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">{FEEDINGS_PER_DAY_LABEL}: <span id="feedFrequencyDisplay">{FEED_FREQUENCY}</span></label>
                        <input type="range" id="feedFrequency" min="1" max="8" value="{FEED_FREQUENCY}"
                               class="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer slider"
                               oninput="updateFeedFrequencyDisplay(this.value)">
                    </div>
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">{FIRST_FEEDING_LABEL}: <span id="sunriseOffsetDisplay">{SUNRISE_OFFSET}</span>h {AFTER_SUNRISE_TEXT}</label>
                        <input type="range" id="sunriseOffset" min="1" max="4" value="{SUNRISE_OFFSET}"
                               class="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer slider"
                               oninput="updateSunriseOffsetDisplay(this.value)">
                    </div>
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">{LAST_FEEDING_LABEL}: <span id="sunsetOffsetDisplay">{SUNSET_OFFSET}</span>h {BEFORE_SUNSET_TEXT}</label>
                        <input type="range" id="sunsetOffset" min="1" max="4" value="{SUNSET_OFFSET}"
                               class="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer slider"
                               oninput="updateSunsetOffsetDisplay(this.value)">
                    </div>
                    <div class="flex justify-center">
                        <button id="update-config-btn" class="bg-emerald-500 hover:bg-emerald-600 text-white font-medium py-3 px-8 rounded-xl transition-all shadow-lg hover:shadow-xl">
                            {UPDATE_BUTTON_TEXT}
                        </button>
                    </div>
                </div>
            </div>

            <!-- Calibration -->
            <div class="bg-gradient-to-br from-white to-emerald-50 rounded-2xl shadow-xl border border-emerald-200/30 p-6">
                <h3 class="text-xl font-semibold text-gray-800 mb-4 flex items-center gap-2">
                    <i data-lucide="scale" class="w-6 h-6 text-gray-500"></i>
                    {CALIBRATION_TITLE}
                </h3>
                <div class="space-y-4">
                    <p class="text-gray-600 text-sm">F&uuml;hren Sie einen 10-Sekunden-Kalibrierungstest durch, messen Sie dann die tats&auml;chlich ausgegebene Menge und geben Sie diese ein.</p>
                    <div class="grid md:grid-cols-3 gap-4 items-end">
                        <div>
                            <label class="block text-sm font-medium text-gray-700 mb-2">Gemessene Menge (g)</label>
                            <input type="number" id="calValue" placeholder="Ausgegebene Gramm" step="0.1"
                                   class="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-primary focus:border-transparent">
                        </div>
                        <button id="calibrate-btn" class="bg-slate-500 hover:bg-slate-600 text-white font-medium py-3 px-6 rounded-xl transition-all shadow-lg hover:shadow-xl">
                            Test starten
                        </button>
                        <button id="set-calibration-btn" class="bg-emerald-500 hover:bg-emerald-600 text-white font-medium py-3 px-6 rounded-xl transition-all shadow-lg hover:shadow-xl">
                            Kalibrierung speichern
                        </button>
                    </div>
                </div>
            </div>

            <!-- Timezone Configuration -->
            <div class="bg-gradient-to-br from-white to-teal-50 rounded-2xl shadow-xl border border-teal-200/30 p-6">
                <h3 class="text-xl font-semibold text-gray-800 mb-4 flex items-center gap-2">
                    <i data-lucide="clock" class="w-6 h-6 text-gray-500"></i>
                    Zeitzone
                </h3>
                <div class="space-y-4">
                    <div class="bg-blue-50 border border-blue-200 rounded-lg p-3">
                        <div class="text-sm font-medium text-blue-800">Aktuelle Zeit</div>
                        <div class="text-blue-600">{CURRENT_TIME}</div>
                    </div>
                    <div class="grid md:grid-cols-2 gap-4">
                        <div>
                            <label class="block text-sm font-medium text-gray-700 mb-2">Zeitzone</label>
                            <select id="timezone" class="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-primary focus:border-transparent">
                                <option value="CET-1CEST,M3.5.0,M10.5.0/3">Europa/Berlin (MEZ/MESZ)</option>
                                <option value="GMT0BST,M3.5.0/1,M10.5.0">Europa/London (GMT/BST)</option>
                                <option value="CET-1CEST,M3.5.0/2,M10.5.0/3">Europa/Paris (MEZ/MESZ)</option>
                                <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Europa/Helsinki (OEZ/OESZ)</option>
                                <option value="EST5EDT,M3.2.0,M11.1.0">Amerika/New_York (EST/EDT)</option>
                                <option value="PST8PDT,M3.2.0,M11.1.0">Amerika/Los_Angeles (PST/PDT)</option>
                                <option value="JST-9">Asien/Tokio (JST)</option>
                            </select>
                        </div>
                        <div class="flex items-end">
                            <button id="update-timezone-btn" class="bg-primary hover:bg-secondary text-white font-medium py-2 px-6 rounded-lg transition-colors">
                                Zeitzone speichern
                            </button>
                        </div>
                    </div>
                </div>
            </div>

            <!-- WiFi Configuration -->
            <div class="bg-gradient-to-br from-white to-blue-50 rounded-2xl shadow-xl border border-blue-200/30 p-6">
                <h3 class="text-xl font-semibold text-gray-800 mb-4 flex items-center gap-2">
                    <i data-lucide="wifi" class="w-6 h-6 text-gray-500"></i>
                    WLAN-Konfiguration
                </h3>
                <div class="space-y-4">
                    <div class="bg-blue-50 border border-blue-200 rounded-lg p-3">
                        <div class="text-sm font-medium text-blue-800">Aktuelle Verbindung</div>
                        <div class="text-blue-600">{WIFI_INFO}</div>
                    </div>
                    <div class="grid md:grid-cols-2 gap-4">
                        <div>
                            <label class="block text-sm font-medium text-gray-700 mb-2">Netzwerkname (SSID)</label>
                            <input type="text" id="wifiSSID" placeholder="WLAN-Netzwerkname"
                                   class="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-primary focus:border-transparent">
                        </div>
                        <div>
                            <label class="block text-sm font-medium text-gray-700 mb-2">Passwort</label>
                            <input type="password" id="wifiPassword" placeholder="WLAN-Passwort"
                                   class="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-primary focus:border-transparent">
                        </div>
                    </div>
                    <button id="update-wifi-btn" class="bg-blue-500 hover:bg-blue-600 text-white font-medium py-2 px-6 rounded-lg transition-colors">
                        WLAN speichern & neustarten
                    </button>
                </div>
            </div>

            <!-- Firmware Update -->
            <div class="bg-gradient-to-br from-white to-purple-50 rounded-2xl shadow-xl border border-purple-200/30 p-6">
                <h3 class="text-xl font-semibold text-gray-800 mb-4 flex items-center gap-2">
                    <i data-lucide="download" class="w-6 h-6 text-gray-500"></i>
                    Firmware-Update
                </h3>
                <div class="space-y-4">
                    <div class="bg-purple-50 border border-purple-200 rounded-lg p-3">
                        <div class="text-sm font-medium text-purple-800">Aktuelle Version</div>
                        <div class="text-purple-600">Henny v2.0 - Built {BUILD_DATE}</div>
                    </div>
                    <div class="bg-yellow-50 border border-yellow-200 rounded-lg p-3">
                        <div class="text-sm font-medium text-yellow-800">⚠️ Hinweis</div>
                        <div class="text-yellow-700 text-sm">Laden Sie nur offizielle Firmware-Dateien (.bin) hoch. Während des Updates darf die Stromversorgung nicht unterbrochen werden.</div>
                    </div>
                    <a href="/update" class="inline-block bg-purple-500 hover:bg-purple-600 text-white font-medium py-2 px-6 rounded-lg transition-colors">
                        Firmware aktualisieren
                    </a>
                </div>
            </div>
        </div>
    </div>

    <script>
        // Language definitions
        const translations = {
            de: {
                motorTestStarted: 'Motor-Test gestartet (3 Sekunden)',
                motorTestFailed: 'Motor-Test fehlgeschlagen',
                calibrationStarted: 'Kalibrierung gestartet! Messen Sie die ausgegebene Menge und geben Sie diese unten ein.',
                calibrationFailed: 'Kalibrierung fehlgeschlagen. Bitte erneut versuchen.',
                calibrationUpdated: 'Kalibrierung aktualisiert!',
                calibrationUpdateFailed: 'Kalibrierung konnte nicht aktualisiert werden.',
                validCalibrationValue: 'Bitte geben Sie einen gültigen Kalibrierungswert ein.',
                configUpdated: 'Konfiguration aktualisiert!',
                configUpdateFailed: 'Konfiguration konnte nicht aktualisiert werden.',
                completed: 'Erledigt',
                pending: 'Ausstehend',
                scheduled: 'Geplant'
            },
            en: {
                motorTestStarted: 'Motor test started (3 seconds)',
                motorTestFailed: 'Motor test failed',
                calibrationStarted: 'Calibration started! Measure the dispensed amount and enter it below.',
                calibrationFailed: 'Calibration failed. Please try again.',
                calibrationUpdated: 'Calibration updated!',
                calibrationUpdateFailed: 'Could not update calibration.',
                validCalibrationValue: 'Please enter a valid calibration value.',
                configUpdated: 'Configuration updated!',
                configUpdateFailed: 'Could not update configuration.',
                completed: 'Completed',
                pending: 'Pending',
                scheduled: 'Scheduled'
            }
        };
        
        const lang = translations['{LANGUAGE}'] || translations['de'];
        
        function toggleSettings() {
            const panel = document.getElementById('settings-panel');
            const dashboard = document.getElementById('dashboard-grid');
            
            panel.classList.toggle('hidden');
            dashboard.classList.toggle('hidden');
        }
        
        async function testMotor() {
            try {
                await fetch('/test-motor');
                showNotification(lang.motorTestStarted, 'info');
            } catch (error) {
                showNotification(lang.motorTestFailed, 'error');
            }
        }

        
        async function calibrate() {
            try {
                await fetch('/calibrate');
                showNotification(lang.calibrationStarted, 'info');
            } catch (error) {
                showNotification(lang.calibrationFailed, 'error');
            }
        }
        
        async function setCalibration() {
            const value = document.getElementById('calValue').value;
            if (value && value > 0) {
                try {
                    await fetch('/setcal?value=' + value);
                    showNotification(lang.calibrationUpdated, 'success');
                    setTimeout(() => location.reload(), 1500);
                } catch (error) {
                    showNotification(lang.calibrationUpdateFailed, 'error');
                }
            } else {
                showNotification(lang.validCalibrationValue, 'error');
            }
        }
        
        async function toggleLanguage() {
            const currentLang = '{LANGUAGE}';
            const newLang = currentLang === 'de' ? 'en' : 'de';
            try {
                await fetch('/config?language=' + newLang);
                location.reload();
            } catch (error) {
                console.error('Language toggle failed:', error);
            }
        }
        
        async function updateConfig() {
            const adults = document.getElementById('adultCount').value;
            const feedAmount = document.getElementById('feedAmount').value;
            const feedingFrequency = document.getElementById('feedFrequency').value;
            const sunriseOffset = document.getElementById('sunriseOffset').value;
            const sunsetOffset = document.getElementById('sunsetOffset').value;
            if (adults >= 0 && feedAmount >= 80 && feedAmount <= 200 && feedingFrequency >= 1 && feedingFrequency <= 8 && sunriseOffset >= 1 && sunriseOffset <= 4 && sunsetOffset >= 1 && sunsetOffset <= 4) {
                try {
                    await fetch('/config?adults=' + adults + '&feedAmount=' + feedAmount + '&feedFrequency=' + feedingFrequency + '&sunriseOffset=' + sunriseOffset + '&sunsetOffset=' + sunsetOffset);
                    showNotification(lang.configUpdated, 'success');
                    setTimeout(() => location.reload(), 1500);
                } catch (error) {
                    showNotification(lang.configUpdateFailed, 'error');
                }
            }
        }
        
        async function updateTimezone() {
            const timezone = document.getElementById('timezone').value;
            
            if (confirm('Zeitzone ändern? Das Gerät wird neu gestartet.')) {
                try {
                    await fetch('/timezone', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                        body: 'timezone=' + encodeURIComponent(timezone)
                    });
                    showNotification('Zeitzone gespeichert! Gerät startet neu...', 'success');
                } catch (error) {
                    showNotification('Zeitzone konnte nicht gespeichert werden.', 'error');
                }
            }
        }
        
        async function updateWiFi() {
            const ssid = document.getElementById('wifiSSID').value.trim();
            const password = document.getElementById('wifiPassword').value;
            
            if (!ssid) {
                showNotification('Bitte geben Sie einen Netzwerknamen ein.', 'error');
                return;
            }
            
            if (confirm('WLAN-Einstellungen speichern und neu starten? Das Gerät wird sich verbinden mit: ' + ssid)) {
                try {
                    await fetch('/wifi', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                        body: 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(password)
                    });
                    showNotification('WLAN-Einstellungen gespeichert! Gerät startet neu...', 'success');
                } catch (error) {
                    showNotification('WLAN-Einstellungen konnten nicht gespeichert werden.', 'error');
                }
            }
        }
        
        function showNotification(message, type) {
            const colors = {
                success: 'bg-green-500',
                error: 'bg-red-500',
                info: 'bg-blue-500'
            };
            
            const notification = document.createElement('div');
            notification.className = `fixed top-4 right-4 ${colors[type]} text-white px-6 py-3 rounded-lg shadow-lg z-50 transform translate-x-full transition-transform duration-300`;
            notification.textContent = message;
            
            document.body.appendChild(notification);
            
            setTimeout(() => notification.classList.remove('translate-x-full'), 100);
            setTimeout(() => {
                notification.classList.add('translate-x-full');
                setTimeout(() => document.body.removeChild(notification), 300);
            }, 1000);
        }
        
        function updateChickenDisplay(value) {
            document.getElementById('chickenCountDisplay').textContent = value;
        }
        
        function updateFeedAmountDisplay(value) {
            document.getElementById('feedAmountDisplay').textContent = value;
        }
        
        function updateFeedFrequencyDisplay(value) {
            document.getElementById('feedFrequencyDisplay').textContent = value;
        }
        
        function updateSunriseOffsetDisplay(value) {
            document.getElementById('sunriseOffsetDisplay').textContent = value;
        }
        
        function updateSunsetOffsetDisplay(value) {
            document.getElementById('sunsetOffsetDisplay').textContent = value;
        }
        
        function updateFeedingSchedule() {
            // Get current time
            const now = new Date();
            const currentHour = now.getHours();
            const currentMinute = now.getMinutes();
            
            // Generate schedule based on feeding frequency and sunrise/sunset offsets
            const configuredFrequency = {FEED_FREQUENCY};
            const sunriseOffsetHours = {SUNRISE_OFFSET};
            const sunsetOffsetHours = {SUNSET_OFFSET};
            let currentSchedule = [];
            
            // Calculate sunrise and sunset times (simplified for demo)
            const dayOfYear = Math.floor((now - new Date(now.getFullYear(), 0, 0)) / 86400000);
            const angle = (dayOfYear - 172) * 2.0 * Math.PI / 365.0;
            const sunriseHour = Math.floor(7.0 - 1.5 * Math.cos(angle)); // Between 5.5 and 8.5
            const sunsetHour = Math.floor(19.0 + 2.5 * Math.cos(angle)); // Between 16.5 and 21.5
            
            // Generate feeding times based on offsets
            const startHour = sunriseHour + sunriseOffsetHours;
            const endHour = sunsetHour - sunsetOffsetHours;
            const totalHours = Math.max(1, endHour - startHour); // Ensure at least 1 hour window
            
            if (configuredFrequency === 1) {
                currentSchedule.push({hour: startHour + Math.floor(totalHours / 2), minute: 0});
            } else {
                for (let i = 0; i < configuredFrequency; i++) {
                    const position = i / (configuredFrequency - 1);
                    const hour = startHour + Math.floor(totalHours * position);
                    currentSchedule.push({hour: hour, minute: 0});
                }
            }
            
            // Calculate feed amount per feeding
            const adultChickens = {ADULTS};
            const feedPerChicken = {FEED_AMOUNT};
            const dailyTotal = adultChickens * feedPerChicken;
            const perFeeding = Math.round(dailyTotal / configuredFrequency);
            
            // Calculate runtime based on calibration (grams per 10 seconds)
            const calibration = {CALIBRATION}; // grams per 10 seconds
            const gramsPerSecond = calibration / 10;
            const runtimeSeconds = Math.round(perFeeding / gramsPerSecond);
            
            // Generate schedule HTML
            const scheduleContainer = document.getElementById('feeding-schedule');
            scheduleContainer.innerHTML = '';
            
            currentSchedule.forEach(feeding => {
                const feedingTime = feeding.hour * 100 + feeding.minute;
                const currentTime = currentHour * 100 + currentMinute;
                
                let status, statusClass;
                if (feedingTime < currentTime - 5) {
                    status = lang.completed;
                    statusClass = 'bg-green-100 text-green-800';
                } else if (feedingTime <= currentTime + 5 && feedingTime >= currentTime - 5) {
                    status = lang.pending;
                    statusClass = 'bg-yellow-100 text-yellow-800';
                } else {
                    status = lang.scheduled;
                    statusClass = 'bg-gray-100 text-gray-600';
                }
                
                const timeStr = feeding.hour.toString().padStart(2, '0') + ':' + feeding.minute.toString().padStart(2, '0');
                
                const feedingRow = document.createElement('tr');
                feedingRow.innerHTML = `
                    <td class="text-gray-600 font-medium text-right py-2 pr-3">${timeStr}</td>
                    <td class="text-xs text-gray-500 font-medium text-center py-2 px-3">${perFeeding}g</td>
                    <td class="text-xs text-gray-400 font-medium text-center py-2 px-3">${runtimeSeconds}s</td>
                    <td class="py-2 pl-3">
                        <span class="text-sm ${statusClass} px-3 py-1 rounded-full">${status}</span>
                    </td>
                `;
                scheduleContainer.appendChild(feedingRow);
            });
        }
        
        // Initialize everything when DOM is ready
        document.addEventListener('DOMContentLoaded', function() {
            // Initialize Lucide icons
            lucide.createIcons();
            
            // Setup event listeners
            document.getElementById('test-motor-btn').addEventListener('click', testMotor);
            document.getElementById('language-btn').addEventListener('click', toggleLanguage);
            document.getElementById('install-btn').addEventListener('click', showInstallPrompt);
            document.getElementById('settings-btn').addEventListener('click', toggleSettings);
            document.getElementById('update-config-btn').addEventListener('click', updateConfig);
            document.getElementById('calibrate-btn').addEventListener('click', calibrate);
            document.getElementById('set-calibration-btn').addEventListener('click', setCalibration);
            document.getElementById('update-timezone-btn').addEventListener('click', updateTimezone);
            document.getElementById('update-wifi-btn').addEventListener('click', updateWiFi);
            
            // Update feeding schedule
            updateFeedingSchedule();
        });
        
        // PWA Install functionality
        let deferredPrompt;
        
        window.addEventListener('beforeinstallprompt', (e) => {
            e.preventDefault();
            deferredPrompt = e;
            document.getElementById('install-btn').classList.remove('hidden');
        });
        
        function showInstallPrompt() {
            if (deferredPrompt) {
                deferredPrompt.prompt();
                deferredPrompt.userChoice.then((choiceResult) => {
                    if (choiceResult.outcome === 'accepted') {
                        showNotification('App installed successfully!', 'success');
                    } else {
                        showNotification('App installation declined', 'info');
                    }
                    deferredPrompt = null;
                    document.getElementById('install-btn').classList.add('hidden');
                });
            } else {
                showNotification('App is already installed or not supported', 'info');
            }
        }
        
        // Register service worker
        if ('serviceWorker' in navigator) {
            window.addEventListener('load', () => {
                navigator.serviceWorker.register('/sw.js')
                    .then((registration) => {
                        console.log('SW registered: ', registration);
                    })
                    .catch((registrationError) => {
                        console.log('SW registration failed: ', registrationError);
                    });
            });
        }
        
        // Fallback for immediate loading
        if (document.readyState !== 'loading') {
            lucide.createIcons();
            document.getElementById('test-motor-btn')?.addEventListener('click', testMotor);
            document.getElementById('language-btn')?.addEventListener('click', toggleLanguage);
            document.getElementById('install-btn')?.addEventListener('click', showInstallPrompt);
            document.getElementById('settings-btn')?.addEventListener('click', toggleSettings);
            document.getElementById('update-config-btn')?.addEventListener('click', updateConfig);
            document.getElementById('calibrate-btn')?.addEventListener('click', calibrate);
            document.getElementById('set-calibration-btn')?.addEventListener('click', setCalibration);
            document.getElementById('update-timezone-btn')?.addEventListener('click', updateTimezone);
            document.getElementById('update-wifi-btn')?.addEventListener('click', updateWiFi);
            updateFeedingSchedule();
        }
    </script>
</body>
</html>
)HTML";

    // Get current time
    String currentTime = "---";
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char timeStr[6];
        strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
        currentTime = String(timeStr);
    }
    
    // Calculate feed amounts
    float dailyFeed = scheduler.getDailyFeedAmount(adultChickens, feedAmountPerChicken);
    float monthlyFeed = dailyFeed * 30.0 / 1000.0; // Convert to kg
    
    // Get build date
    String buildDate = String(__DATE__) + " " + String(__TIME__);
    
    // Replace placeholders
    html.replace("{LANG}", language);
    html.replace("{LANGUAGE}", language);
    html.replace("{ADULTS}", String(adultChickens));
    html.replace("{FEED_AMOUNT}", String(feedAmountPerChicken));
    html.replace("{FEED_FREQUENCY}", String(feedFrequency));
    html.replace("{SUNRISE_OFFSET}", String(sunriseOffset));
    html.replace("{SUNSET_OFFSET}", String(sunsetOffset));
    html.replace("{CALIBRATION}", String(spreader.getCalibration()));
    html.replace("{WIFI_NETWORK}", WiFi.isConnected() ? WiFi.SSID() : (language == "en" ? "AP Mode" : "AP-Modus"));
    html.replace("{WIFI_INFO}", WiFi.isConnected() ? WiFi.SSID() + (language == "en" ? " (Connected)" : " (Verbunden)") : (language == "en" ? "AP Mode: Henny-Setup" : "AP-Modus: Henny-Setup"));
    html.replace("{SUNRISE}", scheduler.getSunriseTime());
    html.replace("{SUNSET}", scheduler.getSunsetTime());
    html.replace("{CURRENT_TIME}", currentTime);
    html.replace("{DAILY_FEED}", String((int)dailyFeed));
    html.replace("{MONTHLY_FEED}", String(monthlyFeed, 1));
    html.replace("{BUILD_DATE}", buildDate);
    
    // Replace translation placeholders
    html.replace("{SUBTITLE}", getTranslation("subtitle", language));
    html.replace("{FEEDING_SCHEDULE_TITLE}", getTranslation("feeding_schedule_title", language));
    html.replace("{SUNRISE_TEXT}", getTranslation("sunrise_text", language));
    html.replace("{SUNSET_TEXT}", getTranslation("sunset_text", language));
    html.replace("{SYSTEM_STATUS_TITLE}", getTranslation("system_status_title", language));
    html.replace("{ADULT_CHICKENS_TEXT}", getTranslation("adult_chickens_text", language));
    html.replace("{CALIBRATION_TEXT}", getTranslation("calibration_text", language));
    html.replace("{TIME_TEXT}", getTranslation("time_text", language));
    html.replace("{DAILY_FEED_TEXT}", getTranslation("daily_feed_text", language));
    html.replace("{MONTHLY_FEED_TEXT}", getTranslation("monthly_feed_text", language));
    html.replace("{CHICKEN_CONFIG_TITLE}", getTranslation("chicken_config_title", language));
    html.replace("{ADULT_CHICKENS_LABEL}", getTranslation("adult_chickens_label", language));
    html.replace("{FEED_PER_CHICKEN_LABEL}", getTranslation("feed_per_chicken_label", language));
    html.replace("{FEEDINGS_PER_DAY_LABEL}", getTranslation("feedings_per_day_label", language));
    html.replace("{FIRST_FEEDING_LABEL}", getTranslation("first_feeding_label", language));
    html.replace("{LAST_FEEDING_LABEL}", getTranslation("last_feeding_label", language));
    html.replace("{AFTER_SUNRISE_TEXT}", getTranslation("after_sunrise_text", language));
    html.replace("{BEFORE_SUNSET_TEXT}", getTranslation("before_sunset_text", language));
    html.replace("{UPDATE_BUTTON_TEXT}", getTranslation("update_button_text", language));
    html.replace("{CALIBRATION_TITLE}", getTranslation("calibration_title", language));
    html.replace("{CALIBRATION_INSTRUCTION}", getTranslation("calibration_instruction", language));
    html.replace("{MEASURED_AMOUNT_LABEL}", getTranslation("measured_amount_label", language));
    html.replace("{DISPENSED_GRAMS_PLACEHOLDER}", getTranslation("dispensed_grams_placeholder", language));
    html.replace("{START_TEST_BUTTON}", getTranslation("start_test_button", language));
    html.replace("{SAVE_CALIBRATION_BUTTON}", getTranslation("save_calibration_button", language));
    String langDisplay = language;
    langDisplay.toUpperCase();
    html.replace("{LANGUAGE_DISPLAY}", langDisplay);
    
    return html;
}

void handleRoot() {
    server.send(200, "text/html", generateHTML());
}

void handleFeed() {
    if (server.hasArg("amount")) {
        float amount = server.arg("amount").toFloat();
        spreader.spreadFeed(amount);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing amount");
    }
}

void handleCalibrate() {
    spreader.calibrationRun();
    server.send(200, "text/plain", "Calibration started");
}

void handleTestMotor() {
    spreader.testRun();
    server.send(200, "text/plain", "Motor test started");
}

void handleSetCalibration() {
    if (server.hasArg("value")) {
        float value = server.arg("value").toFloat();
        spreader.setCalibration(value);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing value");
    }
}

void handleConfig() {
    bool updated = false;
    
    if (server.hasArg("adults")) {
        adultChickens = server.arg("adults").toInt();
        preferences.putInt("adults", adultChickens);
        updated = true;
    }
    
    if (server.hasArg("feedAmount")) {
        feedAmountPerChicken = server.arg("feedAmount").toInt();
        preferences.putInt("feedAmount", feedAmountPerChicken);
        updated = true;
    }
    
    if (server.hasArg("feedFrequency")) {
        feedFrequency = server.arg("feedFrequency").toInt();
        preferences.putInt("feedFreq", feedFrequency);
        updated = true;
    }
    
    if (server.hasArg("sunriseOffset")) {
        sunriseOffset = server.arg("sunriseOffset").toInt();
        preferences.putInt("sunriseOff", sunriseOffset);
        updated = true;
    }
    
    if (server.hasArg("sunsetOffset")) {
        sunsetOffset = server.arg("sunsetOffset").toInt();
        preferences.putInt("sunsetOff", sunsetOffset);
        updated = true;
    }
    
    if (server.hasArg("language")) {
        language = server.arg("language");
        preferences.putString("lang", language);
        updated = true;
    }
    
    if (updated) {
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing parameters");
    }
}

void handleTimezoneConfig() {
    if (server.hasArg("timezone")) {
        String timezone = server.arg("timezone");
        
        preferences.putString("timezone", timezone);
        
        server.send(200, "text/plain", "Timezone settings saved! Restarting...");
        
        Serial.println("Timezone updated: " + timezone);
        Serial.println("Restarting in 2 seconds...");
        
        delay(2000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Missing timezone");
    }
}

void handleWiFiConfig() {
    if (server.hasArg("ssid")) {
        String ssid = server.arg("ssid");
        String password = server.arg("password");
        
        preferences.putString("ssid", ssid);
        preferences.putString("pass", password);
        
        server.send(200, "text/plain", "WiFi settings saved! Restarting...");
        
        Serial.println("WiFi settings updated:");
        Serial.println("SSID: " + ssid);
        Serial.println("Restarting in 2 seconds...");
        
        delay(2000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Missing SSID");
    }
}

void handleOTAUpload() {
    String html = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>Henny - Firmware Update</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <script src="https://cdn.tailwindcss.com"></script>
</head>
<body class="min-h-screen" style="background: #415554;">
    <div class="container mx-auto px-4 py-8 max-w-2xl">
        <div class="bg-gradient-to-br from-emerald-600 to-teal-700 rounded-2xl shadow-xl p-6 mb-6">
            <h1 class="text-3xl font-bold text-white text-center">Firmware Update</h1>
            <p class="text-emerald-100 text-center mt-2">Henny Chicken Feeder</p>
        </div>
        
        <div class="bg-white rounded-2xl shadow-xl p-6">
            <form method="POST" action="/update" enctype="multipart/form-data">
                <div class="mb-6">
                    <label class="block text-sm font-medium text-gray-700 mb-2">Firmware-Datei (.bin)</label>
                    <input type="file" name="update" accept=".bin" required
                           class="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-emerald-500 focus:border-transparent">
                </div>
                
                <div class="bg-yellow-50 border border-yellow-200 rounded-lg p-4 mb-6">
                    <h3 class="text-yellow-800 font-medium mb-2">⚠️ Wichtige Hinweise:</h3>
                    <ul class="text-yellow-700 text-sm space-y-1">
                        <li>• Laden Sie nur offizielle .bin Dateien hoch</li>
                        <li>• Unterbrechen Sie während des Updates nicht die Stromversorgung</li>
                        <li>• Das Gerät startet nach dem Update automatisch neu</li>
                        <li>• Der Vorgang dauert etwa 30-60 Sekunden</li>
                    </ul>
                </div>
                
                <button type="submit" class="w-full bg-emerald-500 hover:bg-emerald-600 text-white font-medium py-3 px-6 rounded-lg transition-colors">
                    Firmware aktualisieren
                </button>
            </form>
            
            <div class="mt-6 text-center">
                <a href="/" class="text-emerald-600 hover:text-emerald-700 font-medium">← Zurück zum Dashboard</a>
            </div>
        </div>
    </div>
</body>
</html>
)HTML";
    
    server.send(200, "text/html", html);
}

void handleOTAUpdate() {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update Start: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("Update Success: %uB\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

void handleOTAUpdatePost() {
    server.sendHeader("Connection", "close");
    if (Update.hasError()) {
        server.send(500, "text/html", "<h1>Update Failed!</h1><p>Check serial output for details.</p><a href='/update'>Try Again</a>");
    } else {
        server.send(200, "text/html", "<h1>Update Success!</h1><p>Device will restart in 3 seconds...</p><script>setTimeout(() => window.location.href='/', 5000);</script>");
        delay(3000);
        ESP.restart();
    }
}

void handleManifest() {
    String manifest = R"JSON({
  "name": "Henny Smart Chicken Feeder",
  "short_name": "Henny",
  "description": "Intelligent chicken feeding system with configurable schedules and remote monitoring",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#415554",
  "theme_color": "#059669",
  "orientation": "portrait-primary",
  "scope": "/",
  "icons": [
    {
      "src": "/icon-192.png",
      "sizes": "192x192",
      "type": "image/png",
      "purpose": "maskable any"
    },
    {
      "src": "/icon-512.png", 
      "sizes": "512x512",
      "type": "image/png",
      "purpose": "maskable any"
    }
  ],
  "categories": ["utilities", "productivity"]
})JSON";
    
    server.send(200, "application/manifest+json", manifest);
}

void handleServiceWorker() {
    String sw = R"JS(const CACHE_NAME = 'henny-v1';
const urlsToCache = [
  '/',
  '/manifest.json',
  '/icon-192.png',
  '/icon-512.png',
  'https://cdn.tailwindcss.com',
  'https://unpkg.com/lucide@latest/dist/umd/lucide.js'
];

self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(CACHE_NAME)
      .then(cache => {
        console.log('Opened cache');
        return cache.addAll(urlsToCache);
      })
  );
});

self.addEventListener('fetch', event => {
  event.respondWith(
    caches.match(event.request)
      .then(response => {
        return response || fetch(event.request);
      }
    )
  );
});

self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys().then(cacheNames => {
      return Promise.all(
        cacheNames.map(cacheName => {
          if (cacheName !== CACHE_NAME) {
            console.log('Deleting old cache:', cacheName);
            return caches.delete(cacheName);
          }
        })
      );
    })
  );
});)JS";
    
    server.send(200, "application/javascript", sw);
}

void setup() {
    Serial.begin(115200);
    delay(2000); // Wait for USB-CDC to be ready
    Serial.println("\nHenny Feeder v2.0 (C++)");
    Serial.println("Serial output working!");
    
    spreader.begin();
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    preferences.begin("henny", false);
    adultChickens = preferences.getInt("adults", 6);
    feedAmountPerChicken = preferences.getInt("feedAmount", 120);
    feedFrequency = preferences.getInt("feedFreq", 3);
    sunriseOffset = preferences.getInt("sunriseOff", 2);
    sunsetOffset = preferences.getInt("sunsetOff", 2);
    language = preferences.getString("lang", "de");
    spreader.setCalibration(preferences.getFloat("cal", 50.0));
    
    WiFi.begin(preferences.getString("ssid", "").c_str(), 
               preferences.getString("pass", "").c_str());
    
    Serial.print("Connecting to WiFi");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        
        // Set up mDNS hostname
        if (MDNS.begin("henny")) {
            Serial.println("mDNS responder started");
            Serial.println("You can now access the device at: http://henny.local");
            
            // Add service to mDNS-SD
            MDNS.addService("http", "tcp", 80);
            MDNS.addServiceTxt("http", "tcp", "model", "Henny Smart Chicken Feeder");
            MDNS.addServiceTxt("http", "tcp", "version", "v2.0");
        } else {
            Serial.println("Error setting up mDNS responder!");
        }
    } else {
        Serial.println("\nFailed to connect. Starting AP mode...");
        WiFi.softAP("Henny-Setup", "hennyfeeder");
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
        
        // Set up mDNS in AP mode too
        if (MDNS.begin("henny")) {
            Serial.println("mDNS responder started in AP mode");
            Serial.println("You can access the device at: http://henny.local");
            MDNS.addService("http", "tcp", 80);
        }
    }
    
    configTime(0, 0, "pool.ntp.org");
    String savedTimezone = preferences.getString("timezone", "CET-1CEST,M3.5.0,M10.5.0/3");
    setenv("TZ", savedTimezone.c_str(), 1);
    tzset();
    Serial.println("Timezone set to: " + savedTimezone);
    
    server.on("/", handleRoot);
    server.on("/feed", handleFeed);
    server.on("/calibrate", handleCalibrate);
    server.on("/test-motor", handleTestMotor);
    server.on("/setcal", handleSetCalibration);
    server.on("/config", handleConfig);
    server.on("/timezone", HTTP_POST, handleTimezoneConfig);
    server.on("/wifi", HTTP_POST, handleWiFiConfig);
    server.on("/update", HTTP_GET, handleOTAUpload);
    server.on("/update", HTTP_POST, handleOTAUpdatePost, handleOTAUpdate);
    server.on("/manifest.json", handleManifest);
    server.on("/sw.js", handleServiceWorker);
    server.begin();
    
    // Setup Arduino OTA
    ArduinoOTA.setHostname("henny-feeder");
    ArduinoOTA.setPassword("hennyfeeder");
    ArduinoOTA.begin();
    Serial.println("OTA Ready");
    
    Serial.println("Web server started");
}

void handleButton() {
    bool currentState = digitalRead(BUTTON_PIN);
    
    if (currentState == LOW && lastButtonState == HIGH) {
        buttonPressStart = millis();
        buttonPressed = true;
    }
    
    if (currentState == HIGH && lastButtonState == LOW && buttonPressed) {
        unsigned long pressDuration = millis() - buttonPressStart;
        
        if (pressDuration > BUTTON_LONG_PRESS_MS) {
            spreader.calibrationRun();
        } else {
            spreader.spreadFeed(25.0);
        }
        
        buttonPressed = false;
    }
    
    lastButtonState = currentState;
}

void loop() {
    spreader.update();
    handleButton();
    server.handleClient();
    ArduinoOTA.handle();
    
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 30000) {
        lastCheck = millis();
        
        float feedAmount;
        if (scheduler.shouldFeedNow(feedAmount, adultChickens, feedAmountPerChicken, feedFrequency, sunriseOffset, sunsetOffset)) {
            spreader.spreadFeed(feedAmount);
        }
    }
    
    delay(10);
}