/* ESP-01 (ESP8266EX) - NTP schedule + DHT11 + MQTT (PubSubClient)
   - Arduino Core (ESP8266)
   - Serial 9600 (según tu ESP-01)
   - DHT11 en GPIO2 (define DHTPIN abajo como 2)
   - DHCP (WiFi.begin(ssid, pass))
   - Reintentos NTP: 10 intentos, timeout 5s por intento
   - Si NTP falla -> deepSleep 3 horas
   - Schedule: {8,11,14,17,20,23} (horas locales UTC-3)
   - Uso: conectar GPIO16 <-> RST mediante SOLDER JUMPER para poder wakeup; abrir jumper al flashear (ver explicación abajo).
*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <DHT.h>

#include <stdint.h>

#define SERIAL_BAUD 9600
#define DHTPIN 2            // GPIO2 (ajustalo si preferís otro pin; en ESP-01 son limitados)
#define DHTTYPE DHT11

// ----- CONFIGURAR -----
const char* ssid = "BA Escuela";       // tu SSID (red con portal)
const char* wifi_user = "";            // si tu auth lo requiere en código (si no, dejá vacío)
const char* wifi_pass = "";            // lo manejás en el segundo código: deja aquí si querés
const char* mqttServer = "not-known-yet"; // <- COMPLETAR
const uint16_t mqttPort = 1883;       // sin TLS
const char* mqttUser = "raspberrypi"; // <- COMPLETAR si aplica
const char* mqttPass = "Hacemuchocalor"; // <- COMPLETAR si aplica

const char* probeUrl = "http://detectportal.firefox.com/canonical.html"; // portal detection
const char* topic = "aula/1"; // minimal payload; cambia por sensor por dispositivo

// NTP servers
const char* NTP_SERVERS[] = {
  "0.south-america.pool.ntp.org",
  "1.south-america.pool.ntp.org",
  "2.south-america.pool.ntp.org",
  "3.south-america.pool.ntp.org",
  "pool.ntp.org",
  "0.pool.ntp.org",
  "1.pool.ntp.org"
};
const int NTP_SERVERS_COUNT = sizeof(NTP_SERVERS)/sizeof(NTP_SERVERS[0]);

const int MAX_NTP_ATTEMPTS = 10;
const unsigned long NTP_ATTEMPT_TIMEOUT_MS = 5000UL; // 5s
const unsigned long NTP_RETRY_DELAY_MS = 200UL;      // pequeño gap entre solicitudes
const uint64_t FAIL_SLEEP_MICROS = (uint64_t)3ULL * 3600ULL * 1000000ULL; // 3h en micros

// schedule horas 24h
const uint8_t scheduleHours[] = {8, 11, 14, 17, 20, 23};
const int SCHEDULE_LEN = sizeof(scheduleHours)/sizeof(scheduleHours[0]);

// timezone Argentina UTC-3 fijo
const int TZ_OFFSET_SECONDS = -3 * 3600;

// objetos globales
WiFiUDP udp;
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// helpers NTP
const int NTP_PACKET_SIZE = 48;
byte ntpPacketBuffer[NTP_PACKET_SIZE];

// Construye paquete NTP
void sendNTPpacket(const char* address) {
  memset(ntpPacketBuffer, 0, NTP_PACKET_SIZE);
  ntpPacketBuffer[0] = 0b11100011; // LI, VN, Mode (v4, client)
  udp.beginPacket(address, 123);
  udp.write(ntpPacketBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

// Pide NTP y devuelve true con epochUnix (segundos unix UTC) si OK
bool requestNTP(const char* server, uint32_t &epochUnix, unsigned long timeoutMs) {
  udp.beginPacket(server, 123);
  memset(ntpPacketBuffer, 0, NTP_PACKET_SIZE);
  ntpPacketBuffer[0] = 0b11100011;
  udp.write(ntpPacketBuffer, NTP_PACKET_SIZE);
  udp.endPacket();

  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    int len = udp.parsePacket();
    if (len >= NTP_PACKET_SIZE) {
      udp.read(ntpPacketBuffer, NTP_PACKET_SIZE);
      uint32_t seconds1900 = (uint32_t)ntpPacketBuffer[40] << 24 |
                             (uint32_t)ntpPacketBuffer[41] << 16 |
                             (uint32_t)ntpPacketBuffer[42] << 8  |
                             (uint32_t)ntpPacketBuffer[43];
      const uint32_t NTP_TO_UNIX = 2208988800UL;
      if (seconds1900 == 0 || seconds1900 <= NTP_TO_UNIX) return false;
      epochUnix = seconds1900 - NTP_TO_UNIX;
      return true;
    }
    delay(10);
  }
  return false;
}

// Sincroniza con varios intentos (round-robin servers). Devuelve true y setea epochUnixUTC.
bool syncNTPWithRetries(uint32_t &epochUnixUTC) {
  for (int attempt = 0; attempt < MAX_NTP_ATTEMPTS; ++attempt) {
    const char* server = NTP_SERVERS[attempt % NTP_SERVERS_COUNT];
    Serial.printf("NTP intento %d -> %s\n", attempt + 1, server);
    uint32_t got = 0;
    if (requestNTP(server, got, NTP_ATTEMPT_TIMEOUT_MS)) {
      epochUnixUTC = got;
      return true;
    }
    Serial.println("NTP no respondió (o no válido).");
    delay(NTP_RETRY_DELAY_MS);
  }
  return false;
}

// Convierte epoch UTC a segundos desde medianoche local (0..86399)
uint32_t secondsSinceMidnightLocal(uint32_t epochUnixUTC) {
  int64_t localEpoch = (int64_t)epochUnixUTC + (int64_t)TZ_OFFSET_SECONDS;
  uint32_t secondsOfDay = (uint32_t)((localEpoch % 86400 + 86400) % 86400);
  return secondsOfDay;
}

// Calcula segundos faltantes hasta 'hour:00:00' (ajusta +24h si ya pasó). Devuelve segundos (no micros)
uint64_t secondsUntilHour(uint32_t currentSeconds, uint8_t hour) {
  uint32_t target = (uint32_t)hour * 3600U;
  int64_t diff = (int64_t)target - (int64_t)currentSeconds;
  if (diff <= 0) diff += 24LL * 3600LL;
  return (uint64_t)diff;
}

// Calcula microsegundos hasta el siguiente horario del arreglo
uint64_t computeSleepMicros(uint32_t currentSeconds) {
  for (int i = 0; i < SCHEDULE_LEN; ++i) {
    uint8_t h = scheduleHours[i];
    uint64_t secs = secondsUntilHour(currentSeconds, h);
    // el primer horario con secs>0 será el objetivo (incluye caso wrap)
    if (secs > 0) {
      return secs * 1000000ULL;
    }
  }
  // fallback: 24h
  return (uint64_t)24ULL * 3600ULL * 1000000ULL;
}

// Detecta portal cautivo: hace GET al probeUrl y si hay redirección devuelve true
bool detectPortal() {
  HTTPClient http;
  WiFiClient httpClient;
  if (!http.begin(httpClient, probeUrl)) {
    return false;
  }
  http.addHeader("User-Agent", "Mozilla/5.0");
  int code = http.GET();
  http.end();
  // Redirección 3xx suele indicar portal; suponer 3xx => portal
  return (code >= 300 && code < 400);
}

// Ensure MQTT connected (sin TLS). Reintenta hasta publicar con exito (intentos limitados).
bool mqttPublishWithRetry(const char* topic, const char* payload, int maxAttempts = 5) {
  int attempts = 0;
  while (attempts < maxAttempts) {
    if (!mqttClient.connected()) {
      String clientId = "ESP-" + String(ESP.getChipId());
      Serial.print("Conectando MQTT...");
      if (mqttClient.connect(clientId.c_str(), mqttUser, mqttPass)) {
        Serial.println("OK");
      } else {
        Serial.printf("Fallo MQTT state=%d. Retry...\n", mqttClient.state());
        delay(2000);
        attempts++;
        continue;
      }
    }
    if (mqttClient.publish(topic, payload)) {
      Serial.println("Publicado MQTT OK");
      return true;
    } else {
      Serial.println("Publish fallo. Reintentando...");
      attempts++;
      delay(1000);
    }
  }
  return false;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);
  Serial.println("\n=== Inicio ESP-01: schedule NTP + DHT11 + MQTT ===");

  dht.begin();

  // Limpieza WiFi previa
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  // ----- Conectar WiFi (DHCP) -----
  Serial.printf("Conectando a SSID '%s' ...\n", ssid);
  if (strlen(wifi_pass) > 0) {
    WiFi.begin(ssid, wifi_pass);
  } else {
    WiFi.begin(ssid); // red que maneja auth por portal
  }

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000UL) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi NO conectado (seguiré intentando NTP igual; puede fallar).");
  }

  // Iniciar UDP para NTP
  udp.begin(2390);

  // Intentar NTP (hasta MAX_NTP_ATTEMPTS)
  uint32_t epochUTC = 0;
  bool ntpOk = syncNTPWithRetries(epochUTC);
  if (!ntpOk) {
    Serial.println("NTP falló tras varios intentos. Entrando en deep sleep 3 horas.");
    delay(50);
    ESP.deepSleep((uint64_t)FAIL_SLEEP_MICROS);
    // no vuelve aquí
    return;
  }

  // Obtener hora local (h:m:s) aproximada
  uint32_t secondsLocal = secondsSinceMidnightLocal(epochUTC);
  uint32_t hour = secondsLocal / 3600;
  uint32_t minute = (secondsLocal % 3600) / 60;
  uint32_t second = secondsLocal % 60;
  Serial.printf("Hora local aproximada: %02u:%02u:%02u\n", hour, minute, second);

  // Detectar portal cautivo -> si hay portal: no publicar MQTT
  bool portal = detectPortal();
  if (portal) {
    Serial.println("Portal cautivo DETECTADO. No publicar MQTT en este ciclo.");
  } else {
    // Leer DHT (solo si tenemos sensor y la lectura es válida)
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      // Preparar payload minimalista (solo topic — tu servidor distinguirá por topic)
      // Como pediste minimal: mandamos temperatura y humedad en un string simple
      char payload[64];
      snprintf(payload, sizeof(payload), "{\"t\":%.1f,\"h\":%.1f}", t, h);
      Serial.print("Payload: ");
      Serial.println(payload);

      // MQTT config y publicar (sin TLS). Intentos internos para asegurar llegada
      mqttClient.setServer(mqttServer, mqttPort);
      if (!mqttPublishWithRetry(topic, payload, 6)) {
        Serial.println("No se pudo publicar por MQTT después de reintentos.");
      }
      // desconectar para ahorrar energía (PubSubClient no tiene disconnect, se puede call client.disconnect)
      mqttClient.disconnect();
    } else {
      Serial.println("Lectura DHT fallida: no se publicará.");
    }
  }

  // Calcular tiempo restante hasta próximo schedule
  uint64_t sleepMicros = computeSleepMicros(secondsLocal);
  Serial.printf("Durmiendo %llu micros (aprox %.2f h)\n", (unsigned long long)sleepMicros, (double)sleepMicros / 1.0e6 / 3600.0);
  delay(50);

  // Deep Sleep -> requiere GPIO16 (XPD) conectado a RST para wakeup
  ESP.deepSleep(sleepMicros);
}

void loop() {
  // no se usa
}

