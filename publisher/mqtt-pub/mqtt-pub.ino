#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP8266mDNS.h>

#define DHTPIN D2
#define DHTTYPE DHT11
#define probeUrl "http://detectportal.firefox.com/canonical.html"
#define topic "aula/1"
// WiFi
const char* ssid = "BA Escuela";

// MQTT
const char* mqttServer = "not-known-yet";
const int mqttPort = 1883;

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// Portal detection
String detectPortal() {
  HTTPClient http;
  WiFiClient client;
  
  if (!http.begin(client, probeUrl)) {
    return String();
  }
  
  http.addHeader("User-Agent", "Mozilla/5.0");
  int code = http.GET();

  if (code >= 300 && code < 400) {
    String location = http.header("Location");
    http.end();
    return location;
  }

  http.end();
  return String();
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Error al configurar IP estática");
  }
  delay(1000);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid);  


  Portal detection
  String portalUrl = detectPortal();
  if (portalUrl.length() > 0) {
    Serial.println("Portal detected");
  }

    while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("Connected! IP: " + WiFi.localIP().toString());
    if (MDNS.begin("esp-client")) {
    Serial.println("mDNS responder started");
  } else {
    Serial.println("mDNS failed");
  }
  dht.begin();

  
  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttCallback);
  
  randomSeed(ESP.getChipId()); // Para delays únicos por dispositivo
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  if (message == "read") {
    delay(random(500, 3000)); // Random delay 0.5-3 segundos
    sendSensorData();
  }
}

void sendMQTT(float temp, float hum) { // No estoy seguro si este código sirve para mandar la temperatura
  String clientId = "ESP-" + String(ESP.getChipId());
  Serial.println("MQTT Client ID: " + clientId);
  const char* username = "raspberrypi";
  const char* password = "Hacemuchocalor"; // This must be changed.
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Intentar conectar
    if (client.connect(clientId.c_str(), username, password)) {
      Serial.println(" connected!");
    } else {
      // Mostrar el error de conexión (devuelve código de PubSubClient)
      int error = client.state();
      Serial.print(" failed, state=");
      Serial.println(error);

      // Esperar antes de reintentar
      Serial.println("Retrying in 5 seconds...");
      delay(5000);
    }
  }
}


void pubMQTT(float temp, float hum) { // Fijate si esto te sirve
  if (isnan(h) || isnan(t)) {
    Serial.println("DHT read failed");
    return;
  }

  String payload = "{\"temperature\":" + String(t) + ",\"humidity\":" + String(h) + "}";
  
  // Publicar with QoS 1
  if (client.publish(topic, payload.c_str())) {
    Serial.println("Data sent with QoS 1");
  } else {
    Serial.println("Publish failed");
  }
}
