/*********
  ESP32 Professional Irrigation Dashboard
  - Real-time sensor graphs with Chart.js
  - Actuator status indicators (Pump, Fan, Lights)
  - 24-hour irrigation heatmap calendar
  - Irrigation history timeline
  - Responsive design for portfolio demonstrations
*********/

#include <WiFi.h>
#include <ArduinoJson.h>

const char* ssid = "ESP32-UART-AP";
const char* password = "12345678";

WiFiServer server(80);

// Enhanced node data with actuator states
struct NodeData {
  int humidity;
  int temp;
  int light;
  String profile;
  bool irrigation;
  bool fan_active;
  bool light1_active;
  bool humid_active;
  bool valid;
  unsigned long lastIrrigationStart;
  unsigned long irrigationDuration;
  int irrigationCount24h;
};

NodeData node1 = { 0, 0, 0, "None", false, false, false, false, false, 0, 0, 0 };
NodeData node2 = { 0, 0, 0, "None", false, false, false, false, false, 0, 0, 0 };
unsigned long lastUpdate = 0;

// Sensor history (last 60 readings ~2min at 2s intervals)
#define HISTORY_SIZE 60
struct SensorHistory {
  int humidity[HISTORY_SIZE];
  int temp[HISTORY_SIZE];
  int light[HISTORY_SIZE];
  int index;
  bool full;
} history1, history2;

// 24-hour irrigation heatmap (144 slots = 10min intervals)
#define HEATMAP_SLOTS 144
struct IrrigationHeatmap {
  uint8_t node1[HEATMAP_SLOTS];  // 0-255 intensity
  uint8_t node2[HEATMAP_SLOTS];
  unsigned long startTime;
} heatmap;

// Recent irrigation events log
#define EVENT_LOG_SIZE 20
struct IrrigationEvent {
  unsigned long timestamp;
  uint8_t node;
  uint16_t duration;
  String reason;
} eventLog[EVENT_LOG_SIZE];
int eventLogIndex = 0;
int eventLogCount = 0;

#define RXD2 16
#define TXD2 17
HardwareSerial stmSerial(2);

void addToHistory(SensorHistory* hist, int m, int t, int l) {
  hist->humidity[hist->index] = m;
  hist->temp[hist->index] = t;
  hist->light[hist->index] = l;
  hist->index = (hist->index + 1) % HISTORY_SIZE;
  if (hist->index == 0) hist->full = true;
}

void updateHeatmap(uint8_t node, bool active) {
  unsigned long now = millis();
  if (heatmap.startTime == 0) heatmap.startTime = now;

  // Calculate which 10-minute slot we're in
  unsigned long elapsed = now - heatmap.startTime;
  int slot = (elapsed / 600000) % HEATMAP_SLOTS;  // 600000ms = 10min

  if (active) {
    if (node == 1 && heatmap.node1[slot] < 255) heatmap.node1[slot] += 10;
    if (node == 2 && heatmap.node2[slot] < 255) heatmap.node2[slot] += 10;
  }
}

void logIrrigationEvent(uint8_t node, uint16_t duration) {
  eventLog[eventLogIndex].timestamp = millis();
  eventLog[eventLogIndex].node = node;
  eventLog[eventLogIndex].duration = duration;
  eventLog[eventLogIndex].reason = "Scheduled";
  eventLogIndex = (eventLogIndex + 1) % EVENT_LOG_SIZE;
  if (eventLogCount < EVENT_LOG_SIZE) eventLogCount++;
}

void setup() {
  Serial.begin(115200);
  stmSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);
  delay(100);

  Serial.println("\n=== ESP32 Professional Dashboard ===");

  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.begin();
  Serial.println("Dashboard: http://192.168.4.1\n");

  memset(&history1, 0, sizeof(SensorHistory));
  memset(&history2, 0, sizeof(SensorHistory));
  memset(&heatmap, 0, sizeof(IrrigationHeatmap));
  memset(eventLog, 0, sizeof(eventLog));
}

