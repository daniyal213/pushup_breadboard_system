#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>

// --- HARDWARE PINS ---
#define rx 22
#define tx 13  
#define BATTERY_PIN 35

const int Pins[] = {27, 26, 25, 33, 32, 4, 16, 17, 18, 19, 21, 14, 23};
const int pinCount = 13;

// --- SENSOR VARIABLES ---
int activeQ = -1;      
int activeW = -1;      
int lastActiveQ = -1;  
int lastActiveW = -1;  

bool state = false;
bool insideRep = false;
int interval[] = {15, 35, 70};
int count = 0;          

String currentPosition = "NONE";
String lastPosition = "NONE";

// --- GAME LOGIC & PREFERENCES ---
Preferences preferences;
long totalPoints = 0;
long totalReps = 0;        
int sessionReps = 0; // Tracks reps for the current calendar day in Kazakhstan       

// Personal Records
int maxChestReps = 0;
int maxBackReps = 0;
int maxShoulderReps = 0;
int maxTricepsReps = 0;

// Daily Quests Progress
int currentDayCycle = 0;
int quest1Progress = 0;
int quest2Progress = 0;
int quest1Reps = 0;   // Tracks push-ups specifically done for Quest 1 today
int quest2Reps = 0;   // Tracks push-ups specifically done for Quest 2 today
int lastSavedDay = -1;

// Dynamic Quest Targets
int targetChest = 700;
int targetShoulder = 700;
int targetBack = 700;
int targetTriceps = 700;

// --- WIFI, TIME, & SERVER ---
const char* ssid = "Daniyal";
const char* password = "qwertyuio";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 18000; // UTC+5 (Kazakhstan Time)
const int   daylightOffset_sec = 0;

WebServer server(80);
unsigned long lastTimeCheck = 0;

