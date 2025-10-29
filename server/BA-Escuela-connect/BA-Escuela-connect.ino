#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

// --- Config ---
const char* SSID = "BA Escuela";
const char* PROBE_URL = "http://detectportal.firefox.com/canonical.html";
const char* USER_AGENT = "Mozilla/5.0 (X11; Linux x86_64; rv:140.0) Gecko/20100101 Firefox/140.0";

// --- util: mostrar headers ---
void dumpResponseHeaders(HTTPClient &hc) {
  int nh = hc.headers();
  Serial.printf("Headers recibidos: %d\n", nh);
  for (int i = 0; i < nh; ++i) {
    String name = hc.headerName(i);
    String val  = hc.header(i);
    Serial.printf("  [%d] %s: %s\n", i, name.c_str(), val.c_str());
  }
}

// --- probe detect portal ---
String probeDetectPortal() {
  HTTPClient http;
  WiFiClient client;
  http.setTimeout(7000);
  if (!http.begin(client, PROBE_URL)) {
    Serial.println("http.begin falló (probe)");
    return String();
  }
  http.addHeader("User-Agent", USER_AGENT);
  http.addHeader("Accept", "*/*");
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("Pragma", "no-cache");

  int code = http.GET();
  Serial.printf("Probe HTTP code: %d\n", code);
  dumpResponseHeaders(http);

  // intentar Location header
  if (code >= 300 && code < 400) {
    String loc = http.header("Location");
    if (loc.length()) {
      Serial.println("Location encontrado (header): " + loc);
      http.end();
      return loc;
    }
  }

  // fallback: buscar en body <LoginURL> o https://.../splash/
  String body = http.getString();
  int p = body.indexOf("<LoginURL>");
  if (p >= 0) {
    int start = p + strlen("<LoginURL>");
    int end = body.indexOf("</LoginURL>", start);
    if (end > start) {
      String loginurl = body.substring(start, end);
      Serial.println("LoginURL extraído del XML: " + loginurl);
      http.end();
      return loginurl;
    }
  }
  p = body.indexOf("https://");
  if (p >= 0) {
    int q = body.indexOf('"', p);
    if (q < 0) q = body.indexOf('\'', p);
    if (q < 0) q = body.length();
    String candidate = body.substring(p, q);
    if (candidate.indexOf("/splash/") >= 0 || candidate.indexOf("network-auth.com") >= 0) {
      Serial.println("Posible splash extraído del body: " + candidate);
      http.end();
      return candidate;
    }
  }

  Serial.println("No se extrajo URL del portal.");
  http.end();
  return String();
}

// --- HEAD para Continue-Url ---
String headGetContinueUrl(const String &location) {
  if (location.length() == 0) return String();
  WiFiClientSecure wcs;
  wcs.setInsecure();
  HTTPClient https;
  https.setTimeout(7000);
  if (!https.begin(wcs, location)) {
    Serial.println("begin HEAD falló para: " + location);
    return String();
  }
  https.addHeader("User-Agent", USER_AGENT);
  https.addHeader("X-Requested-With", "XMLHttpRequest");

  int code = https.sendRequest("HEAD");
  Serial.printf("HEAD code: %d\n", code);
  dumpResponseHeaders(https);

  String cont = https.header("Continue-Url");
  if (cont.length() == 0) cont = https.header("Continue-url");
  if (cont.length() == 0) cont = https.header("continue-url");

  if (cont.length()) {
    Serial.println("Continue-Url (header): " + cont);
    https.end();
    return cont;
  }

  // fallback: GET corto
  int code2 = https.GET();
  Serial.printf("GET fallback code: %d\n", code2);
  dumpResponseHeaders(https);
  if (code2 > 0) {
    String body = https.getString();
    int p = body.indexOf("Continue-Url");
    if (p >= 0) {
      int colon = body.indexOf(':', p);
      if (colon > 0) {
        int eol = body.indexOf('\n', colon);
        if (eol < 0) eol = body.length();
        String val = body.substring(colon+1, eol);
        val.trim();
        Serial.println("Continue-Url extraído del body: " + val);
        https.end();
        return val;
      }
    }
    p = body.indexOf("http");
    if (p >= 0) {
      int q = body.indexOf('"', p);
      if (q < 0) q = body.indexOf('\'', p);
      if (q < 0) q = body.length();
      String candidate = body.substring(p, q);
      Serial.println("Candidate Continue-url from body: " + candidate);
      https.end();
      return candidate;
    }
  }

  https.end();
  return String();
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID);

  Serial.print("Conectando a WiFi...");
  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 20) {
    delay(500); Serial.print(".");
    t++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No se pudo conectar al WiFi.");
    return;
  }
  Serial.println("Conectado. IP: " + WiFi.localIP().toString());

  // probe
  String splashURL = probeDetectPortal();
  if (splashURL.length() == 0) {
    Serial.println("No hubo redirect. Puede que ya estés autenticado o gateway no intercepta.");
    return;
  }
  Serial.println("URL Splash detectada: " + splashURL);

  // HEAD para Continue-Url
  String continueUrl = headGetContinueUrl(splashURL);
  Serial.println("Continue-Url final: " + continueUrl);
}

void loop() {
  // nada por ahora
}
