#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include "index_html.h"
#include "dcf77_encoder.h"

// --- KONFIGURACJA SIECI DOMOWEJ (Domyślna) ---
const char* ssid     = "WIFI_NAME";
const char* password = "PASS";

// --- DOMYŚLNA KONFIGURACJA NTP I STREFY ---
String ntp_server   = "pool.ntp.org";
String time_zone    = "CET-1CEST,M3.5.0,M10.5.0/3"; 
String tz_name      = "Europa Środkowa (Warszawa)";

// --- MODYFIKOWALNA KONFIGURACJA BEZPIECZEŃSTWA ---
const char* www_user  = "admin";                
String www_pass       = "admin";             
const char* ap_ssid   = "Televox_DCF77_Konfig"; 
String ap_password    = "test123456789";          

const int dcfPin = 4; // GPIO4 = D2 na Wemos D1 Mini
const unsigned led_pin = LED_BUILTIN; 
ESP8266WebServer server(80); 
bool ap_active = false; 

String getSignalQuality(long rssi) {
  if (rssi == 31) return "Brak połączenia"; 
  if (rssi >= -50) return "Doskonały (" + String(rssi) + " dBm)";
  if (rssi >= -67) return "Dobry (" + String(rssi) + " dBm)";
  if (rssi >= -70) return "Średni (" + String(rssi) + " dBm)";
  if (rssi >= -80) return "Słaby (" + String(rssi) + " dBm)";
  return "Bardzo słaby (" + String(rssi) + " dBm)";
}

void handleRoot() {
  if (!server.authenticate(www_user, www_pass.c_str())) {
    return server.requestAuthentication();
  }
  server.send(200, "text/html", MAIN_page);
}