// --- HTML/CSS/JS (CYBERPUNK UI) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>PUSHUP SYSTEM INTERFACE</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Orbitron:wght=400;700;900&display=swap');
    
    * { box-sizing: border-box; }

    body {
      background-color: #050914;
      background-image: 
        linear-gradient(rgba(0, 255, 255, 0.05) 1px, transparent 1px),
        linear-gradient(90deg, rgba(0, 255, 255, 0.05) 1px, transparent 1px);
      background-size: 20px 20px;
      color: #a0c0d0;
      font-family: 'Orbitron', monospace;
      margin: 0; padding: 20px;
      display: flex; flex-direction: column; align-items: center;
    }
    .header { text-align: center; margin-bottom: 30px; }
    .header h4 { color: #00ffff; font-weight: 400; letter-spacing: 5px; margin: 5px 0; font-size: 10px;}
    .header h1 { color: #ffffff; font-size: 40px; margin: 0; text-shadow: 0 0 15px #00ffff; letter-spacing: 8px;}
    .header h1 span { color: #8a2be2; text-shadow: 0 0 15px #8a2be2; }
    
    .panel {
      background: rgba(10, 15, 24, 0.8);
      border: 1px solid #1a3a4a;
      width: 100%; max-width: 600px;
      margin-bottom: 20px; padding: 20px;
      position: relative;
      clip-path: polygon(0 0, 95% 0, 100% 15px, 100% 100%, 5% 100%, 0 calc(100% - 15px));
    }
    .panel::before {
      content: ''; position: absolute; top: 0; left: 0; width: 30px; height: 2px; background: #00ffff;
    }
    .panel-title { font-size: 10px; color: #00ffff; letter-spacing: 3px; margin-bottom: 15px; display: flex; align-items: center;}
    .panel-title span { display: inline-block; width: 6px; height: 6px; background: #00ffff; border-radius: 50%; margin-right: 10px; box-shadow: 0 0 5px #00ffff;}
    
    .grid-3 { 
      display: grid; 
      grid-template-columns: 1fr 1fr 1fr; 
      gap: 12px; 
      width: 100%; 
      max-width: 600px; 
      margin-bottom: 20px;
    }
    .grid-3 .panel { width: 100%; max-width: none; margin-bottom: 0; }
    .grid-3 .value-large { font-size: 34px; }

    .grid-4 {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 12px;
      margin-top: 15px;
    }
    @media (min-width: 450px) {
      .grid-4 { grid-template-columns: repeat(4, 1fr); }
    }
    .record-box {
      background: rgba(5, 10, 20, 0.6);
      border: 1px solid #142834;
      padding: 10px;
      text-align: center;
    }
    
    .value-large { font-size: 48px; font-weight: 700; margin: 0; line-height: 1; }
    .value-cyan { color: #00ffff; text-shadow: 0 0 10px rgba(0,255,255,0.5); }
    .value-purple { color: #d080ff; text-shadow: 0 0 10px rgba(138,43,226,0.8); }
    .label-small { font-size: 10px; color: #507080; letter-spacing: 2px; margin-top: 5px; }

    .rank-text { font-size: 26px; font-weight: 900; color: #ff0055; text-shadow: 0 0 15px #ff0055; text-transform: uppercase; }
    
    .progress-bar-bg { background: #0a1520; height: 10px; width: 100%; margin-top: 10px; border: 1px solid #1a3a4a; position: relative;}
    .progress-bar-fill { background: #00ffff; height: 100%; width: 0%; box-shadow: 0 0 10px #00ffff; transition: width 0.5s ease;}
    
    .quest-item { display: flex; justify-content: space-between; margin-bottom: 5px; font-size: 14px;}
    .quest-reps { font-size: 10px; color: #507080; margin-bottom: 12px; letter-spacing: 1px;}
</style>
</head>
<body>

  <div class="header">
    <h4>[ SYSTEM INTERFACE ]</h4>
    <h1>PUSH<span>UP</span></h1>
    <h4>ESP32 TRACKER</h4>
  </div>

  <div class="panel">
    <div class="panel-title"><span></span>RANK STATUS</div>
    <div style="display: flex; justify-content: space-between; align-items: flex-end;">
      <div>
        <div class="label-small">CURRENT TIER</div>
        <div class="rank-text" id="rankText">BEGINNER</div>
      </div>
      <div style="text-align: right;">
        <div class="value-purple" style="font-size: 24px;" id="totalPoints">0</div>
        <div class="label-small">/ <span id="rankMax">1,000</span> PTS</div>
      </div>
    </div>
    <div class="progress-bar-bg"><div class="progress-bar-fill" id="rankProgress"></div></div>
  </div>

  <div class="grid-3">
    <div class="panel">
      <div class="panel-title"><span></span>REPS</div>
      <div class="value-large value-cyan" id="currentReps">0</div>
      <div class="label-small">CURRENT SET</div>
    </div>
    <div class="panel">
      <div class="panel-title"><span></span>POINTS</div>
      <div class="value-large value-purple" id="sessionPoints">0</div>
      <div class="label-small">CURRENT SET</div>
    </div>
    <div class="panel">
      <div class="panel-title"><span></span>REPS</div>
      <div class="value-large value-cyan" id="sessionReps">0</div>
      <div class="label-small">TODAY'S TOTAL</div>
    </div>
  </div>

  <div class="panel">
    <div class="panel-title"><span></span>GRIP POSITION</div>
    <div class="value-large value-cyan" id="gripPosition" style="font-size: 32px;">NONE</div>
  </div>

  <div class="panel">
    <div class="panel-title"><span></span>DAILY QUESTS</div>
    <div id="questsContainer"></div>
  </div>

  <div class="panel">
    <div class="panel-title"><span></span>ARCHIVE // LIFETIME METRICS</div>
    <div style="display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #1a3a4a; padding-bottom: 15px;">
      <div>
        <div class="label-small">GRAND TOTAL REPS</div>
        <div class="value-large value-cyan" id="totalReps" style="font-size: 38px;">0</div>
      </div>
      <div style="text-align: right;">
        <div class="label-small" style="color: #ff0055;">ALL TIME RECORD DATA</div>
      </div>
    </div>
    
    <div class="label-small" style="margin-top: 15px;">PERSONAL BEST SINGLE-SET PEAKS</div>
    <div class="grid-4">
      <div class="record-box">
        <div class="label-small">CHEST</div>
        <div class="value-large value-cyan" id="maxChest" style="font-size: 24px;">0</div>
      </div>
      <div class="record-box">
        <div class="label-small">BACK</div>
        <div class="value-large value-cyan" id="maxBack" style="font-size: 24px;">0</div>
      </div>
      <div class="record-box">
        <div class="label-small">SHOULDER</div>
        <div class="value-large value-cyan" id="maxShoulder" style="font-size: 24px;">0</div>
      </div>
      <div class="record-box">
        <div class="label-small">TRICEPS</div>
        <div class="value-large value-cyan" id="maxTriceps" style="font-size: 24px;">0</div>
      </div>
    </div>
  </div>

  <script>
    function updateUI() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('currentReps').innerText = data.reps;
          document.getElementById('sessionPoints').innerText = data.sessionPoints;
          document.getElementById('sessionReps').innerText = data.sessionReps.toLocaleString();
          document.getElementById('totalReps').innerText = data.totalReps.toLocaleString();

          document.getElementById('maxChest').innerText = data.maxChest;
          document.getElementById('maxBack').innerText = data.maxBack;
          document.getElementById('maxShoulder').innerText = data.maxShoulder;
          document.getElementById('maxTriceps').innerText = data.maxTriceps;

          document.getElementById('gripPosition').innerText = data.position;
          document.getElementById('totalPoints').innerText = data.totalPoints.toLocaleString();
          document.getElementById('rankText').innerText = data.rank;

          // Rank Progression Math
          let progressPercent;
          if (data.rank === 'Immortal') {
            document.getElementById('rankMax').innerText = '\u221e';
            progressPercent = 100;
          } else {
            document.getElementById('rankMax').innerText = data.rankMax.toLocaleString();
            let progressRange = data.rankMax - data.rankMin;
            let progressInRank = data.totalPoints - data.rankMin;
            progressPercent = (progressInRank / progressRange) * 100;
            if (progressPercent > 100) progressPercent = 100;
            if (progressPercent < 0)   progressPercent = 0;
          }
          document.getElementById('rankProgress').style.width = progressPercent + '%';

          // Dynamic rendering with efficiency limits displayed
          let questHTML = '';
          let limit1 = Math.floor(data.t1 / 15.4);
          let limit2 = Math.floor(data.t2 / 15.4);

          if (data.dayCycle === 0) {
            questHTML += `<div class="quest-item"><span>CHEST</span> <span><span style="color:#00ffff">${data.q1}</span> / ${data.t1} PTS</span></div>`;
            questHTML += `<div class="quest-reps">Quest Pushups: ${data.qr1} (Keep below ${limit1} for Level Up)</div>`;
            questHTML += `<div class="quest-item"><span>SHOULDERS</span> <span><span style="color:#00ffff">${data.q2}</span> / ${data.t2} PTS</span></div>`;
            questHTML += `<div class="quest-reps">Quest Pushups: ${data.qr2} (Keep below ${limit2} for Level Up)</div>`;
          } else {
            questHTML += `<div class="quest-item"><span>BACK</span> <span><span style="color:#00ffff">${data.q1}</span> / ${data.t1} PTS</span></div>`;
            questHTML += `<div class="quest-reps">Quest Pushups: ${data.qr1} (Keep below ${limit1} for Level Up)</div>`;
            questHTML += `<div class="quest-item"><span>TRICEPS</span> <span><span style="color:#00ffff">${data.q2}</span> / ${data.t2} PTS</span></div>`;
            questHTML += `<div class="quest-reps">Quest Pushups: ${data.qr2} (Keep below ${limit2} for Level Up)</div>`;
          }
          document.getElementById('questsContainer').innerHTML = questHTML;
        });
    }
    setInterval(updateUI, 700);
  </script>
</body>
</html>
)rawliteral";

// --- LOGIC FUNCTIONS ---

String getPositionName(int pin1, int pin2) {
  int pMin = min(pin1, pin2);
  int pMax = max(pin1, pin2);

  if ((pMin == 4  && pMax == 16) || (pMin == 25 && pMax == 33) ||
      (pMin == 25 && pMax == 32) || (pMin == 16 && pMax == 27) ||
      (pMin == 27 && pMax == 33)) return "Chest";
      
  if ((pMin == 4  && pMax == 26) || (pMin == 26 && pMax == 32)) return "Back";
  
  if ((pMin == 17 && pMax == 23) || (pMin == 17 && pMax == 32) ||
      (pMin == 4  && pMax == 21) || (pMin == 19 && pMax == 21)) return "Triceps";
      
  if ((pMin == 19 && pMax == 23) || (pMin == 14 && pMax == 18)) return "Shoulder";
  
  return "Unknown";
}

String getRank(long pts) {
  if (pts >= 1000000) return "Immortal";
  if (pts >= 500000)  return "Legend";
  if (pts >= 250000)  return "Master";
  if (pts >= 100000)  return "Champion";
  if (pts >= 50000)   return "Elite";
  if (pts >= 25000)   return "Advanced";
  if (pts >= 10000)   return "Athlete";
  if (pts >= 5000)    return "Strong";
  if (pts >= 1000)    return "Active";
  return "Beginner";
}

void getRankRange(long pts, long &rankMin, long &rankMax) {
  if      (pts >= 1000000) { rankMin = 1000000; rankMax = 1000000; }
  else if (pts >= 500000)  { rankMin = 500000;  rankMax = 1000000; }
  else if (pts >= 250000)  { rankMin = 250000;  rankMax = 500000;  }
  else if (pts >= 100000)  { rankMin = 100000;  rankMax = 250000;  }
  else if (pts >= 50000)   { rankMin = 50000;   rankMax = 100000;  }
  else if (pts >= 25000)   { rankMin = 25000;   rankMax = 50000;   }
  else if (pts >= 10000)   { rankMin = 10000;   rankMax = 25000;   }
  else if (pts >= 5000)    { rankMin = 5000;    rankMax = 10000;   }
  else if (pts >= 1000)    { rankMin = 1000;    rankMax = 5000;    }
  else                     { rankMin = 0;       rankMax = 1000;    }
}

int calculatePoints(int reps) {
  int total = 0;
  int currentRepPoints = 10;
  
  for (int i = 1; i <= reps; i++) {
    total += currentRepPoints;
    if (i < 50) {
      currentRepPoints += 1;
    } else {
      currentRepPoints += 5; 
    }
  }
  return total;
}

void savePreferences() {
  preferences.begin("pushupApp", false);
  preferences.putLong("totalPoints", totalPoints);
  preferences.putLong("totalReps",   totalReps);
  preferences.putInt("sessReps",    sessionReps);
  
  preferences.putInt("maxChest",    maxChestReps);
  preferences.putInt("maxBack",     maxBackReps);
  preferences.putInt("maxShoulder", maxShoulderReps);
  preferences.putInt("maxTriceps",  maxTricepsReps);

  preferences.putInt("tChest",      targetChest);
  preferences.putInt("tShoulder",   targetShoulder);
  preferences.putInt("tBack",       targetBack);
  preferences.putInt("tTriceps",    targetTriceps);

  preferences.putInt("dayCycle",    currentDayCycle);
  preferences.putInt("q1",          quest1Progress);
  preferences.putInt("q2",          quest2Progress);
  preferences.putInt("qr1",         quest1Reps);
  preferences.putInt("qr2",         quest2Reps);
  preferences.putInt("lastDay",     lastSavedDay);
  preferences.end();
}

void checkDailyReset() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  
  int currentDay = timeinfo.tm_yday;
  
  if (lastSavedDay != -1 && currentDay != lastSavedDay) {
    int t1 = (currentDayCycle == 0) ? targetChest : targetBack;
    int t2 = (currentDayCycle == 0) ? targetShoulder : targetTriceps;

    float comp1 = (float)quest1Progress / t1;
    float comp2 = (float)quest2Progress / t2;

    // --- DIFFICULTY UPGRADE LOGIC ---
    // Target is scaled UP by 100 points ONLY if completed 100% within (Target / 15.4) pushups
    if (quest1Progress >= t1 && quest1Reps > 0 && quest1Reps <= (t1 / 15.4)) {
      if (currentDayCycle == 0) targetChest += 100;
      else targetBack += 100;
    }
    if (quest2Progress >= t2 && quest2Reps > 0 && quest2Reps <= (t2 / 15.4)) {
      if (currentDayCycle == 0) targetShoulder += 100;
      else targetTriceps += 100;
    }

    // --- ROTATION / RESET GATE ---
    if (comp1 >= 0.8 && comp2 >= 0.8) {
      // Both hit >= 80%: Switch daily quest to next day's muscle group
      currentDayCycle = (currentDayCycle == 0) ? 1 : 0;
      Serial.println("\n--- DAILY QUEST CLEAR (>=80%): CHANGING MUSCLE GROUPS ---");
    } else {
      // Did not hit 80%: Stay on same day cycle, all quest progress resets to 0
      Serial.println("\n--- QUEST FAILED (<80%): RETAINING MUSCLES, PROGRESS WIPED TO 0 ---");
    }
    
    // Always clear daily trackers at midnight
    quest1Progress = 0;
    quest2Progress = 0;
    quest1Reps = 0;
    quest2Reps = 0;
    sessionReps = 0; // Daily session volume reset
    lastSavedDay = currentDay;
    
    savePreferences();
  } else if (lastSavedDay == -1) {
    lastSavedDay = currentDay;
    savePreferences();
  }
}

void processFinishedSet() {
  if (count <= 0) return;

  int pointsEarned = calculatePoints(count);
  totalPoints += pointsEarned;
  totalReps   += count;
  sessionReps += count; 

  if (lastPosition == "Chest" && count > maxChestReps) maxChestReps = count;
  else if (lastPosition == "Back" && count > maxBackReps) maxBackReps = count;
  else if (lastPosition == "Shoulder" && count > maxShoulderReps) maxShoulderReps = count;
  else if (lastPosition == "Triceps" && count > maxTricepsReps) maxTricepsReps = count;

  int t1 = (currentDayCycle == 0) ? targetChest : targetBack;
  int t2 = (currentDayCycle == 0) ? targetShoulder : targetTriceps;

  if (currentDayCycle == 0) { 
    if (lastPosition == "Chest") {
      quest1Progress = min(t1, quest1Progress + pointsEarned);
      quest1Reps += count;
    }
    if (lastPosition == "Shoulder") {
      quest2Progress = min(t2, quest2Progress + pointsEarned);
      quest2Reps += count;
    }
  } else { 
    if (lastPosition == "Back") {
      quest1Progress = min(t1, quest1Progress + pointsEarned);
      quest1Reps += count;
    }
    if (lastPosition == "Triceps") {
      quest2Progress = min(t2, quest2Progress + pointsEarned);
      quest2Reps += count;
    }
  }

  savePreferences();
}

// --- WEBSERVER ROUTINES ---

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleData() {
  long rankMin, rankMax;
  getRankRange(totalPoints, rankMin, rankMax);

  int t1 = (currentDayCycle == 0) ? targetChest : targetBack;
  int t2 = (currentDayCycle == 0) ? targetShoulder : targetTriceps;

  String json = "{";
  json += "\"reps\":"          + String(count) + ",";
  json += "\"sessionReps\":"   + String(sessionReps) + ",";
  json += "\"totalReps\":"     + String(totalReps) + ",";
  json += "\"sessionPoints\":" + String(calculatePoints(count)) + ",";
  json += "\"position\":\""    + currentPosition + "\",";
  json += "\"totalPoints\":"   + String(totalPoints) + ",";
  json += "\"rank\":\""        + getRank(totalPoints) + "\",";
  json += "\"rankMin\":"       + String(rankMin) + ",";
  json += "\"rankMax\":"       + String(rankMax) + ",";
  json += "\"dayCycle\":"      + String(currentDayCycle) + ",";
  json += "\"q1\":"            + String(quest1Progress) + ",";
  json += "\"q2\":"            + String(quest2Progress) + ",";
  json += "\"qr1\":"           + String(quest1Reps) + ",";
  json += "\"qr2\":"           + String(quest2Reps) + ",";
  json += "\"t1\":"            + String(t1) + ",";
  json += "\"t2\":"            + String(t2) + ",";
  json += "\"maxChest\":"      + String(maxChestReps) + ",";
  json += "\"maxBack\":"       + String(maxBackReps) + ",";
  json += "\"maxShoulder\":"   + String(maxShoulderReps) + ",";
  json += "\"maxTriceps\":"    + String(maxTricepsReps);
  json += "}";
  
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, rx, tx);

  for (int i = 0; i < pinCount; i++) {
    pinMode(Pins[i], INPUT_PULLUP);
  }

  // Load Preferences
  preferences.begin("pushupApp", false);
  totalPoints     = preferences.getLong("totalPoints", 0);
  totalReps       = preferences.getLong("totalReps",   0);
  sessionReps     = preferences.getInt("sessReps",    0);
  
  maxChestReps    = preferences.getInt("maxChest", 0);
  maxBackReps     = preferences.getInt("maxBack", 0);
  maxShoulderReps = preferences.getInt("maxShoulder", 0);
  maxTricepsReps  = preferences.getInt("maxTriceps", 0);

  targetChest     = preferences.getInt("tChest", 700);
  targetShoulder  = preferences.getInt("tShoulder", 700);
  targetBack      = preferences.getInt("tBack", 700);
  targetTriceps   = preferences.getInt("tTriceps", 700);

  currentDayCycle = preferences.getInt("dayCycle",    0);
  quest1Progress  = preferences.getInt("q1",           0);
  quest2Progress  = preferences.getInt("q2",           0);
  quest1Reps      = preferences.getInt("qr1",          0);
  quest2Reps      = preferences.getInt("qr2",          0);
  lastSavedDay    = preferences.getInt("lastDay",     -1);
  preferences.end();

  // Setup WiFi
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP Address: ");
  Serial.println(WiFi.localIP());

  // Configure Time for Kazakhstan Zone (UTC+5)
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Setup Server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();

  if (millis() - lastTimeCheck > 10000) {
    checkDailyReset();
    lastTimeCheck = millis();
  }

  state   = false;
  activeQ = -1;
  activeW = -1;

  for (int q = 0; q < pinCount; q++) {
    pinMode(Pins[q], OUTPUT);
    digitalWrite(Pins[q], LOW);
    delayMicroseconds(10);

    for (int w = q + 1; w < pinCount; w++) {
      if (digitalRead(Pins[w]) == LOW) {
        activeQ = Pins[q];
        activeW = Pins[w];
        state   = true;
        break;
      }
    }
    pinMode(Pins[q], INPUT_PULLUP);
    if (state) break;
  }

  if (state) {
    currentPosition = getPositionName(activeQ, activeW);

    if (activeQ != lastActiveQ || activeW != lastActiveW) {
      count       = 0;
      insideRep   = false;
      lastActiveQ = activeQ;
      lastActiveW = activeW;
      lastPosition = currentPosition; 
      Serial.println("\n--- New Position Detected ---");
    }
  } else {
    if (lastActiveQ != -1) {
      if (count > 0) {
        Serial.println("\n=======================");
        Serial.println("Rep set finished");
        processFinishedSet(); 
      }
      count           = 0;
      insideRep       = false;
      lastActiveQ     = -1;
      lastActiveW     = -1;
      currentPosition = "NONE";
    }
  }

  if (state) {
    while(Serial2.available() > 0) { Serial2.read(); }
    Serial2.write(0x55);
    delay(40);

    if (Serial2.available() >= 2) {
      byte highByte = Serial2.read();
      byte lowByte = Serial2.read();
      float distance = ((highByte << 8) + lowByte) / 10.0;

      if (distance <= interval[0] && !insideRep) {
        insideRep = true;
      }

      if (distance >= interval[1] && distance <= interval[2] && insideRep) {
        count++;
        insideRep = false;
      } 
      else if (distance > interval[2] && insideRep) {
        insideRep = false; 
      }
    }
  }

  delay(20); 
}