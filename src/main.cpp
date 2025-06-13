#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
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
    
    bool fedToday[4] = {false, false, false, false};
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
    float getDailyFeedAmount(int adultChickens) {
        float total = adultChickens * 120.0;
        
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
    
    FeedingTime* getCurrentSchedule(int &count) {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            count = 3;
            return springSchedule; // Fallback
        }
        
        int month = timeinfo.tm_mon + 1;
        int dayOfYear = timeinfo.tm_yday;
        int sunsetHour = getSunsetHour(dayOfYear);
        int maxFeedingHour = sunsetHour - 2; // 2 hours before sunset
        
        FeedingTime* baseSchedule;
        int baseCount;
        
        if (month >= 6 && month <= 8) {
            baseSchedule = summerSchedule;
            baseCount = 4;
        } else if (month == 12 || month <= 2) {
            baseSchedule = winterSchedule;
            baseCount = 3;
        } else {
            baseSchedule = springSchedule;
            baseCount = 3;
        }
        
        // Adjust last feeding time if it's too close to sunset
        static FeedingTime adjustedSchedule[4];
        for (int i = 0; i < baseCount; i++) {
            adjustedSchedule[i] = baseSchedule[i];
            // If last feeding is within 2 hours of sunset, move it earlier
            if (i == baseCount - 1 && adjustedSchedule[i].hour >= maxFeedingHour) {
                adjustedSchedule[i].hour = maxFeedingHour;
                adjustedSchedule[i].minute = 0;
            }
        }
        
        count = baseCount;
        return adjustedSchedule;
    }
    
    bool shouldFeedNow(float &feedAmount, int adultChickens) {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return false;
        }
        
        int currentDay = timeinfo.tm_yday;
        if (currentDay != lastFeedDay) {
            memset(fedToday, false, sizeof(fedToday));
            lastFeedDay = currentDay;
        }
        
        int scheduleCount;
        FeedingTime* schedule = getCurrentSchedule(scheduleCount);
        float dailyTotal = getDailyFeedAmount(adultChickens);
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

unsigned long buttonPressStart = 0;
bool buttonPressed = false;
bool lastButtonState = HIGH;

String generateHTML() {
    String html = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Henny</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <script src="https://unpkg.com/lucide@latest/dist/umd/lucide.js"></script>
    <style>
        .slider::-webkit-slider-thumb {
            appearance: none;
            height: 20px;
            width: 20px;
            border-radius: 50%;
            background: #16a34a;
            cursor: pointer;
            box-shadow: 0 0 2px 0 #555;
        }
        .slider::-moz-range-thumb {
            height: 20px;
            width: 20px;
            border-radius: 50%;
            background: #16a34a;
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
                        primary: '#16a34a',
                        secondary: '#059669',
                    }
                }
            }
        }
    </script>
