#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>

class HennyWebServer {
private:
    AsyncWebServer server;
    Spreader* spreader;
    Scheduler* scheduler;
    Preferences* prefs;
    
public:
    HennyWebServer(int port, Spreader* s, Scheduler* sch, Preferences* p) 
        : server(port), spreader(s), scheduler(sch), prefs(p) {}
    
    void begin() {
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/html", getIndexHTML());
        });
        
        server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request){
            DynamicJsonDocument doc(512);
            
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                char timeStr[32];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
                doc["time"] = timeStr;
            }
            
            doc["adults"] = prefs->getInt("adults", 6);
            doc["calibration"] = prefs->getFloat("cal", 50.0);
            doc["wifi_connected"] = WiFi.isConnected();
            
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        });
        
        server.on("/api/feed", HTTP_POST, [this](AsyncWebServerRequest *request){
            if (request->hasParam("amount", true)) {
                float amount = request->getParam("amount", true)->value().toFloat();
                spreader->spreadFeed(amount);
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Missing amount");
            }
        });
        
        server.on("/api/calibrate", HTTP_POST, [this](AsyncWebServerRequest *request){
            spreader->calibrationRun();
            request->send(200, "text/plain", "Calibration started");
        });
        
        server.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest *request){
            if (request->hasParam("adults", true)) {
                int adults = request->getParam("adults", true)->value().toInt();
                prefs->putInt("adults", adults);
            }
            if (request->hasParam("calibration", true)) {
                float cal = request->getParam("calibration", true)->value().toFloat();
                prefs->putFloat("cal", cal);
                spreader->setCalibration(cal);
            }
            request->send(200, "text/plain", "Config updated");
        });
        
        server.begin();
    }
    
    static String getIndexHTML() {
        return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Henny Feeder</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 20px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
        h1 { color: #333; text-align: center; }
        .status { background: #e8f4f8; padding: 15px; border-radius: 5px; margin: 20px 0; }
        .control { margin: 20px 0; }
        button { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin: 5px; }
        button:hover { background: #45a049; }
        input[type="number"] { padding: 8px; font-size: 16px; width: 100px; }
        .feed-btn { background: #2196F3; }
        .calibrate-btn { background: #ff9800; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🐔 Henny Feeder</h1>
        <div class="status">
            <h3>Status</h3>
            <p>Time: <span id="time">--:--:--</span></p>
            <p>Adult Chickens: <span id="adults">-</span></p>
            <p>Calibration: <span id="calibration">-</span>g per 10s</p>
        </div>
        
        <div class="control">
            <h3>Manual Feed</h3>
            <button class="feed-btn" onclick="feed(25)">Feed 25g</button>
            <button class="feed-btn" onclick="feed(50)">Feed 50g</button>
            <br><br>
            <input type="number" id="customAmount" placeholder="Amount (g)" min="10" max="500">
            <button class="feed-btn" onclick="feedCustom()">Feed Custom</button>
        </div>
        
        <div class="control">
            <h3>Calibration</h3>
            <button class="calibrate-btn" onclick="calibrate()">Run 10s Calibration</button>
            <br><br>
            <input type="number" id="calValue" placeholder="Grams dispensed" step="0.1">
            <button onclick="setCalibration()">Set Calibration</button>
        </div>
        
        <div class="control">
            <h3>Configuration</h3>
            <label>Adult Chickens: </label>
            <input type="number" id="adultCount" min="0" max="100">
            <button onclick="updateConfig()">Update</button>
        </div>
    </div>
    
    <script>
        async function updateStatus() {
            try {
                const res = await fetch('/api/status');
                const data = await res.json();
                document.getElementById('time').textContent = data.time || '--:--:--';
                document.getElementById('adults').textContent = data.adults;
                document.getElementById('calibration').textContent = data.calibration.toFixed(1);
                document.getElementById('adultCount').value = data.adults;
            } catch (e) {
                console.error(e);
            }
        }
        
        async function feed(amount) {
            await fetch('/api/feed', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'amount=' + amount
            });
        }
        
        async function feedCustom() {
            const amount = document.getElementById('customAmount').value;
            if (amount) await feed(amount);
        }
        
        async function calibrate() {
            await fetch('/api/calibrate', {method: 'POST'});
            alert('Calibration started - measure amount after 10 seconds');
        }
        
        async function setCalibration() {
            const value = document.getElementById('calValue').value;
            if (value) {
                await fetch('/api/config', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: 'calibration=' + value
                });
                updateStatus();
            }
        }
        
        async function updateConfig() {
            const adults = document.getElementById('adultCount').value;
            await fetch('/api/config', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'adults=' + adults
            });
            updateStatus();
        }
        
        setInterval(updateStatus, 5000);
        updateStatus();
    </script>
</body>
</html>
)rawliteral";
    }
};