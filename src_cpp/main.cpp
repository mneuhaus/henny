#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>

#define RELAY_PIN 5
#define LED_PIN 2
#define BUTTON_PIN 0

#define MOTOR_TIMEOUT_MS 30000
#define CALIBRATION_DURATION_MS 10000
#define BUTTON_LONG_PRESS_MS 3000

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
    
    void setCalibration(float gramsPerTenSeconds) {
        gramsPerSecond = gramsPerTenSeconds / 10.0;
        Serial.printf("Calibration set: %.2fg per second\n", gramsPerSecond);
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
    
public:
    float getDailyFeedAmount(int adultChickens, int chicksByAge[6]) {
        float total = adultChickens * 120.0;
        
        float ageMultipliers[6] = {0.125, 0.25, 0.42, 0.67, 0.67, 1.0};
        for (int i = 0; i < 6; i++) {
            total += chicksByAge[i] * 120.0 * ageMultipliers[i];
        }
        
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
        int month = 0;
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            month = timeinfo.tm_mon + 1;
        }
        
        if (month >= 6 && month <= 8) {
            count = 4;
            return summerSchedule;
        } else if (month == 12 || month <= 2) {
            count = 3;
            return winterSchedule;
        } else {
            count = 3;
            return springSchedule;
        }
    }
    
    bool shouldFeedNow(float &feedAmount, int adultChickens, int chicksByAge[6]) {
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
        float dailyTotal = getDailyFeedAmount(adultChickens, chicksByAge);
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
};

#include "WebServer.h"

Spreader spreader;
Scheduler scheduler;
Preferences preferences;
HennyWebServer* webServer;

int adultChickens = 6;
int chicksByAge[6] = {0, 0, 0, 0, 0, 0};

unsigned long buttonPressStart = 0;
bool buttonPressed = false;
bool lastButtonState = HIGH;

void setup() {
    Serial.begin(115200);
    Serial.println("\nHenny Feeder v2.0 (C++)");
    
    spreader.begin();
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    preferences.begin("henny", false);
    adultChickens = preferences.getInt("adults", 6);
    spreader.setCalibration(preferences.getFloat("cal", 50.0));
    
    WiFi.begin(preferences.getString("ssid", "").c_str(), 
               preferences.getString("pass", "").c_str());
    
    configTime(0, 0, "pool.ntp.org");
    setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
    tzset();
    
    webServer = new HennyWebServer(80, &spreader, &scheduler, &preferences);
    webServer->begin();
    Serial.println("Web server started on port 80");
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
    
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 30000) {
        lastCheck = millis();
        
        float feedAmount;
        if (scheduler.shouldFeedNow(feedAmount, adultChickens, chicksByAge)) {
            spreader.spreadFeed(feedAmount);
        }
    }
    
    delay(10);
}