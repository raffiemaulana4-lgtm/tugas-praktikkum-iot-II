#include <painlessMesh.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define MESH_PREFIX   "LabIoTMesh"
#define MESH_PASSWORD "iotmeshpassword"
#define MESH_PORT     5555

#define DHTPIN 2       // Pin D4 (GPIO 2)
#define DHTTYPE DHT22  // 
DHT dht(DHTPIN, DHTTYPE);

const char* nodeName = "Node-1-DHT"; 

Scheduler userScheduler;
painlessMesh mesh;

void sendMessage();
Task taskSendMessage(TASK_SECOND * 3, TASK_FOREVER, &sendMessage);

void sendMessage() {
  float suhu = dht.readTemperature();
  float kelembapan = dht.readHumidity();

  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("[DHT Error] Gagal membaca data dari sensor DHT!");
    return;
  }

  // Membuat payload JSON
  StaticJsonDocument<200> doc;
  doc["tipe"] = "suhu_node";
  doc["suhu"] = suhu;
  doc["kelembapan"] = kelembapan;

  String msg;
  serializeJson(doc, msg);
  
  mesh.sendBroadcast(msg);
  Serial.print("[KIRIM MESH - NODE 1] ");
  Serial.println(msg);
}

void receivedCallback(uint32_t from, String &msg) {
  // Node 1 hanya bertindak sebagai pengirim sensor
}

void newConnectionCallback(uint32_t nodeId) {}
void changedConnectionCallback() {}

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();

  Serial.println("Node 1 (Sensor DHT) Siap!");
}

void loop() {
  mesh.update();
}