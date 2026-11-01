#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Kredensial Jaringan Wi-Fi
const char* ssid = "iQOO Z10R 5G";
const char* password = "12345678";

// Konfigurasi Broker MQTT Publik
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* clientId = "ESP8266_LabIoT_25782021_Full";

// Pembagian Topik Mandiri
const char* topicPublish   = "kampus/labiot/25782021/telemetri";
const char* topicSubscribe = "kampus/labiot/25782021/kendali";
const char* topicAlert = "kampus/labiot/25782021/alert";
const char* topicStatus = "kampus/labiot/25782021/status";

bool isOverheatAlertSent = false;

// Alokasi Pin Perangkat Keras
const byte dhtPin = 2; // Pin D4 (GPIO 2)
const byte ledPin = 5; // Pin D1 (GPIO 5)
#define DHTTYPE DHT22

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTelemetryTime = 0;
const unsigned long telemetryInterval = 4000; // Siklus kirim 4 detik

void callback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("----------------------------------------");
  Serial.printf("[KONTROL DITERIMA] Topik: %s | Payload: %s\n", topic, incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);
  if (err) {
    Serial.println("Gagal membedah payload JSON!");
    return;
  }

  if (doc.containsKey("led")) {
    int val = doc["led"];
    digitalWrite(ledPin, val == 1 ? HIGH : LOW);
    Serial.printf("--> [INDIKATOR] Status LED diubah: %s\n", val == 1 ? "MENYALA" : "PADAM");
  }
  Serial.println("----------------------------------------");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan kembali ke Broker...");

    // 1. Siapkan isi Pesan Wasiat (LWT) jika perangkat terputus mendadak
    const char* lwtPayload = "{\"status\":\"OFFLINE\",\"keterangan\":\"Perangkat Terputus Tak Terduga\"}";
    int lwtQoS = 1;
    bool lwtRetain = true;

    // 2. Hubungkan ke Broker MQTT dengan parameter LWT:
    // client.connect(id, willTopic, willQoS, willRetain, willMessage)
    if (client.connect(clientId, topicStatus, lwtQoS, lwtRetain, lwtPayload)) {
      Serial.println(" Sukses Terhubung!");

      // 3. Kirim status ONLINE sesaat setelah berhasil terhubung
      const char* onlinePayload = "{\"status\":\"ONLINE\",\"keterangan\":\"Perangkat Beroperasi Normal\"}";
      client.publish(topicStatus, onlinePayload, true); // true = retain flag

      // 4. Subskripsi ke topik kendali
      client.subscribe(topicSubscribe);
      Serial.printf("Subskripsi Topik: %s\n", topicSubscribe);
    } else {
      Serial.printf("Gagal (rc=%d). Coba lagi dalam 4 detik...\n", client.state());
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Terkoneksi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Memelihara sesi TCP dan memproses callback masuk

  unsigned long now = millis();
  if (now - lastTelemetryTime >= telemetryInterval) {
    lastTelemetryTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {

      if (t > 31.0) {
        // Nyalakan LED indikator bahaya secara otomatis
        digitalWrite(ledPin, HIGH);

        // Hanya kirim alert ke MQTT jika alert belum pernah terkirim sebelumnya
        if (!isOverheatAlertSent) {
          StaticJsonDocument<128> docAlert;
          docAlert["status"] = "BAHAYA";
          docAlert["parameter"] = "OVERHEAT";
          docAlert["nilai_suhu"] = serialized(String(t, 1)); // Mengirim nilai suhu misal 31.8

          char alertBuffer[128];
          serializeJson(docAlert, alertBuffer);

          client.publish(topicAlert, alertBuffer);
          Serial.printf("[ALERT TERKIRIM] %s\n", alertBuffer);

          // Pasang Latch agar tidak terkirim berulang kali
          isOverheatAlertSent = true; 
        }
      } else {
        // Jika suhu kembali aman (<= 31.0°C), reset latch agar siap untuk peringatan berikutnya
        isOverheatAlertSent = false;
      }

      StaticJsonDocument<256> docOut;
      docOut["node_id"] = clientId;
      docOut["suhu"] = serialized(String(t, 2));
      docOut["kelembapan"] = serialized(String(h, 2));
      docOut["status_led"] = digitalRead(ledPin);

      if (t <= 31.0) {
        docOut["kondisi_suhu"] = "NORMAL";
      } else {
        docOut["kondisi_suhu"] = "PANAS";
      }

      char buffer[256];
      serializeJson(docOut, buffer);
      
      bool success = client.publish(topicPublish, buffer);
      if (success) {
        Serial.printf("[TELEMETRI TERKIRIM] %s\n", buffer);
      } else {
        Serial.println("[ERROR] Gagal memublikasikan telemetri!");
      }
    }
  }
}