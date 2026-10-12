#include <painlessMesh.h>
#include <ArduinoJson.h>

#define MESH_PREFIX   "LabIoTMesh"
#define MESH_PASSWORD "iotmeshpassword"
#define MESH_PORT     5555

const char* nodeName = "Node-2-LDR";
const int ldrPin = A0; // Pin ADC NodeMCU

Scheduler userScheduler;
painlessMesh mesh;

void sendMessage();
Task taskSendMessage(TASK_SECOND * 3, TASK_FOREVER, &sendMessage);

void sendMessage() {
  int nilaiLDR = analogRead(ldrPin);

  // Membuat payload JSON
  StaticJsonDocument<200> doc;
  doc["tipe"] = "cahaya_node";
  doc["adc"] = nilaiLDR;

  String msg;
  serializeJson(doc, msg);
  
  mesh.sendBroadcast(msg);
  Serial.print("[KIRIM MESH - NODE 2] ");
  Serial.println(msg);
}

void receivedCallback(uint32_t from, String &msg) {
  // Node 2 hanya bertindak sebagai pengirim sensor
}

void newConnectionCallback(uint32_t nodeId) {}
void changedConnectionCallback() {}

void setup() {
  Serial.begin(115200);
  
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();

  Serial.println("Node 2 (Sensor LDR) Siap!");
}

void loop() {
  mesh.update();
}