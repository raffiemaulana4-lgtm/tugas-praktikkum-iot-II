#include <painlessMesh.h>
#include <ArduinoJson.h>

#define MESH_PREFIX   "LabIoTMesh"
#define MESH_PASSWORD "iotmeshpassword"
#define MESH_PORT     5555

const char* nodeName = "Node-3-Hub";
const int ledPin = 12; // Pin D6 (GPIO 12)

// Variabel status sensor terakhir untuk rule engine
float suhuTerakhir = 0.0;
int cahayaTerakhir = 1023; // Default terang

Scheduler userScheduler;
painlessMesh mesh;

void evaluasiRuleEngine() {
  // JIKA suhu di atas 31.0°C ATAU nilai ADC cahaya di bawah 300 (gelap)
  if (suhuTerakhir > 31.0 || cahayaTerakhir < 300) {
    digitalWrite(ledPin, HIGH);
    Serial.println("[RULE ENGINE] KONDISI KRITIS! LED MENYALA (HIGH)");
  } else {
    digitalWrite(ledPin, LOW);
    Serial.println("[RULE ENGINE] Kondisi Aman. LED MATI (LOW)");
  }
}

void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, msg);

  if (!error) {
    const char* tipe = doc["tipe"];
    
    if (strcmp(tipe, "suhu_node") == 0) {
      suhuTerakhir = doc["suhu"];
      float kelembapan = doc["kelembapan"];
      Serial.printf("[TERIMA DHT] Dari %u -> Suhu: %.2f °C, Kelembapan: %.2f %%\n", from, suhuTerakhir, kelembapan);
    } 
    else if (strcmp(tipe, "cahaya_node") == 0) {
      cahayaTerakhir = doc["adc"];
      Serial.printf("[TERIMA LDR] Dari %u -> Nilai ADC Cahaya: %d\n", from, cahayaTerakhir);
    }

    // Jalankan evaluasi setiap kali ada data baru masuk
    evaluasiRuleEngine();
    Serial.println("----------------------------------------");
  } else {
    Serial.printf("[DATA MENTAH DARI %u]: %s\n", from, msg.c_str());
  }
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("--> Koneksi Baru Terdeteksi! Node ID: %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.println("--> Topologi rantai mesh telah diperbarui (Self-Healing/Topology Change)");
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Pastikan LED mati saat awal

  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  // Perhatikan: taskSendMessage TIDAK diaktifkan di Node 3 (berperan pasif)

  Serial.println("Node 3 (Hub Aktuator Cerdas) Siap Menunggu Data...");
}

void loop() {
  mesh.update();
}