#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

const char* ssid = "Raffi";
const char* password = "123344556677";

// Konfigurasi Pin
const byte dhtPin = 2;        // D4 (GPIO 2)
const byte buttonPin = 4;     // D2 (GPIO 4)
const byte ledPin = 12;       // D6 (GPIO 12) - On/Off LED
const byte pwmLedPin = 5;     // D1 (GPIO 5)  - Dimmer PWM LED

DHT dht(dhtPin, DHT22);

// Variabel Status & Cache
bool ledState = false;
int pwmValue = 0;
String currentTemp = "--";
int buttonState;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long lastTime = 0;

// Inisialisasi Server & WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---------------- FRONT-END (HTML, CSS, JS) ----------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Lighting & Sensor Dashboard</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f9; margin: 0; padding: 20px; }
    h1 { color: #333; }
    .card { background: white; margin: 15px auto; padding: 20px; max-width: 350px; border-radius: 12px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
    button { padding: 12px 28px; font-size: 18px; border: none; border-radius: 6px; cursor: pointer; color: white; transition: 0.3s; }  
    .btn-on { background-color: #4CAF50; }  
    .btn-off { background-color: #f44336; }  
    .slider { width: 80%; margin: 15px 0; }
  </style>
</head>
<body>
  <h1>Smart Room Control</h1>
  
  <div class="card">
    <h2>Suhu: <span id="tempValue">--</span> &deg;C</h2>
  </div>

  <div class="card">
    <h2>LED Sakelar: <span id="ledStatus">OFF</span></h2>
    <button id="toggleBtn" class="btn-off" onclick="toggleLed()">Turn ON</button>
  </div>

  <div class="card">
    <h2>LED Dimmer (PWM)</h2>
    <input type="range" min="0" max="1023" value="0" class="slider" id="pwmSlider" oninput="sendPWM(this.value)">
    <p>Nilai PWM: <span id="pwmValueText">0</span> / 1023</p>
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    window.addEventListener('load', onLoad);

    function onLoad(event) {
      initWebSocket();
    }

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = function(e) { console.log('WebSocket Terkoneksi'); };
      websocket.onclose = function(e) { setTimeout(initWebSocket, 2000); };
      websocket.onmessage = onMessage;
    }

    function toggleLed() {  
      websocket.send('toggle');
    }

    // Mengirimkan nilai slider secara langsung saat digeser
    function sendPWM(val) {
      document.getElementById('pwmValueText').innerHTML = val;
      websocket.send('pwm,' + val);
    }

    function onMessage(event) {
      var dataObj = JSON.parse(event.data);  
        
      if(dataObj.suhu !== undefined) {  
         document.getElementById('tempValue').innerHTML = dataObj.suhu;  
      }  
        
      if(dataObj.led !== undefined) {  
         var btn = document.getElementById('toggleBtn');  
         var status = document.getElementById('ledStatus');  
         if(dataObj.led == "1"){  
           status.innerHTML = "ON";  
           btn.innerHTML = "Turn OFF";  
           btn.className = "btn-on";  
         } else {  
           status.innerHTML = "OFF";  
           btn.innerHTML = "Turn ON";  
           btn.className = "btn-off";  
         }  
      }

      if(dataObj.pwm !== undefined) {
         document.getElementById('pwmSlider').value = dataObj.pwm;
         document.getElementById('pwmValueText').innerHTML = dataObj.pwm;
      }
    }  
  </script>  
</body>  
</html>  
)rawliteral";

// ---------------- BACK-END (C++) ----------------

void notifyClients() {
  String jsonString = "{\"led\":\"" + String(ledState ? 1 : 0) + "\", ";
  jsonString += "\"pwm\":\"" + String(pwmValue) + "\", ";
  jsonString += "\"suhu\":\"" + currentTemp + "\"}";
  ws.textAll(jsonString);
}

// Handler Parsing Pesan Masuk dari Browser
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = String((char*)data);

    // Perintah Toggle LED Sakelar
    if (message == "toggle") {
      ledState = !ledState;
      notifyClients();
    } 
    // Parsing Perintah Slider PWM (Format: "pwm,NILAI")
    else if (message.startsWith("pwm,")) {
      int commaIndex = message.indexOf(',');
      if (commaIndex != -1) {
        String valStr = message.substring(commaIndex + 1);
        pwmValue = valStr.toInt();
        analogWrite(pwmLedPin, pwmValue); // Atur redup/terang LED
        notifyClients();                  // Sinkronisasi ke browser lain
      }
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      notifyClients();
      break;
    case WS_EVT_DISCONNECT:
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(pwmLedPin, OUTPUT);
  
  digitalWrite(ledPin, LOW);
  analogWrite(pwmLedPin, 0); // Kondisi awal LED PWM mati

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\nIP Address: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  ws.cleanupClients();

  // 1. Eksekusi LED Utama (Digital)
  digitalWrite(ledPin, ledState ? HIGH : LOW);

  // 2. Baca Tombol Fisik (Debounce)
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        ledState = !ledState;
        notifyClients();
      }
    }
  }
  lastButtonState = reading;

  // 3. Baca DHT11 Non-Blocking (Setiap 3 detik)
  if ((millis() - lastTime) > 3000) {
    float t = dht.readTemperature();
    if(!isnan(t)) {
      currentTemp = String(t);
      notifyClients();
    }
    lastTime = millis();
  }
}