void loop() {
  // UART parsing
  if (stmSerial.available()) {
    String incoming = stmSerial.readStringUntil('\n');
    incoming.trim();

    if (incoming.length() > 10 && incoming.startsWith("{")) {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, incoming);

      if (!error) {
        bool prevIrrigation1 = node1.irrigation;
        bool prevIrrigation2 = node2.irrigation;

        // Node 1
        if (doc.containsKey("node1")) {
          node1.humidity = doc["node1"]["humidity"];
          node1.temp = doc["node1"]["temp"];
          node1.light = doc["node1"]["light"];
          node1.profile = doc["node1"]["profile"].as<String>();
          node1.irrigation = doc["node1"]["irrigation"];
          node1.valid = true;

          // Actuator states
          node1.humid_active = doc["node1"]["humid"];  
          node1.fan_active = doc["node1"]["fan"];      
          node1.light1_active = doc["node1"]["light1"]; 

          addToHistory(&history1, node1.humidity, node1.temp, node1.light);
          updateHeatmap(1, node1.irrigation);

          if (node1.irrigation && !prevIrrigation1) {
            node1.lastIrrigationStart = millis();
            node1.irrigationCount24h++;
          } else if (!node1.irrigation && prevIrrigation1) {
            uint16_t duration = (millis() - node1.lastIrrigationStart) / 1000;
            node1.irrigationDuration = duration;
            logIrrigationEvent(1, duration);
          }
        }

        // Node 2
        if (doc.containsKey("node2")) {
          node2.humidity = doc["node2"]["humidity"];
          node2.temp = doc["node2"]["temp"];
          node2.light = doc["node2"]["light"];
          node2.profile = doc["node2"]["profile"].as<String>();
          node2.irrigation = doc["node2"]["irrigation"];
          node2.valid = true;

          node2.fan_active = (node2.temp > 250);
          node2.light1_active = (node2.light < 400);
          node2.humid_active = false;

          addToHistory(&history2, node2.humidity, node2.temp, node2.light);
          updateHeatmap(2, node2.irrigation);

          if (node2.irrigation && !prevIrrigation2) {
            node2.lastIrrigationStart = millis();
            node2.irrigationCount24h++;
          } else if (!node2.irrigation && prevIrrigation2) {
            uint16_t duration = (millis() - node2.lastIrrigationStart) / 1000;
            node2.irrigationDuration = duration;
            logIrrigationEvent(2, duration);
          }
        }

        lastUpdate = millis();
      }
    }
  }

  // Web server
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    String request = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        if (c == '\n') {
          if (currentLine.length() == 0) {
            if (request.indexOf("GET /api/data") >= 0) {
              sendJsonData(client);
            } else {
              sendHtmlPage(client);
            }
            break;
          } else {
            if (currentLine.startsWith("GET ")) request = currentLine;
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
  }
}

