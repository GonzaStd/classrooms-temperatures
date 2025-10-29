#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// I/O
const int pinBtn = D1;
const int pinGreenLed = D2;

// WiFi
const char *ssid = "ESP8266-weather";
const char *password = "PK7q4l567vG2";

// GPS
static const int RXPin = D7, TXPin = D8;
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

// Server-Send Event
AsyncWebServer server(8080);
AsyncEventSource events("/events");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<body>
<h2>Sensor Data</h2>
<div id="data">Esperando datos...</div>
<script>
const src = new EventSource("/events");
src.onmessage = (e) => {
  const data = JSON.parse(e.data);
  document.getElementById("data").innerHTML =
    "Temp: " + data.temp + " Hum: " + data.hum;
};
</script>
</body>
</html>
)rawliteral";

void setup()
{
  // Serial
  Serial.begin(9600);
  
  // I/O
  pinMode(pinBtn, INPUT_PULLUP);
  pinMode(pinGreenLed, OUTPUT);
  
  // WiFi
  WiFi.mode(WIFI_AP);
  while(!WiFi.softAP(ssid, password))
  {
  Serial.println(".");
    delay(100);
  }
  Serial.print("IP address:\t");
  Serial.println(WiFi.softAPIP());

  // Listen
   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);});
  
  // Server-Send Event
  server.addHandler(&events);
  server.begin();

  //GPS
  ss.begin(GPSBaud);
  Serial.print(F("TinyGPSPlus library version. ")); Serial.println(TinyGPSPlus::libraryVersion());  Serial.println();
}

void loop()
{
  // This sketch displays information every time a new sentence is correctly encoded.
  while (ss.available() > 0)
    if (gps.encode(ss.read()))
      if (digitalRead(pinBtn) == LOW)
        displayInfo();

  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while(true);
  }
}

void displayInfo(){
  
  Serial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else{
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Timestamp: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else{
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  int hour = gps.time.hour();
  int minute = gps.time.minute();
  int second = gps.time.second();

  // Ajuste horario para Argentina (UTC-3)
  hour -= 3;
  if (hour < 0) hour += 24;
  
  if (gps.time.isValid())
  {
    if (hour < 10) Serial.print(F("0"));
    Serial.print(hour);
    Serial.print(F(":"));
    if (minute < 10) Serial.print(F("0"));
    Serial.print(minute);
    Serial.print(F(":"));
    if (second < 10) Serial.print(F("0"));
    Serial.print(second);
  }
  else{
    Serial.print(F("INVALID"));
  }
  float t = 25.4;
  float h = 48.9;
  char payload[50];
  sprintf(payload, "{\"temp\":%.1f,\"hum\":%.1f}", t, h);
  events.send(payload, "update", millis());

  
  digitalWrite(pinGreenLed, HIGH);
  delay(2000);
  digitalWrite(pinGreenLed, LOW);
  Serial.println();
}
