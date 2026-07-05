#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <ElegantOTA.h>
#include <LittleFS.h>
#include "index_html.h"
#include "dcf77_encoder.h"

// --- KONFIGURACJA (Domyślne wartości zastępowane danymi z LittleFS) ---
String sta_ssid     = "wifi_name";
String sta_password = "wifi_pass";
String ntp_server   = "pool.ntp.org";
String time_zone    = "CET-1CEST,M3.5.0,M10.5.0/3"; 
String tz_name      = "Europa Środkowa (Warszawa)";
String www_user     = "admin";
String www_pass     = "admin";             
String ap_ssid      = "Televox_DCF77_Konfig";
String ap_password  = "Test1234";          

const int dcfPin = 4; // GPIO4 = D2
const unsigned led_pin = LED_BUILTIN; 
ESP8266WebServer server(80); 
bool ap_active = false;

// Zmienne do nieblokującego generowania impulsów DCF
unsigned long pulseStartMillis = 0;
int currentPulseDuration = 0;
bool pulseActive = false;

// --- FUNKCJE ZAPISU/ODCZYTU KONFIGURACJI (LittleFS) ---
void loadConfig() {
  if (LittleFS.begin()) {
    if (LittleFS.exists("/config.txt")) {
      File f = LittleFS.open("/config.txt", "r");
      if (f) {
        String t_ssid = f.readStringUntil('\n'); t_ssid.trim(); if(t_ssid.length() > 0) sta_ssid = t_ssid;
        String t_pass = f.readStringUntil('\n'); t_pass.trim(); if(t_pass.length() > 0) sta_password = t_pass;
        String t_ntp  = f.readStringUntil('\n'); t_ntp.trim();  if(t_ntp.length() > 0) ntp_server = t_ntp;
        String t_tz   = f.readStringUntil('\n'); t_tz.trim();   if(t_tz.length() > 0) time_zone = t_tz;
        String t_tzn  = f.readStringUntil('\n'); t_tzn.trim();  if(t_tzn.length() > 0) tz_name = t_tzn;
        String t_wwwp = f.readStringUntil('\n'); t_wwwp.trim(); if(t_wwwp.length() > 0) www_pass = t_wwwp;
        String t_app  = f.readStringUntil('\n'); t_app.trim();  if(t_app.length() >= 8) ap_password = t_app;
        f.close();
      }
    }
  }
}

void saveConfig() {
  File f = LittleFS.open("/config.txt", "w");
  if (f) {
    f.println(sta_ssid);
    f.println(sta_password);
    f.println(ntp_server);
    f.println(time_zone);
    f.println(tz_name);
    f.println(www_pass);
    f.println(ap_password);
    f.close();
  }
}

String getSignalQuality(long rssi) {
  if (rssi == 31) return F("Brak połączenia");
  if (rssi >= -50) return String(F("Doskonały (")) + rssi + F(" dBm)");
  if (rssi >= -67) return String(F("Dobry (")) + rssi + F(" dBm)");
  if (rssi >= -70) return String(F("Średni (")) + rssi + F(" dBm)");
  if (rssi >= -80) return String(F("Słaby (")) + rssi + F(" dBm)");
  return String(F("Bardzo słaby (")) + rssi + F(" dBm)");
}

void handleRoot() {
  if (!server.authenticate(www_user.c_str(), www_pass.c_str())) {
    return server.requestAuthentication();
  }
  
  // Wymuszenie braku cache dla głównego panelu
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  
  server.send_P(200, "text/html", MAIN_page);
}

void handleStatus() {
  if (!server.authenticate(www_user.c_str(), www_pass.c_str())) {
    return server.requestAuthentication();
  }

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char timeStr[64];
  
  if (timeinfo->tm_year > 100) {
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
  } else {
    snprintf(timeStr, sizeof(timeStr), "Oczekiwanie na synchronizacje NTP...");
  }

  long rssi = WiFi.RSSI();
  String wifiStatus = getSignalQuality(rssi);
  String localIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : F("Rozłączony");
  String apIP = ap_active ? WiFi.softAPIP().toString() : F("Wyłączone");

  // Optymalizacja pamięci za pomocą bufora char i snprintf (zamiast String concat)
  char jsonBuf[512];
  snprintf(jsonBuf, sizeof(jsonBuf), 
    "{\"time\":\"%s\",\"wifi\":\"%s\",\"local_ip\":\"%s\",\"ap_ip\":\"%s\",\"ntp_server\":\"%s\",\"tz_name\":\"%s\"}",
    timeStr, wifiStatus.c_str(), localIP.c_str(), apIP.c_str(), ntp_server.c_str(), tz_name.c_str()
  );

  server.send(200, "application/json", jsonBuf);
}

