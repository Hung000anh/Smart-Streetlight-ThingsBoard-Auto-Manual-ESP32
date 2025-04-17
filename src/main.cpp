#include <WiFi.h>
#include <PubSubClient.h>

#define LDR_PIN 34
#define RELAY_PIN 23
#define LED_PIN 23

const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ThingsBoard
const char* mqttServer = "thingsboard.cloud";
const int mqttPort = 1883;
const char* token = "kYUfEw6B3rNghi92RkJa";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastSend = 0;
const int interval = 5000; // gửi mỗi 5 giây

String currentMode = "auto"; // "auto" hoặc "manual"
String lightStatus = "OFF";  // trạng thái đèn hiện tại

void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");
}

void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard...");
    if (client.connect("ESP32", token, NULL)) {
      Serial.println("Connected!");
      client.subscribe("v1/devices/me/rpc/request/+");
    } else {
      Serial.print("Failed. Error: ");
      Serial.print(client.state());
      delay(1000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String request = "";
  for (int i = 0; i < length; i++) {
    request += (char)payload[i];
  }
  Serial.print("RPC Received: ");
  Serial.println(request);

  if (request.indexOf("setStatus") != -1) {
    if (request.indexOf("ON") != -1) {
      lightStatus = "ON";
    } else if (request.indexOf("OFF") != -1) {
      lightStatus = "OFF";
    }
  }

  if (request.indexOf("setMode") != -1) {
    if (request.indexOf("auto") != -1) {
      currentMode = "auto";
    } else if (request.indexOf("manual") != -1) {
      currentMode = "manual";
    }
  }

  // Gửi phản hồi trạng thái hiện tại
  String response = "{\"mode\":\"" + currentMode + "\",\"status\":\"" + lightStatus + "\"}";
  client.publish("v1/devices/me/attributes", response.c_str());
}

void sendTelemetry(int ldr, String status) {
  String payload = "{\"ambient_light\":" + String(ldr) + ",\"status\":\"" + status + "\",\"mode\":\"" + currentMode + "\"}";
  client.publish("v1/devices/me/telemetry", payload.c_str());
  Serial.println("Telemetry Sent: " + payload);
}

void setup() {
  Serial.begin(115200);
  pinMode(LDR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  connectToWiFi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  connectToMQTT();
  Serial.println("System Ready");
}

void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  if (millis() - lastSend > interval) {
    lastSend = millis();

    int ldrValue = analogRead(LDR_PIN);
    String status;

    if (currentMode == "auto") {
      if (ldrValue < 2000) {
        digitalWrite(RELAY_PIN, HIGH);
        status = "ON";
      } else {
        digitalWrite(RELAY_PIN, LOW);
        status = "OFF";
      }
    } else {
      status = lightStatus;
      digitalWrite(RELAY_PIN, (status == "ON") ? HIGH : LOW);
    }

    sendTelemetry(ldrValue, status);
  }
}