void sendJsonData(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type: application/json");
  client.println("Connection: close");
  client.println();

  StaticJsonDocument<2048> doc;

  // Node 1
  doc["node1"]["humidity"] = node1.humidity;
  doc["node1"]["temp"] = node1.temp;
  doc["node1"]["light"] = node1.light;
  doc["node1"]["profile"] = node1.profile;
  doc["node1"]["irrigation"] = node1.irrigation;
  doc["node1"]["fan"] = node1.fan_active;
  doc["node1"]["light1"] = node1.light1_active;
  doc["node1"]["humid"] = node1.humid_active;
  doc["node1"]["count24h"] = node1.irrigationCount24h;
  if (node1.irrigation) {
    doc["node1"]["irrigating_for"] = (millis() - node1.lastIrrigationStart) / 1000;
  }

  // Node 2
  doc["node2"]["humidity"] = node2.humidity;
  doc["node2"]["temp"] = node2.temp;
  doc["node2"]["light"] = node2.light;
  doc["node2"]["profile"] = node2.profile;
  doc["node2"]["irrigation"] = node2.irrigation;
  doc["node2"]["fan"] = node2.fan_active;
  doc["node2"]["light1"] = node2.light1_active;
  doc["node2"]["humid"] = node2.humid_active;
  doc["node2"]["count24h"] = node2.irrigationCount24h;
  if (node2.irrigation) {
    doc["node2"]["irrigating_for"] = (millis() - node2.lastIrrigationStart) / 1000;
  }

  // History
  JsonArray hist1_m = doc["history1"]["humidity"].to<JsonArray>();
  JsonArray hist1_t = doc["history1"]["temp"].to<JsonArray>();
  JsonArray hist1_l = doc["history1"]["light"].to<JsonArray>();

  int start = history1.full ? history1.index : 0;
  int count = history1.full ? HISTORY_SIZE : history1.index;
  for (int i = 0; i < count; i++) {
    int idx = (start + i) % HISTORY_SIZE;
    hist1_m.add(history1.humidity[idx]);
    hist1_t.add(history1.temp[idx]);
    hist1_l.add(history1.light[idx]);
  }

  JsonArray hist2_m = doc["history2"]["humidity"].to<JsonArray>();
  JsonArray hist2_t = doc["history2"]["temp"].to<JsonArray>();
  JsonArray hist2_l = doc["history2"]["light"].to<JsonArray>();

  start = history2.full ? history2.index : 0;
  count = history2.full ? HISTORY_SIZE : history2.index;
  for (int i = 0; i < count; i++) {
    int idx = (start + i) % HISTORY_SIZE;
    hist2_m.add(history2.humidity[idx]);
    hist2_t.add(history2.temp[idx]);
    hist2_l.add(history2.light[idx]);
  }

  // Heatmap data
  JsonArray heat1 = doc["heatmap"]["node1"].to<JsonArray>();
  JsonArray heat2 = doc["heatmap"]["node2"].to<JsonArray>();
  for (int i = 0; i < HEATMAP_SLOTS; i++) {
    heat1.add(heatmap.node1[i]);
    heat2.add(heatmap.node2[i]);
  }

  // Event log
  JsonArray events = doc["events"].to<JsonArray>();
  unsigned long currentSeconds = millis() / 1000;
  for (int i = 0; i < eventLogCount; i++) {
    int idx = (eventLogIndex - eventLogCount + i + EVENT_LOG_SIZE) % EVENT_LOG_SIZE;
    JsonObject evt = events.createNestedObject();
    evt["time"] = currentSeconds - ((millis() - eventLog[idx].timestamp) / 1000);
    evt["node"] = eventLog[idx].node;
    evt["duration"] = eventLog[idx].duration;
  }

  doc["uptime"] = millis() / 1000;
  doc["lastUpdate"] = (millis() - lastUpdate) / 1000;

  serializeJson(doc, client);
}

