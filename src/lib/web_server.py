import socket
import json
import gc
import time
from time import localtime

HTML_TEMPLATE = """<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Henny - Smart Chicken Feeder</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: Arial, sans-serif; background: #f5f5f5; color: #333; }
        .container { max-width: 800px; margin: 0 auto; padding: 20px; }
        .header { background: #2c5c2d; color: white; padding: 20px; text-align: center; border-radius: 8px; margin-bottom: 20px; }
        .header h1 { font-size: 2.5em; margin-bottom: 10px; }
        .header p { opacity: 0.9; }
        .card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .card h2 { color: #2c5c2d; margin-bottom: 15px; border-bottom: 2px solid #2c5c2d; padding-bottom: 5px; }
        .status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
        .status-item { text-align: center; padding: 15px; background: #f8f9fa; border-radius: 5px; }
        .status-value { font-size: 1.8em; font-weight: bold; color: #2c5c2d; }
        .form-group { margin-bottom: 15px; }
        .form-group label { display: block; margin-bottom: 5px; font-weight: bold; }
        .form-group input, .form-group select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; }
        .btn { padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; margin: 5px; }
        .btn-primary { background: #2c5c2d; color: white; }
        .btn-secondary { background: #6c757d; color: white; }
        .btn-warning { background: #ffc107; color: black; }
        .btn-danger { background: #dc3545; color: white; }
        .btn:hover { opacity: 0.9; }
        .schedule-list { list-style: none; }
        .schedule-item { display: flex; justify-content: space-between; align-items: center; padding: 10px; margin: 5px 0; background: #f8f9fa; border-radius: 4px; }
        .schedule-completed { background: #d4edda; color: #155724; }
        .chick-group { display: flex; justify-content: space-between; align-items: center; padding: 10px; margin: 5px 0; background: #f8f9fa; border-radius: 4px; }
        .progress-bar { width: 100%; height: 20px; background: #e9ecef; border-radius: 10px; overflow: hidden; }
        .progress-fill { height: 100%; background: #2c5c2d; transition: width 0.3s ease; }
        .calibration-history { max-height: 150px; overflow-y: auto; }
        .error { color: #dc3545; font-weight: bold; }
        .success { color: #28a745; font-weight: bold; }
        .two-column { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        @media (max-width: 768px) { .two-column { grid-template-columns: 1fr; } .status-grid { grid-template-columns: 1fr; } }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🐔 Henny</h1>
            <p>Smart Chicken Feeder Control Panel</p>
        </div>
        
        <div class="card">
            <h2>📊 Current Status</h2>
            <div class="status-grid">
                <div class="status-item">
                    <div class="status-value" id="current-time">--:--</div>
                    <div>Current Time</div>
                </div>
                <div class="status-item">
                    <div class="status-value" id="next-feed">--:--</div>
                    <div>Next Feeding</div>
                </div>
                <div class="status-item">
                    <div class="status-value" id="daily-progress">0%</div>
                    <div>Today's Progress</div>
                </div>
                <div class="status-item">
                    <div class="status-value" id="total-birds">0</div>
                    <div>Total Birds</div>
                </div>
            </div>
        </div>

        <div class="two-column">
            <div class="card">
                <h2>⚙️ Configuration</h2>
                <form id="config-form">
                    <div class="form-group">
                        <label>Adult Chickens:</label>
                        <input type="number" id="adults" min="0" max="50" value="6">
                    </div>
                    <div class="form-group">
                        <label>Base Feed per Adult (g/day):</label>
                        <input type="range" id="base-feed" min="80" max="200" value="120" oninput="document.getElementById('base-feed-value').textContent=this.value">
                        <span id="base-feed-value">120</span>g
                    </div>
                    <div class="form-group">
                        <label>
                            <input type="checkbox" id="season-factor" checked> 
                            Enable Seasonal Adjustments
                        </label>
                    </div>
                    <button type="submit" class="btn btn-primary">Save Configuration</button>
                </form>
            </div>

            <div class="card">
                <h2>🐣 Chick Groups</h2>
                <div id="chick-groups">
                    <!-- Chick groups will be populated here -->
                </div>
                <form id="add-chicks-form">
                    <div class="form-group">
                        <label>Number of Chicks:</label>
                        <input type="number" id="chick-count" min="1" max="50" required>
                    </div>
                    <div class="form-group">
                        <label>Birth Date:</label>
                        <input type="date" id="birth-date" required>
                    </div>
                    <button type="submit" class="btn btn-secondary">Add Chick Group</button>
                </form>
            </div>
        </div>

        <div class="card">
            <h2>⏰ Today's Schedule</h2>
            <div class="status-item">
                <div>Season: <strong id="current-season">Spring</strong></div>
                <div>Daily Target: <strong id="daily-target">720g</strong></div>
            </div>
            <ul class="schedule-list" id="schedule-list">
                <!-- Schedule items will be populated here -->
            </ul>
            <div class="progress-bar">
                <div class="progress-fill" id="progress-fill" style="width: 0%"></div>
            </div>
        </div>

        <div class="two-column">
            <div class="card">
                <h2>🔧 Calibration</h2>
                <div class="status-item">
                    <div class="status-value" id="calibration-rate">50.0</div>
                    <div>Grams per 10 seconds</div>
                </div>
                <button onclick="runCalibration()" class="btn btn-warning">Run Calibration (10s)</button>
                <div class="form-group" style="margin-top: 15px;">
                    <label>Measured Amount (grams):</label>
                    <input type="number" id="measured-grams" step="0.1" min="0">
                    <button onclick="saveCalibration()" class="btn btn-secondary">Save Calibration</button>
                </div>
                <div class="calibration-history" id="calibration-history">
                    <!-- Calibration history will be populated here -->
                </div>
            </div>

            <div class="card">
                <h2>🎛️ Manual Controls</h2>
                <button onclick="feedNow(25)" class="btn btn-primary">Feed Now (25g)</button>
                <button onclick="feedNow(50)" class="btn btn-primary">Feed Now (50g)</button>
                <div class="form-group" style="margin-top: 15px;">
                    <label>Custom Amount (grams):</label>
                    <input type="number" id="custom-amount" min="1" max="500" value="25">
                    <button onclick="feedCustom()" class="btn btn-secondary">Feed Custom</button>
                </div>
                <button onclick="resetDaily()" class="btn btn-danger">Reset Daily Counter</button>
            </div>
        </div>
    </div>

    <script>
        function updateTime() {
            const now = new Date();
            document.getElementById('current-time').textContent = 
                now.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'});
        }
        
        function refreshStatus() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    // Update status display
                    document.getElementById('next-feed').textContent = data.next_feed || '--:--';
                    document.getElementById('daily-progress').textContent = data.progress + '%';
                    document.getElementById('total-birds').textContent = data.total_birds;
                    document.getElementById('current-season').textContent = data.season;
                    document.getElementById('daily-target').textContent = data.daily_target + 'g';
                    document.getElementById('calibration-rate').textContent = data.calibration_rate.toFixed(1);
                    
                    // Update progress bar
                    document.getElementById('progress-fill').style.width = data.progress + '%';
                    
                    // Update chick groups
                    updateChickGroups(data.chick_groups);
                    
                    // Update schedule
                    updateSchedule(data.schedule);
                })
                .catch(error => console.error('Error fetching status:', error));
        }
        
        function updateChickGroups(groups) {
            const container = document.getElementById('chick-groups');
            container.innerHTML = '';
            groups.forEach((group, index) => {
                const div = document.createElement('div');
                div.className = 'chick-group';
                div.innerHTML = `
                    <span>${group.count} chicks (${group.age_days} days old)</span>
                    <button onclick="removeChickGroup(${index})" class="btn btn-danger">Remove</button>
                `;
                container.appendChild(div);
            });
        }
        
        function updateSchedule(schedule) {
            const list = document.getElementById('schedule-list');
            list.innerHTML = '';
            schedule.sessions.forEach(session => {
                const li = document.createElement('li');
                li.className = 'schedule-item' + (session.completed ? ' schedule-completed' : '');
                li.innerHTML = `
                    <span>${session.time}</span>
                    <span>${session.amount}g ${session.completed ? '✓' : ''}</span>
                `;
                list.appendChild(li);
            });
        }
        
        function feedNow(amount) {
            fetch('/feed_now', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({amount: amount})
            })
            .then(response => response.json())
            .then(data => {
                alert(data.message);
                refreshStatus();
            });
        }
        
        function feedCustom() {
            const amount = parseInt(document.getElementById('custom-amount').value);
            if (amount > 0) {
                feedNow(amount);
            }
        }
        
        function runCalibration() {
            fetch('/calibrate', {method: 'POST'})
            .then(response => response.json())
            .then(data => {
                alert(data.message);
            });
        }
        
        function saveCalibration() {
            const grams = parseFloat(document.getElementById('measured-grams').value);
            if (grams > 0) {
                fetch('/save_calibration', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({grams: grams})
                })
                .then(response => response.json())
                .then(data => {
                    alert(data.message);
                    refreshStatus();
                });
            }
        }
        
        function removeChickGroup(index) {
            fetch('/remove_chicks', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({index: index})
            })
            .then(response => response.json())
            .then(data => {
                alert(data.message);
                refreshStatus();
            });
        }
        
        function resetDaily() {
            if (confirm('Reset daily feeding counter?')) {
                fetch('/reset_daily', {method: 'POST'})
                .then(response => response.json())
                .then(data => {
                    alert(data.message);
                    refreshStatus();
                });
            }
        }
        
        document.getElementById('config-form').addEventListener('submit', function(e) {
            e.preventDefault();
            const config = {
                adults: parseInt(document.getElementById('adults').value),
                base_feed: parseInt(document.getElementById('base-feed').value),
                season_factor: document.getElementById('season-factor').checked
            };
            
            fetch('/config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(config)
            })
            .then(response => response.json())
            .then(data => {
                alert(data.message);
                refreshStatus();
            });
        });
        
        document.getElementById('add-chicks-form').addEventListener('submit', function(e) {
            e.preventDefault();
            const data = {
                count: parseInt(document.getElementById('chick-count').value),
                birth_date: document.getElementById('birth-date').value
            };
            
            fetch('/add_chicks', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(data)
            })
            .then(response => response.json())
            .then(data => {
                alert(data.message);
                refreshStatus();
                document.getElementById('add-chicks-form').reset();
            });
        });
        
        // Initialize
        setInterval(updateTime, 1000);
        setInterval(refreshStatus, 10000);  // Refresh every 10 seconds
        updateTime();
        refreshStatus();
    </script>
</body>
</html>"""

