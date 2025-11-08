#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP8266mDNS.h>

#define DHTPIN D2
#define DHTTYPE DHT22

// WiFi
const char* ssid = "BA Escuela";

/* MUST CONFIGURE WITH THE RIGHT SUBNET
IPAddress local_IP(192,168,0,50); // FREE IP ON THE NETWORK
IPAddress gateway(192,168,0,1);   // ROUTER IP
IPAddress subnet(255,255,255,0);
*/

// MQTT
const char* mqttServer = "not-known-yet";
const int mqttPort = 1883;
const char* topicData = "1/aula/1"; // Example
const char* topicSend = "send";

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// Portal detection
const char* probeUrl = "http://detectportal.firefox.com/canonical.html";

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

void reconnect() {
  String clientId = "ESP-" + String(ESP.getChipId());
  Serial.println("MQTT Client ID: " + clientId);
  const char* username = "raspberrypi";
  const char* password = "Hacemuchocalor"; // This must be changed.
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Intentar conectar
    if (client.connect(clientId.c_str(), username, password)) {
      Serial.println(" connected!");
      
      // Suscribirse al topic
      if (client.subscribe(topicSend)) {
        Serial.println("Subscribed to topic: " + String(topicSend));
      } else {
        Serial.println("Failed to subscribe to topic: " + String(topicSend));
      }
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


void sendSensorData() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("DHT read failed");
    return;
  }

  String payload = "{\"temperature\":" + String(t) + ",\"humidity\":" + String(h) + "}";
  
  // Publicar with QoS 1
  if (client.publish(topicData, payload.c_str())) {
    Serial.println("Data sent with QoS 1");
  } else {
    Serial.println("Publish failed");
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(1000);
}
