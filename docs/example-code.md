# Generic and example code
This code is an example of what you could use to flash the ESP-01 firmware and use it with the circuit shown below. This is NOT the code upload on this repository, this is for general use.

```cpp
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#define DHTPIN D2          // Pin where the DHT22 is connected
#define DHTTYPE DHT22     // DHT 22 (AM2302)

// Wi-Fi credentials
const char* ssid = "yourSSID";
const char* password = "yourPASSWORD";

// MQTT server details
const char* mqttServer = "mqttServer";
const int mqttPort = 1883; // Default MQTT port
const char* topic = "sensor/data";

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(9600);
  dht.begin();
  setupWiFi();
  client.setServer(mqttServer, mqttPort);
}

void setupWiFi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  String payload = String("Temperature: ") + t + " °C, Humidity: " + h + " %";
  client.publish(topic, payload.c_str());
  
  delay(2000); // Publish every 2 seconds
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
```