class WebServer:
    def __init__(self, config, spreader, scheduler):
        self.config = config
        self.spreader = spreader
        self.scheduler = scheduler
        self.socket = None
        self.running = False
        print("Web server initialized")
    
    def start(self, host='0.0.0.0', port=80):
        """Start the web server"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.socket.bind((host, port))
            self.socket.listen(5)
            self.socket.settimeout(1.0)  # Non-blocking with timeout
            self.running = True
            
            print(f"Web server started on http://{host}:{port}")
            return True
        except Exception as e:
            print(f"Failed to start web server: {e}")
            return False
    
    def handle_requests(self):
        """Handle incoming HTTP requests (non-blocking)"""
        if not self.running or not self.socket:
            return
        
        try:
            conn, addr = self.socket.accept()
            conn.settimeout(5.0)
            
            request = conn.recv(1024).decode('utf-8')
            
            # Parse request
            if request:
                response = self._process_request(request)
                conn.send(response.encode('utf-8'))
            
            conn.close()
            gc.collect()
            
        except OSError:
            # Timeout or no connection - normal for non-blocking
            pass
        except Exception as e:
            print(f"Request handling error: {e}")
    
    def _process_request(self, request):
        """Process HTTP request and return response"""
        lines = request.split('\n')
        if not lines:
            return self._http_response(400, 'Bad Request')
        
        method_line = lines[0].strip()
        if not method_line:
            return self._http_response(400, 'Bad Request')
        
        parts = method_line.split(' ')
        if len(parts) < 2:
            return self._http_response(400, 'Bad Request')
        
        method = parts[0]
        path = parts[1]
        
        # Extract body for POST requests
        body = ''
        if method == 'POST':
            body_start = request.find('\r\n\r\n')
            if body_start != -1:
                body = request[body_start + 4:]
        
        # Route handling
        if method == 'GET' and path == '/':
            return self._serve_main_page()
        elif method == 'GET' and path == '/status':
            return self._serve_status()
        elif method == 'POST' and path == '/config':
            return self._handle_config(body)
        elif method == 'POST' and path == '/add_chicks':
            return self._handle_add_chicks(body)
        elif method == 'POST' and path == '/remove_chicks':
            return self._handle_remove_chicks(body)
        elif method == 'POST' and path == '/feed_now':
            return self._handle_feed_now(body)
        elif method == 'POST' and path == '/calibrate':
            return self._handle_calibrate()
        elif method == 'POST' and path == '/save_calibration':
            return self._handle_save_calibration(body)
        elif method == 'POST' and path == '/reset_daily':
            return self._handle_reset_daily()
        else:
            return self._http_response(404, 'Not Found')
    
    def _serve_main_page(self):
        """Serve the main HTML page"""
        return self._http_response(200, HTML_TEMPLATE, 'text/html')
    
    def _serve_status(self):
        """Serve current status as JSON"""
        progress = self.scheduler.get_daily_progress()
        schedule = self.scheduler.get_feeding_schedule_display()
        chicken_summary = self.scheduler.get_chicken_summary()
        next_feed = self.scheduler.get_next_feeding_time()
        
        status = {
            'current_time': self._format_time(localtime()),
            'next_feed': f"{next_feed[0]:02d}:{next_feed[1]:02d}",
            'progress': progress['progress_percent'],
            'total_birds': chicken_summary['total_birds'],
            'season': schedule['season'],
            'daily_target': schedule['daily_total'],
            'calibration_rate': self.config.get('spreader.grams_per_10s', 0),
            'chick_groups': self._format_chick_groups(),
            'schedule': schedule
        }
        
        return self._http_response(200, json.dumps(status), 'application/json')
    
    def _handle_config(self, body):
        """Handle configuration update"""
        try:
            data = json.loads(body)
            
            self.config.set('chickens.adults', data.get('adults', 6))
            self.config.set('chickens.base_feed_per_adult', data.get('base_feed', 120))
            self.config.set('feeding.season_factor', data.get('season_factor', True))
            
            return self._json_response({'success': True, 'message': 'Configuration updated'})
        except Exception as e:
            print(f"Config update error: {e}")
            return self._json_response({'success': False, 'message': 'Configuration update failed'})
    
    def _handle_add_chicks(self, body):
        """Handle adding chick group"""
        try:
            data = json.loads(body)
            count = data.get('count', 0)
            birth_date = data.get('birth_date', '')
            
            if count > 0 and birth_date:
                self.config.add_chick_group(count, birth_date)
                return self._json_response({'success': True, 'message': f'Added {count} chicks'})
            else:
                return self._json_response({'success': False, 'message': 'Invalid chick data'})
        except Exception as e:
            print(f"Add chicks error: {e}")
            return self._json_response({'success': False, 'message': 'Failed to add chicks'})
    
    def _handle_remove_chicks(self, body):
        """Handle removing chick group"""
        try:
            data = json.loads(body)
            index = data.get('index', -1)
            
            if self.config.remove_chick_group(index):
                return self._json_response({'success': True, 'message': 'Chick group removed'})
            else:
                return self._json_response({'success': False, 'message': 'Invalid group index'})
        except Exception as e:
            print(f"Remove chicks error: {e}")
            return self._json_response({'success': False, 'message': 'Failed to remove chicks'})
    
    def _handle_feed_now(self, body):
        """Handle manual feeding"""
        try:
            data = json.loads(body)
            amount = data.get('amount', 25)
            
            if self.spreader.spread_feed(amount):
                self.scheduler.record_feeding(amount)
                return self._json_response({'success': True, 'message': f'Feeding {amount}g started'})
            else:
                return self._json_response({'success': False, 'message': 'Feeding failed - spreader busy or not calibrated'})
        except Exception as e:
            print(f"Feed now error: {e}")
            return self._json_response({'success': False, 'message': 'Feeding failed'})
    
    def _handle_calibrate(self):
        """Handle calibration run"""
        try:
            if self.spreader.calibrate_run():
                return self._json_response({'success': True, 'message': 'Calibration run started (10 seconds). Measure the spread amount and save.'})
            else:
                return self._json_response({'success': False, 'message': 'Calibration failed - spreader busy'})
        except Exception as e:
            print(f"Calibration error: {e}")
            return self._json_response({'success': False, 'message': 'Calibration failed'})
    
    def _handle_save_calibration(self, body):
        """Handle saving calibration measurement"""
        try:
            data = json.loads(body)
            grams = data.get('grams', 0)
            
            if grams > 0:
                # Calculate rate: grams per 10 seconds
                rate = float(grams)  # Since calibration run is 10 seconds
                self.config.update_calibration(rate)
                return self._json_response({'success': True, 'message': f'Calibration saved: {rate}g per 10 seconds'})
            else:
                return self._json_response({'success': False, 'message': 'Invalid measurement'})
        except Exception as e:
            print(f"Save calibration error: {e}")
            return self._json_response({'success': False, 'message': 'Failed to save calibration'})
    
    def _handle_reset_daily(self):
        """Handle daily counter reset"""
        try:
            self.config.set('schedule.daily_counter', 0)
            return self._json_response({'success': True, 'message': 'Daily counter reset'})
        except Exception as e:
            print(f"Reset daily error: {e}")
            return self._json_response({'success': False, 'message': 'Reset failed'})
    
    def _format_chick_groups(self):
        """Format chick groups for display"""
        ages = self.config.get_chick_ages()
        formatted = []
        for group in ages:
            formatted.append({
                'count': group['count'],
                'age_days': group['age_days']
            })
        return formatted
    
    def _format_time(self, time_tuple):
        """Format time tuple to HH:MM string"""
        return f"{time_tuple[3]:02d}:{time_tuple[4]:02d}"
    
    def _http_response(self, status_code, body, content_type='text/plain'):
        """Generate HTTP response"""
        status_text = {200: 'OK', 400: 'Bad Request', 404: 'Not Found', 500: 'Internal Server Error'}
        status = status_text.get(status_code, 'Unknown')
        
        response = f"HTTP/1.1 {status_code} {status}\r\n"
        response += f"Content-Type: {content_type}\r\n"
        response += f"Content-Length: {len(body)}\r\n"
        response += "Connection: close\r\n"
        response += "\r\n"
        response += body
        
        return response
    
    def _json_response(self, data):
        """Generate JSON HTTP response"""
        return self._http_response(200, json.dumps(data), 'application/json')
    
    def stop(self):
        """Stop the web server"""
        self.running = False
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
        print("Web server stopped")
        gc.collect()