</head>
<body class="bg-gradient-to-br from-green-50 to-blue-50 min-h-screen">
    <div class="container mx-auto px-4 py-8 max-w-4xl">
        <!-- Header -->
        <div class="bg-white rounded-xl shadow-lg p-6 mb-6">
            <div class="flex items-center justify-between">
                <div>
                    <h1 class="text-3xl font-bold text-gray-800 flex items-center gap-3">
                        <i data-lucide="bird" class="w-8 h-8 text-primary"></i>
                        Henny
                    </h1>
                </div>
                <div class="flex gap-2">
                    <button onclick="testMotor()" class="bg-blue-500 hover:bg-blue-600 text-white p-2 rounded-lg transition-colors" title="Motor Test (3s)">
                        <i data-lucide="zap" class="w-5 h-5"></i>
                    </button>
                    <button onclick="toggleSettings()" class="bg-gray-500 hover:bg-gray-600 text-white p-2 rounded-lg transition-colors" title="Einstellungen">
                        <i data-lucide="settings" class="w-5 h-5"></i>
                    </button>
                </div>
            </div>
        </div>

        <!-- Dashboard Grid -->
        <div id="dashboard-grid" class="grid md:grid-cols-2 gap-6 mb-8">
            <!-- System Status Card -->
            <div class="bg-white rounded-xl shadow-lg p-6">
                <div class="flex items-center justify-between mb-4">
                    <h3 class="text-lg font-semibold text-gray-800">System-Status</h3>
                    <i data-lucide="activity" class="w-6 h-6 text-gray-500"></i>
                </div>
                <div class="space-y-3">
                    <div class="flex justify-between">
                        <span class="text-gray-600">Erwachsene H&uuml;hner</span>
                        <span class="font-medium" id="adults">{ADULTS}</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">Kalibrierung</span>
                        <span class="font-medium" id="calibration">{CALIBRATION}g/10s</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">WiFi</span>
                        <span class="font-medium text-green-600" id="wifi-status">{WIFI_NETWORK}</span>
                    </div>
                    <div class="flex justify-between">
                        <span class="text-gray-600">Zeit</span>
                        <span class="font-medium" id="current-time">{CURRENT_TIME}</span>
                    </div>
                </div>
            </div>

            <!-- Today's Feeding Schedule -->
            <div class="bg-white rounded-xl shadow-lg p-6">
                <div class="flex items-center justify-between mb-4">
                    <h3 class="text-lg font-semibold text-gray-800">Heutige F&uuml;tterungszeiten</h3>
                    <i data-lucide="calendar" class="w-6 h-6 text-gray-500"></i>
                </div>
                <div class="space-y-3">
                    <div class="flex justify-between items-center border-b border-gray-100 pb-2">
                        <span class="text-gray-500 text-sm flex items-center gap-1">
                            <i data-lucide="sunrise" class="w-4 h-4"></i>
                            Sonnenaufgang
                        </span>
                        <span class="text-gray-500 text-sm">{SUNRISE}</span>
                    </div>
                    <div id="feeding-schedule">
                        <!-- This will be populated by JavaScript -->
                    </div>
                    <div class="flex justify-between items-center border-t border-gray-100 pt-2">
                        <span class="text-gray-500 text-sm flex items-center gap-1">
                            <i data-lucide="sunset" class="w-4 h-4"></i>
                            Sonnenuntergang
                        </span>
                        <span class="text-gray-500 text-sm">{SUNSET}</span>
                    </div>
                </div>
            </div>
        </div>


        <!-- Settings Panel (Initially Hidden) -->
        <div id="settings-panel" class="hidden space-y-6">
            <!-- Configuration -->
            <div class="bg-white rounded-xl shadow-lg p-6">
                <h3 class="text-xl font-semibold text-gray-800 mb-4 flex items-center gap-2">
                    <i data-lucide="bird" class="w-6 h-6 text-gray-500"></i>
                    H&uuml;hner-Konfiguration
                </h3>
                <div class="space-y-4">
                    <div>
                        <label class="block text-sm font-medium text-gray-700 mb-2">Erwachsene H&uuml;hner: <span id="chickenCountDisplay">{ADULTS}</span></label>
                        <input type="range" id="adultCount" min="0" max="30" value="{ADULTS}"
                               class="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer slider"
                               oninput="updateChickenDisplay(this.value)">
                    </div>
                    <div class="flex justify-center">
                        <button onclick="updateConfig()" class="bg-primary hover:bg-secondary text-white font-medium py-2 px-6 rounded-lg transition-colors">
                            Aktualisieren
                        </button>
                    </div>
                </div>
            </div>

            <!-- Calibration -->
            <div class="bg-white rounded-xl shadow-lg p-6">
                <h3 class="text-xl font-semibold text-gray-800 mb-4 flex items-center gap-2">
                    <i data-lucide="scale" class="w-6 h-6 text-gray-500"></i>
                    Kalibrierung
                </h3>
                <div class="space-y-4">
                    <p class="text-gray-600 text-sm">F&uuml;hren Sie einen 10-Sekunden-Kalibrierungstest durch, messen Sie dann die tats&auml;chlich ausgegebene Menge und geben Sie diese ein.</p>
                    <div class="grid md:grid-cols-3 gap-4 items-end">
                        <div>
                            <label class="block text-sm font-medium text-gray-700 mb-2">Gemessene Menge (g)</label>
                            <input type="number" id="calValue" placeholder="Ausgegebene Gramm" step="0.1"
                                   class="w-full px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-primary focus:border-transparent">
                        </div>
                        <button onclick="calibrate()" class="bg-orange-500 hover:bg-orange-600 text-white font-medium py-2 px-6 rounded-lg transition-colors">
                            Test starten
                        </button>
                        <button onclick="setCalibration()" class="bg-primary hover:bg-secondary text-white font-medium py-2 px-6 rounded-lg transition-colors">
                            Kalibrierung speichern
                        </button>
                    </div>
                </div>
            </div>

            <!-- Timezone Configuration -->
            <div class="bg-white rounded-xl shadow-lg p-6">
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
                            <button onclick="updateTimezone()" class="bg-primary hover:bg-secondary text-white font-medium py-2 px-6 rounded-lg transition-colors">
                                Zeitzone speichern
                            </button>
                        </div>
                    </div>
                </div>
            </div>

            <!-- WiFi Configuration -->
            <div class="bg-white rounded-xl shadow-lg p-6">
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
                    <button onclick="updateWiFi()" class="bg-blue-500 hover:bg-blue-600 text-white font-medium py-2 px-6 rounded-lg transition-colors">
                        WLAN speichern & neustarten
                    </button>
                </div>
            </div>
        </div>
    </div>

    <script>
        function toggleSettings() {
            const panel = document.getElementById('settings-panel');
            const dashboard = document.getElementById('dashboard-grid');
            
            panel.classList.toggle('hidden');
            dashboard.classList.toggle('hidden');
        }
        
        async function testMotor() {
            try {
                await fetch('/test-motor');
                showNotification('Motor-Test gestartet (3 Sekunden)', 'info');
            } catch (error) {
                showNotification('Motor-Test fehlgeschlagen', 'error');
            }
        }

        
        async function calibrate() {
            try {
                await fetch('/calibrate');
                showNotification('Kalibrierung gestartet! Messen Sie die ausgegebene Menge und geben Sie diese unten ein.', 'info');
            } catch (error) {
                showNotification('Kalibrierung fehlgeschlagen. Bitte erneut versuchen.', 'error');
            }
        }
        
        async function setCalibration() {
            const value = document.getElementById('calValue').value;
            if (value && value > 0) {
                try {
                    await fetch('/setcal?value=' + value);
                    showNotification('Kalibrierung aktualisiert!', 'success');
                    setTimeout(() => location.reload(), 1500);
                } catch (error) {
                    showNotification('Kalibrierung konnte nicht aktualisiert werden.', 'error');
                }
            } else {
                showNotification('Bitte geben Sie einen gültigen Kalibrierungswert ein.', 'error');
            }
        }
        
        async function updateConfig() {
            const adults = document.getElementById('adultCount').value;
            if (adults >= 0) {
                try {
                    await fetch('/config?adults=' + adults);
                    showNotification('Konfiguration aktualisiert!', 'success');
                    setTimeout(() => location.reload(), 1500);
                } catch (error) {
                    showNotification('Konfiguration konnte nicht aktualisiert werden.', 'error');
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
            }, 3000);
        }
        
        function updateChickenDisplay(value) {
            document.getElementById('chickenCountDisplay').textContent = value;
        }
        
        function updateFeedingSchedule() {
            // Get current time
            const now = new Date();
            const currentHour = now.getHours();
            const currentMinute = now.getMinutes();
            
            // Mock schedule data - would come from server
            const schedules = {
                summer: [{hour: 6, minute: 0}, {hour: 10, minute: 0}, {hour: 15, minute: 0}, {hour: 19, minute: 0}],
                winter: [{hour: 8, minute: 0}, {hour: 13, minute: 0}, {hour: 17, minute: 0}],
                spring: [{hour: 7, minute: 0}, {hour: 12, minute: 0}, {hour: 17, minute: 0}]
            };
            
            // Get current month and determine season
            const month = now.getMonth() + 1;
            let currentSchedule;
            if (month >= 6 && month <= 8) {
                currentSchedule = schedules.summer;
            } else if (month === 12 || month <= 2) {
                currentSchedule = schedules.winter;
            } else {
                currentSchedule = schedules.spring;
            }
            
            // Calculate feed amount per feeding (120g per chicken / number of feedings)
            const adultChickens = {ADULTS};
            const dailyTotal = adultChickens * 120;
            const perFeeding = Math.round(dailyTotal / currentSchedule.length);
            
            // Generate schedule HTML
            const scheduleContainer = document.getElementById('feeding-schedule');
            scheduleContainer.innerHTML = '';
            
            currentSchedule.forEach(feeding => {
                const feedingTime = feeding.hour * 100 + feeding.minute;
                const currentTime = currentHour * 100 + currentMinute;
                
                let status, statusClass;
                if (feedingTime < currentTime - 5) {
                    status = 'Erledigt';
                    statusClass = 'bg-green-100 text-green-800';
                } else if (feedingTime <= currentTime + 5 && feedingTime >= currentTime - 5) {
                    status = 'Ausstehend';
                    statusClass = 'bg-yellow-100 text-yellow-800';
                } else {
                    status = 'Geplant';
                    statusClass = 'bg-gray-100 text-gray-600';
                }
                
                const timeStr = feeding.hour.toString().padStart(2, '0') + ':' + feeding.minute.toString().padStart(2, '0');
                
                const feedingDiv = document.createElement('div');
                feedingDiv.className = 'flex justify-between items-center';
                feedingDiv.innerHTML = `
                    <span class="text-gray-600">${timeStr}</span>
                    <div class="flex gap-2 items-center">
                        <span class="text-xs bg-blue-100 text-blue-800 px-2 py-1 rounded">${perFeeding}g</span>
                        <span class="text-sm ${statusClass} px-2 py-1 rounded">${status}</span>
                    </div>
                `;
                scheduleContainer.appendChild(feedingDiv);
            });
        }
        
        // Initialize
        updateFeedingSchedule();
        
        // Initialize Lucide icons
        document.addEventListener('DOMContentLoaded', function() {
            lucide.createIcons();
        });
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
    
    // Replace placeholders
    html.replace("{ADULTS}", String(adultChickens));
    html.replace("{CALIBRATION}", String(spreader.getCalibration()));
    html.replace("{WIFI_NETWORK}", WiFi.isConnected() ? WiFi.SSID() : "AP-Modus");
    html.replace("{WIFI_INFO}", WiFi.isConnected() ? WiFi.SSID() + " (Verbunden)" : "AP-Modus: Henny-Setup");
    html.replace("{SUNRISE}", scheduler.getSunriseTime());
    html.replace("{SUNSET}", scheduler.getSunsetTime());
    html.replace("{CURRENT_TIME}", currentTime);
    
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
    if (server.hasArg("adults")) {
        adultChickens = server.arg("adults").toInt();
        preferences.putInt("adults", adultChickens);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing adults");
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

void setup() {
    Serial.begin(115200);
    delay(2000); // Wait for USB-CDC to be ready
    Serial.println("\nHenny Feeder v2.0 (C++)");
    Serial.println("Serial output working!");
    
    spreader.begin();
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    preferences.begin("henny", false);
    adultChickens = preferences.getInt("adults", 6);
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
    } else {
        Serial.println("\nFailed to connect. Starting AP mode...");
        WiFi.softAP("Henny-Setup", "hennyfeeder");
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
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
    server.begin();
    
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
    
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 30000) {
        lastCheck = millis();
        
        float feedAmount;
        if (scheduler.shouldFeedNow(feedAmount, adultChickens)) {
            spreader.spreadFeed(feedAmount);
        }
    }
    
    delay(10);
}