void sendHtmlPage(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type: text/html");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html><head>");
  client.println("<meta charset=\"UTF-8\">");
  client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  client.println("<title>Smart Irrigation Dashboard</title>");
  client.println("<script src=\"https://cdn.jsdelivr.net/npm/chart.js@3.9.1/dist/chart.min.js\"></script>");
  client.println("<style>");
  client.println("* { margin: 0; padding: 0; box-sizing: border-box; }");
  client.println("body { font-family: 'Segoe UI', Tahoma, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; }");
  client.println(".container { max-width: 1600px; margin: 0 auto; }");
  client.println("h1 { color: white; text-align: center; margin-bottom: 30px; font-size: 2.5em; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }");
  client.println(".grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(350px, 1fr)); gap: 20px; margin-bottom: 20px; }");
  client.println(".card { background: white; border-radius: 16px; padding: 24px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }");
  client.println(".card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }");
  client.println(".card-title { font-size: 1.5em; color: #333; font-weight: 600; }");
  client.println(".profile-badge { background: #667eea; color: white; padding: 6px 12px; border-radius: 20px; font-size: 0.9em; }");
  client.println(".metrics { display: grid; grid-template-columns: repeat(3, 1fr); gap: 15px; margin: 20px 0; min-height: 80px; }");
  client.println("@media (max-width: 600px) { .actuators { margin-bottom: 20px; } }");
  client.println(".metric { text-align: center; padding: 15px; background: #f8f9fa; border-radius: 12px; }");
  client.println(".metric-value { font-size: 2em; font-weight: bold; color: #667eea; }");
  client.println(".metric-label { color: #666; font-size: 0.9em; margin-top: 5px; }");

  // Actuator indicators
  client.println(".actuators { display: flex; gap: 10px; margin: 15px 0; justify-content: center; flex-wrap: wrap; }");
  client.println(".actuator { padding: 8px 16px; border-radius: 20px; font-size: 0.85em; font-weight: 600; display: flex; align-items: center; gap: 6px; transition: all 0.3s; }");
  client.println(".actuator.on { background: #10b981; color: white; box-shadow: 0 0 20px rgba(16, 185, 129, 0.5); }");
  client.println(".actuator.off { background: #e5e7eb; color: #6b7280; }");
  client.println(".actuator-icon { width: 12px; height: 12px; border-radius: 50%; background: currentColor; animation: blink 1s infinite; }");
  client.println(".actuator.off .actuator-icon { animation: none; opacity: 0.3; }");
  client.println("@keyframes blink { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }");

  client.println(".status-bar { padding: 16px; border-radius: 12px; text-align: center; font-weight: bold; font-size: 1.1em; margin-top: 15px; }");
  client.println(".status-active { background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%); color: white; animation: pulse 2s infinite; }");
  client.println(".status-idle { background: #e9ecef; color: #6c757d; }");
  client.println("@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.7; } }");
  client.println(".stats { display: flex; justify-content: space-around; margin: 15px 0; padding: 15px; background: #f8f9fa; border-radius: 12px; }");
  client.println(".stat { text-align: center; }");
  client.println(".stat-value { font-size: 1.8em; font-weight: bold; color: #667eea; }");
  client.println(".stat-label { color: #666; font-size: 0.85em; }");
  client.println(".chart-container { position: relative; height: 300px; margin-top: 20px; }");
  client.println(".full-width { grid-column: 1 / -1; }");

  // Heatmap styles
  client.println(".heatmap { display: grid; grid-template-columns: repeat(24, 1fr); gap: 2px; margin: 20px 0; }");
  client.println(".heatmap-cell { aspect-ratio: 1; border-radius: 3px; transition: all 0.2s; cursor: pointer; }");
  client.println(".heatmap-cell:hover { transform: scale(1.2); z-index: 10; }");
  client.println(".heatmap-labels { display: flex; justify-content: space-between; font-size: 0.75em; color: #666; margin-top: 5px; }");
  client.println(".legend { display: flex; align-items: center; gap: 15px; margin: 15px 0; font-size: 0.85em; }");
  client.println(".legend-item { display: flex; align-items: center; gap: 5px; }");
  client.println(".legend-box { width: 20px; height: 20px; border-radius: 3px; }");

  // Timeline styles
  client.println(".timeline { max-height: 350px; overflow-y: auto; }");
  client.println(".timeline-item { padding: 12px; margin: 8px 0; background: #f8f9fa; border-left: 4px solid #667eea; border-radius: 8px; display: flex; justify-content: space-between; align-items: center; }");
  client.println(".timeline-node { font-weight: bold; color: #667eea; }");
  client.println(".timeline-time { font-size: 0.85em; color: #666; }");
  client.println(".timeline-duration { background: #667eea; color: white; padding: 4px 10px; border-radius: 12px; font-size: 0.85em; }");

  client.println(".offline { opacity: 0.5; filter: grayscale(100%); }");
  client.println(".debug { position: fixed; bottom: 0; left: 0; right: 0; background: rgba(0,0,0,0.9); color: #0f0; padding: 10px; font-family: monospace; font-size: 11px; max-height: 150px; overflow-y: auto; z-index: 1000; }");
  client.println("</style></head><body>");

  client.println("<div class=\"container\">");
  client.println("<h1>Smart Irrigation Dashboard</h1>");

  client.println("<div class=\"grid\">");

  // Node 1 Card
  client.println("<div class=\"card\" id=\"node1-card\">");
  client.println("<div class=\"card-header\">");
  client.println("<div class=\"card-title\">Node 1</div>");
  client.println("<div class=\"profile-badge\" id=\"n1-profile\">Loading...</div>");
  client.println("</div>");

  client.println("<div class=\"actuators\">");
  client.println("<div class=\"actuator off\" id=\"n1-pump\"><span class=\"actuator-icon\"></span>PUMP</div>");
  client.println("<div class=\"actuator off\" id=\"n1-humid\"><span class=\"actuator-icon\"></span>HUMID</div>");
  client.println("<div class=\"actuator off\" id=\"n1-fan\"><span class=\"actuator-icon\"></span>FAN</div>");
  client.println("<div class=\"actuator off\" id=\"n1-light\"><span class=\"actuator-icon\"></span>LIGHT</div>");
  client.println("</div>");

  client.println("<div class=\"metrics\">");
  client.println("<div class=\"metric\"><div class=\"metric-value\" id=\"n1-humidity\">--</div><div class=\"metric-label\">humidity</div></div>");
  client.println("<div class=\"metric\"><div class=\"metric-value\" id=\"n1-temp\">--</div><div class=\"metric-label\">Temp</div></div>");
  client.println("<div class=\"metric\"><div class=\"metric-value\" id=\"n1-light-sensor\">--</div><div class=\"metric-label\">Light</div></div>");
  client.println("</div>");

  client.println("<div class=\"status-bar\" id=\"n1-status\">Standby</div>");
  client.println("<div class=\"stats\">");
  client.println("<div class=\"stat\"><div class=\"stat-value\" id=\"n1-count\">0</div><div class=\"stat-label\">Today's Cycles</div></div>");
  client.println("<div class=\"stat\"><div class=\"stat-value\" id=\"n1-duration\">--</div><div class=\"stat-label\">Current Duration</div></div>");
  client.println("</div>");
  client.println("</div>");

  // Node 2 Card
  client.println("<div class=\"card\" id=\"node2-card\">");
  client.println("<div class=\"card-header\">");
  client.println("<div class=\"card-title\">Node 2</div>");
  client.println("<div class=\"profile-badge\" id=\"n2-profile\">Loading...</div>");
  client.println("</div>");

  client.println("<div class=\"actuators\">");
  client.println("<div class=\"actuator off\" id=\"n2-pump\"><span class=\"actuator-icon\"></span>PUMP</div>");
  client.println("<div class=\"actuator off\" id=\"n2-humid\"><span class=\"actuator-icon\"></span>HUMID</div>");
  client.println("<div class=\"actuator off\" id=\"n2-fan\"><span class=\"actuator-icon\"></span>FAN</div>");
  client.println("<div class=\"actuator off\" id=\"n2-light\"><span class=\"actuator-icon\"></span>LIGHT</div>");
  client.println("</div>");

  client.println("<div class=\"metrics\">");
  client.println("<div class=\"metric\"><div class=\"metric-value\" id=\"n2-humidity\">--</div><div class=\"metric-label\">humidity</div></div>");
  client.println("<div class=\"metric\"><div class=\"metric-value\" id=\"n2-temp\">--</div><div class=\"metric-label\">Temp</div></div>");
  client.println("<div class=\"metric\"><div class=\"metric-value\" id=\"n2-light-sensor\">--</div><div class=\"metric-label\">Light</div></div>");
  client.println("</div>");
  client.println("<div class=\"status-bar\" id=\"n2-status\">Standby</div>");
  client.println("<div class=\"stats\">");
  client.println("<div class=\"stat\"><div class=\"stat-value\" id=\"n2-count\">0</div><div class=\"stat-label\">Today's Cycles</div></div>");
  client.println("<div class=\"stat\"><div class=\"stat-value\" id=\"n2-duration\">--</div><div class=\"stat-label\">Current Duration</div></div>");
  client.println("</div>");
  client.println("</div>");

  // 24-Hour Heatmap
  client.println("<div class=\"card full-width\">");
  client.println("<div class=\"card-title\">24-Hour Irrigation Activity</div>");
  client.println("<div style=\"margin: 20px 0;\">");
  client.println("<div style=\"font-weight: 600; margin-bottom: 10px;\">Node 1</div>");
  client.println("<div class=\"heatmap\" id=\"heatmap1\"></div>");
  client.println("<div class=\"heatmap-labels\"><span>0:00</span><span>6:00</span><span>12:00</span><span>18:00</span><span>23:50</span></div>");
  client.println("</div>");
  client.println("<div style=\"margin: 20px 0;\">");
  client.println("<div style=\"font-weight: 600; margin-bottom: 10px;\">Node 2</div>");
  client.println("<div class=\"heatmap\" id=\"heatmap2\"></div>");
  client.println("<div class=\"heatmap-labels\"><span>0:00</span><span>6:00</span><span>12:00</span><span>18:00</span><span>23:50</span></div>");
  client.println("</div>");
  client.println("<div class=\"legend\">");
  client.println("<div class=\"legend-item\"><div class=\"legend-box\" style=\"background:#e5e7eb\"></div>No Activity</div>");
  client.println("<div class=\"legend-item\"><div class=\"legend-box\" style=\"background:#93c5fd\"></div>Low Activity</div>");
  client.println("<div class=\"legend-item\"><div class=\"legend-box\" style=\"background:#3b82f6\"></div>Medium Activity</div>");
  client.println("<div class=\"legend-item\"><div class=\"legend-box\" style=\"background:#1e40af\"></div>High Activity</div>");
  client.println("</div>");
  client.println("</div>");

  // Irrigation History Timeline
  client.println("<div class=\"card full-width\">");
  client.println("<div class=\"card-header\">");
  client.println("<div class=\"card-title\">Recent Irrigation Events</div>");
  client.println("<div style=\"font-size:0.8em;color:#666;font-style:italic;\">Duration shows actual irrigation time</div>");
  client.println("</div>");
  client.println("<div class=\"timeline\" id=\"timeline\"></div>");
  client.println("</div>");

  // Sensor Charts
  client.println("<div class=\"card full-width\">");
  client.println("<div class=\"card-title\">Node 1 - Sensor Trends</div>");
  client.println("<div class=\"chart-container\"><canvas id=\"chart1\"></canvas></div>");
  client.println("</div>");

  client.println("<div class=\"card full-width\">");
  client.println("<div class=\"card-title\">Node 2 - Sensor Trends</div>");
  client.println("<div class=\"chart-container\"><canvas id=\"chart2\"></canvas></div>");
  client.println("</div>");

  client.println("</div></div>");

  client.println("<div class=\"debug\" id=\"debug\">Debug console...</div>");

  // JavaScript
  client.println("<script>");
  client.println("let chart1, chart2;");
  client.println("const debug = document.getElementById('debug');");
  client.println("function log(msg) { debug.textContent = new Date().toLocaleTimeString() + ' - ' + msg; }");
  client.println("log('Dashboard loaded');");

  client.println("function initCharts() {");
  client.println("  const config = (id) => ({");
  client.println("    type: 'line',");
  client.println("    data: { labels: [], datasets: [");
  client.println("      { label: 'humidity', data: [], borderColor: '#3b82f6', tension: 0.4, fill: false },");
  client.println("      { label: 'Temperature', data: [], borderColor: '#ef4444', tension: 0.4, fill: false },");
  client.println("      { label: 'Light', data: [], borderColor: '#fbbf24', tension: 0.4, fill: false }");
  client.println("    ]},");
  client.println("    options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { position: 'bottom' } } }");
  client.println("  });");
  client.println("  chart1 = new Chart(document.getElementById('chart1'), config('chart1'));");
  client.println("  chart2 = new Chart(document.getElementById('chart2'), config('chart2'));");
  client.println("}");
  client.println("initCharts();");

  client.println("function updateActuator(id, state) {");
  client.println("  const el = document.getElementById(id);");
  client.println("  el.className = state ? 'actuator on' : 'actuator off';");
  client.println("}");

  client.println("function getHeatColor(intensity) {");
  client.println("  if (intensity === 0) return '#e5e7eb';");
  client.println("  if (intensity < 85) return '#93c5fd';");
  client.println("  if (intensity < 170) return '#3b82f6';");
  client.println("  return '#1e40af';");
  client.println("}");

  client.println("function renderHeatmap(containerId, data) {");
  client.println("  const container = document.getElementById(containerId);");
  client.println("  container.innerHTML = '';");
  client.println("  for (let i = 0; i < 144; i++) {");
  client.println("    const cell = document.createElement('div');");
  client.println("    cell.className = 'heatmap-cell';");
  client.println("    cell.style.background = getHeatColor(data[i] || 0);");
  client.println("    const hour = Math.floor(i / 6);");
  client.println("    const min = (i % 6) * 10;");
  client.println("    cell.title = hour + ':' + (min < 10 ? '0' : '') + min + ' - Activity: ' + (data[i] || 0);");
  client.println("    container.appendChild(cell);");
  client.println("  }");
  client.println("}");

  client.println("function renderTimeline(events) {");
  client.println("  const timeline = document.getElementById('timeline');");
  client.println("  if (!events || events.length === 0) {");
  client.println("    timeline.innerHTML = '<div style=\"text-align:center;color:#999;padding:20px;\">No irrigation events recorded yet</div>';");
  client.println("    return;");
  client.println("  }");
  client.println("  timeline.innerHTML = '';");
  client.println("  const now = Date.now() / 1000;");
  client.println("  for (let i = events.length - 1; i >= 0; i--) {");
  client.println("    const evt = events[i];");
  client.println("    const item = document.createElement('div');");
  client.println("    item.className = 'timeline-item';");
  client.println("    const elapsed = Math.floor(now - evt.time);");
  client.println("    let timeStr = '';");
  client.println("    if (elapsed < 60) timeStr = elapsed + 's ago';");
  client.println("    else if (elapsed < 3600) timeStr = Math.floor(elapsed/60) + 'm ago';");
  client.println("    else if (elapsed < 86400) timeStr = Math.floor(elapsed/3600) + 'h ago';");
  client.println("    else timeStr = Math.floor(elapsed/86400) + 'd ago';");
  client.println("    item.innerHTML = '<div><span class=\"timeline-node\">Node ' + evt.node + '</span> - <span class=\"timeline-time\">' + timeStr + '</span></div><div class=\"timeline-duration\">' + evt.duration + 's</div>';");
  client.println("    timeline.appendChild(item);");
  client.println("  }");
  client.println("}");

  client.println("async function updateDashboard() {");
  client.println("  try {");
  client.println("    log('Fetching /api/data...');");
  client.println("    const res = await fetch('/api/data');");
  client.println("    if (!res.ok) throw new Error('HTTP ' + res.status);");
  client.println("    const data = await res.json();");
  client.println("    log('Data received');");
  client.println("    ");
  client.println("    const stale = data.lastUpdate > 10;");
  client.println("    document.getElementById('node1-card').classList.toggle('offline', stale);");
  client.println("    document.getElementById('node2-card').classList.toggle('offline', stale);");
  client.println("    ");
  client.println("    if (!stale) {");
  client.println("      document.getElementById('n1-profile').textContent = data.node1.profile;");
  client.println("      document.getElementById('n1-humidity').textContent = data.node1.humidity;");
  client.println("      document.getElementById('n1-temp').textContent = data.node1.temp;");
  client.println("      document.getElementById('n1-light-sensor').textContent = data.node1.light;");
  client.println("      document.getElementById('n1-count').textContent = data.node1.count24h;");
  client.println("      ");
  client.println("      updateActuator('n1-pump', data.node1.irrigation);");
  client.println("      updateActuator('n1-humid', data.node1.humid);");
  client.println("      updateActuator('n1-fan', data.node1.fan);");
  client.println("      updateActuator('n1-light', data.node1.light1);");
  client.println("      ");
  client.println("      const n1Status = document.getElementById('n1-status');");
  client.println("      if (data.node1.irrigation) {");
  client.println("        n1Status.className = 'status-bar status-active';");
  client.println("        n1Status.textContent = 'IRRIGATING';");
  client.println("        document.getElementById('n1-duration').textContent = data.node1.irrigating_for + 's';");
  client.println("      } else {");
  client.println("        n1Status.className = 'status-bar status-idle';");
  client.println("        n1Status.textContent = 'Standby';");
  client.println("        document.getElementById('n1-duration').textContent = '--';");
  client.println("      }");
  client.println("      ");
  client.println("      document.getElementById('n2-profile').textContent = data.node2.profile;");
  client.println("      document.getElementById('n2-humidity').textContent = data.node2.humidity;");
  client.println("      document.getElementById('n2-temp').textContent = data.node2.temp;");
  client.println("      document.getElementById('n2-light-sensor').textContent = data.node2.light;");
  client.println("      document.getElementById('n2-count').textContent = data.node2.count24h;");
  client.println("      ");
  client.println("      updateActuator('n2-pump', data.node2.irrigation);");
  client.println("      updateActuator('n2-humid', data.node2.humid);");
  client.println("      updateActuator('n2-fan', data.node2.fan);");
  client.println("      updateActuator('n2-light', data.node2.light1);");
  client.println("      ");
  client.println("      const n2Status = document.getElementById('n2-status');");
  client.println("      if (data.node2.irrigation) {");
  client.println("        n2Status.className = 'status-bar status-active';");
  client.println("        n2Status.textContent = 'IRRIGATING';");
  client.println("        document.getElementById('n2-duration').textContent = data.node2.irrigating_for + 's';");
  client.println("      } else {");
  client.println("        n2Status.className = 'status-bar status-idle';");
  client.println("        n2Status.textContent = 'Standby';");
  client.println("        document.getElementById('n2-duration').textContent = '--';");
  client.println("      }");
  client.println("      ");
  client.println("      const labels = data.history1.humidity.map((_, i) => i);");
  client.println("      chart1.data.labels = labels;");
  client.println("      chart1.data.datasets[0].data = data.history1.humidity;");
  client.println("      chart1.data.datasets[1].data = data.history1.temp;");
  client.println("      chart1.data.datasets[2].data = data.history1.light;");
  client.println("      chart1.update('none');");
  client.println("      ");
  client.println("      chart2.data.labels = labels;");
  client.println("      chart2.data.datasets[0].data = data.history2.humidity;");
  client.println("      chart2.data.datasets[1].data = data.history2.temp;");
  client.println("      chart2.data.datasets[2].data = data.history2.light;");
  client.println("      chart2.update('none');");
  client.println("      ");
  client.println("      renderHeatmap('heatmap1', data.heatmap.node1);");
  client.println("      renderHeatmap('heatmap2', data.heatmap.node2);");
  client.println("      renderTimeline(data.events);");
  client.println("    }");
  client.println("  } catch(e) { log('ERROR: ' + e.message); console.error(e); }");
  client.println("}");
  client.println("setInterval(updateDashboard, 1000);");
  client.println("updateDashboard();");
  client.println("</script>");

  client.println("</body></html>");
}