void handleSave() {
  if (!server.authenticate(www_user.c_str(), www_pass.c_str())) {
    return server.requestAuthentication();
  }
  
  String new_ssid        = server.arg("n_ssid");
  String new_pass        = server.arg("n_password");
  String new_ntp         = server.arg("n_ntp");
  String new_tz_raw      = server.arg("n_tz");
  String new_admin_pass  = server.arg("n_admin_pass");
  String new_ap_pass     = server.arg("n_ap_pass");

  bool networkChanged = false;

  if (new_ssid.length() > 0) {
    sta_ssid = new_ssid;
    sta_password = new_pass;
    networkChanged = true;
  }

  if (new_admin_pass.length() > 0) {
    www_pass = new_admin_pass;
  }

  if (new_ap_pass.length() >= 8) { 
    ap_password = new_ap_pass;
    if (ap_active) {
      WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
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

  // Zapis do pamięci trwałej LittleFS
  saveConfig();
  configTime(time_zone.c_str(), ntp_server.c_str());

  // Konstruowanie odpowiedzi (Zastosowano ochronę przed XSS)
  String resp = F("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body style='font-family:Arial; text-align:center; padding-top:50px;'>");
  resp += F("<h2>Konfiguracja została zaktualizowana i zapisana!</h2>");
  
  if (networkChanged) {
    resp += F("<p>Zmieniono dane Wi-Fi. Urządzenie próbuje połączyć się z nową siecią.</p>");
    resp += F("</body></html>");
    server.send(200, "text/html", resp);
    delay(1000);
    WiFi.begin(sta_ssid.c_str(), sta_password.c_str()); 
  } else {
    resp += F("<p>Zmiany zastosowane pomyślnie.</p>");
    resp += F("<script>setTimeout(function(){ window.location.href = '/'; }, 2500);</script></body></html>");
    server.send(200, "text/html", resp);
  }
}

void handleLogout() {
  // Wysyłamy żądanie autoryzacji dla zupełnie nowej strefy (Realm)
  // To zmusza Safari do porzucenia wcześniej zapisanego hasła.
  server.sendHeader("WWW-Authenticate", "Basic realm=\"Wylogowano - kliknij ANULUJ\"");
  
  String resp = F("<!DOCTYPE html><html><head><meta charset='UTF-8'><style>body{font-family:Arial;text-align:center;padding-top:50px;background:#f4f6f9;color:#333;}.card{background:white;padding:30px;border-radius:8px;box-shadow:0 4px 10px rgba(0,0,0,0.1);max-width:400px;margin:0 auto;}</style></head><body>");
  resp += F("<div class='card'><h2>Wylogowano pomyślnie</h2><p>Twoja sesja została ostatecznie zakończona.</p>");
  resp += F("<a href='/' style='display:inline-block;background:#0056b3;color:white;padding:10px 20px;text-decoration:none;border-radius:4px;'>Zaloguj ponownie</a></div></body></html>");
  
  server.send(401, "text/html", resp);
}

void setup() {
  Serial.begin(115200);
  pinMode(dcfPin, OUTPUT);
  digitalWrite(dcfPin, LOW); 
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, HIGH);
  
  // Wczytanie konfiguracji przy starcie
  loadConfig();

  WiFi.hostname("DCF77_emulator");
  WiFi.mode(WIFI_STA);
  WiFi.begin(sta_ssid.c_str(), sta_password.c_str());
  
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
    WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
    ap_active = true;
  }

  server.on("/", handleRoot);
  server.on("/status", handleStatus); 
  server.on("/save", handleSave);
  server.on("/logout", handleLogout);

  ElegantOTA.begin(&server, www_user.c_str(), www_pass.c_str()); 

  server.begin();
  configTime(time_zone.c_str(), ntp_server.c_str());
}

void loop() {
  server.handleClient();
  ElegantOTA.loop();

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
      WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
      ap_active = true;
    }
  }

  bool time_synchronized = (timeinfo->tm_year > 100);
  
  if (timeinfo->tm_sec != last_sec) {
    last_sec = timeinfo->tm_sec;

    if (time_synchronized) {
      if (timeinfo->tm_sec == 0) {
        time_t next_minute = now + 60;
        struct tm* next_timeinfo = localtime(&next_minute);
        prepareDCFFrame(next_timeinfo);
      }

      if (timeinfo->tm_sec < 59) {
        currentPulseDuration = dcfFrame[timeinfo->tm_sec] ? 200 : 100; 
        digitalWrite(dcfPin, HIGH); 
        digitalWrite(led_pin, LOW);
        pulseStartMillis = millis();
        pulseActive = true;
      } else {
        digitalWrite(dcfPin, LOW);
        digitalWrite(led_pin, HIGH);
        pulseActive = false;
      }
    } else {
      // Brak synchronizacji - równy, "awaryjny" sygnał co sekundę, nienależący do protokołu
      digitalWrite(dcfPin, HIGH);
      digitalWrite(led_pin, LOW);
      currentPulseDuration = 50; // Krótki impuls, by zasygnalizować błąd
      pulseStartMillis = millis();
      pulseActive = true;
    }
  }

  if (pulseActive && (millis() - pulseStartMillis >= (unsigned long)currentPulseDuration)) {
    digitalWrite(dcfPin, LOW);
    digitalWrite(led_pin, HIGH);
    pulseActive = false;
  }
}