void handleStatus() {
  if (!server.authenticate(www_user, www_pass.c_str())) {
    return server.requestAuthentication();
  }

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char timeStr[32];
  
  if (timeinfo->tm_year > 100) {
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
  } else {
    strcpy(timeStr, "Oczekiwanie na synchronizację NTP...");
  }

  long rssi = WiFi.RSSI();
  String wifiStatus = getSignalQuality(rssi);
  String localIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "Rozłączony";
  String apIP = ap_active ? WiFi.softAPIP().toString() : "Wyłączone";

  String json = "{";
  json += "\"time\":\"" + String(timeStr) + "\",";
  json += "\"wifi\":\"" + wifiStatus + "\",";
  json += "\"local_ip\":\"" + localIP + "\",";
  json += "\"ap_ip\":\"" + apIP + "\",";
  json += "\"ntp_server\":\"" + ntp_server + "\",";
  json += "\"tz_name\":\"" + tz_name + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleSave() {
  if (!server.authenticate(www_user, www_pass.c_str())) {
    return server.requestAuthentication();
  }
  
  String new_ssid        = server.arg("n_ssid");
  String new_pass        = server.arg("n_password");
  String new_ntp         = server.arg("n_ntp");
  String new_tz_raw      = server.arg("n_tz");
  String new_admin_pass  = server.arg("n_admin_pass");
  String new_ap_pass     = server.arg("n_ap_pass");

  if (new_admin_pass.length() > 0) {
    www_pass = new_admin_pass;
  }

  if (new_ap_pass.length() >= 8) { 
    ap_password = new_ap_pass;
    if (ap_active) {
      WiFi.softAP(ap_ssid, ap_password.c_str());
    }
  }

  if (new_tz_raw.length() > 0) {
    int splitIndex = new_tz_raw.indexOf('|');
    if (splitIndex != -1) {
      time_zone = new_tz_raw.substring(0, splitIndex);
      tz_name = new_tz_raw.substring(splitIndex + 1);
    }
  }

  if (new_ntp.length() > 0) {
    ntp_server = new_ntp;
  }

  configTime(time_zone.c_str(), ntp_server.c_str());

  String resp = "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body style='font-family:Arial; text-align:center; padding-top:50px;'>";
  resp += "<h2>Konfiguracja została zaktualizowana!</h2>";
  
  if (new_ssid.length() > 0) {
    resp += "<p>Zmieniono dane Wi-Fi. ESP próbuje się połączyć z: <b>" + new_ssid + "</b></p>";
    resp += "</body></html>";
    server.send(200, "text/html", resp);
    delay(1000);
    WiFi.begin(new_ssid.c_str(), new_pass.c_str()); 
  } else {
    resp += "<p>Zmiany zastosowane pomyślnie.</p>";
    resp += "<script>setTimeout(function(){ window.location.href = '/'; }, 2500);</script></body></html>";
    server.send(200, "text/html", resp);
  }
}

void handleLogout() {
  String resp = "<!DOCTYPE html><html><head><meta charset='UTF-8'><style>body{font-family:Arial;text-align:center;padding-top:50px;background:#f4f6f9;color:#333;}.card{background:white;padding:30px;border-radius:8px;box-shadow:0 4px 10px rgba(0,0,0,0.1);max-width:400px;margin:0 auto;}</style></head><body>";
  resp += "<div class='card'><h2>Wylogowano pomyślnie</h2><p>Twoja sesja została zakończona.</p>";
  resp += "<a href='/' style='display:inline-block;background:#0056b3;color:white;padding:10px 20px;text-decoration:none;border-radius:4px;'>Zaloguj ponownie</a></div></body></html>";
  server.send(401, "text/html", resp); 
}

void setup() {
  Serial.begin(115200);
  pinMode(dcfPin, OUTPUT);
  digitalWrite(dcfPin, LOW); 
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);
  
  WiFi.hostname("DCF77_emulator");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { 
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.softAPdisconnect(true); 
    WiFi.mode(WIFI_STA);         
    ap_active = false;
  } else {
    WiFi.mode(WIFI_AP_STA);      
    WiFi.softAP(ap_ssid, ap_password.c_str());
    ap_active = true;
  }

  server.on("/", handleRoot);
  server.on("/status", handleStatus); 
  server.on("/save", handleSave);
  server.on("/logout", handleLogout);
  server.begin();

  configTime(time_zone.c_str(), ntp_server.c_str());
}

void loop() {
  server.handleClient();

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  static int last_sec = -1;

  static unsigned long last_wifi_check = 0;
  if (millis() - last_wifi_check > 10000) { 
    last_wifi_check = millis();
    if (ap_active && WiFi.status() == WL_CONNECTED) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      ap_active = false;
      configTime(time_zone.c_str(), ntp_server.c_str()); 
    }
    if (!ap_active && WiFi.status() != WL_CONNECTED) {
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(ap_ssid, ap_password.c_str());
      ap_active = true;
    }
  }

  bool time_synchronized = (timeinfo->tm_year > 100);

  if (timeinfo->tm_sec != last_sec) {
    last_sec = timeinfo->tm_sec;

    if (timeinfo->tm_sec == 0 && time_synchronized) {
      time_t next_minute = now + 60;
      struct tm* next_timeinfo = localtime(&next_minute);
      prepareDCFFrame(next_timeinfo);
    }

    if (time_synchronized) {
      if (timeinfo->tm_sec < 59) {
        int pulseDuration = dcfFrame[timeinfo->tm_sec] ? 200 : 100; 
        digitalWrite(dcfPin, HIGH); 
        digitalWrite(led_pin, HIGH);
        delay(pulseDuration);
        digitalWrite(dcfPin, LOW);
        led_pin, LOW;
        digitalWrite(led_pin, LOW);
      } else {
        digitalWrite(dcfPin, LOW);
        digitalWrite(led_pin, LOW);
      }
    } else {
      digitalWrite(dcfPin, LOW);
      digitalWrite(led_pin, HIGH);
      delay(50);
      digitalWrite(led_pin, LOW);
    }
  }
  delay(10); 
}