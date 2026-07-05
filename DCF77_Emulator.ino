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
String ap_password  = "Test123456789";

const int dcfPin = 4; // GPIO4 = D2
const unsigned led_pin = LED_BUILTIN;
ESP8266WebServer server(80);
bool ap_active = false;

// Limity długości pól konfiguracyjnych - zapobiegają nadmiernemu
// zużyciu RAM/pliku konfiguracyjnego przez bardzo długie dane wejściowe.
const size_t MAX_SSID_LEN  = 32;
const size_t MAX_PASS_LEN  = 63;
const size_t MAX_NTP_LEN   = 64;
const size_t MAX_TZNAME_LEN = 64;
const size_t MIN_ADMIN_PASS_LEN = 8; // wymagana minimalna długość nowego hasła panelu
const size_t MIN_AP_PASS_LEN = 8;    // minimalna długość wg wymagań WPA2

// Zmienne do nieblokującego generowania impulsów DCF
unsigned long pulseStartMillis = 0;
int currentPulseDuration = 0;
bool pulseActive = false;

// --- OCHRONA PRZED BRUTE-FORCE LOGOWANIA ---
static uint16_t failedAttempts = 0;
static unsigned long lockUntilMillis = 0;

// --- TOKEN CSRF (chroni /save przed atakiem Cross-Site Request Forgery) ---
String csrfToken = "";

// ---------------------------------------------------------------------
// Usuwa znaki CR/LF z tekstu oraz przycina go do maksymalnej długości.
// Zapobiega "wstrzykiwaniu" dodatkowych linii do pliku config.txt.
// ---------------------------------------------------------------------
String sanitizeField(String value, size_t maxLen) {
  value.replace("\r", "");
  value.replace("\n", "");
  if (value.length() > maxLen) {
    value = value.substring(0, maxLen);
  }
  return value;
}

String generateCsrfToken() {
  String token = "";
  token.reserve(32);
  for (int i = 0; i < 32; i++) {
    token += String(random(0, 16), HEX);
  }
  return token;
}

// --- FUNKCJE ZAPISU/ODCZYTU KONFIGURACJI (LittleFS) ---
void loadConfig() {
  if (LittleFS.begin()) {
    if (LittleFS.exists("/config.txt")) {
      File f = LittleFS.open("/config.txt", "r");
      if (f) {
        String t_ssid = f.readStringUntil('\n'); t_ssid.trim(); if (t_ssid.length() > 0) sta_ssid = t_ssid;
        String t_pass = f.readStringUntil('\n'); t_pass.trim(); if (t_pass.length() > 0) sta_password = t_pass;
        String t_ntp  = f.readStringUntil('\n'); t_ntp.trim();  if (t_ntp.length() > 0) ntp_server = t_ntp;
        String t_tz   = f.readStringUntil('\n'); t_tz.trim();   if (t_tz.length() > 0) time_zone = t_tz;
        String t_tzn  = f.readStringUntil('\n'); t_tzn.trim();  if (t_tzn.length() > 0) tz_name = t_tzn;
        String t_wwwp = f.readStringUntil('\n'); t_wwwp.trim(); if (t_wwwp.length() > 0) www_pass = t_wwwp;
        String t_app  = f.readStringUntil('\n'); t_app.trim();  if (t_app.length() >= MIN_AP_PASS_LEN) ap_password = t_app;
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
  if (WiFi.status() != WL_CONNECTED) return F("Brak połączenia");
  if (rssi >= -50) return String(F("Doskonały (")) + rssi + F(" dBm)");
  if (rssi >= -67) return String(F("Dobry (")) + rssi + F(" dBm)");
  if (rssi >= -70) return String(F("Średni (")) + rssi + F(" dBm)");
  if (rssi >= -80) return String(F("Słaby (")) + rssi + F(" dBm)");
  return String(F("Bardzo słaby (")) + rssi + F(" dBm)");
}

// Prosta ucieczka znaków specjalnych JSON (cudzysłów, backslash),
// tak by wartości pochodzące od użytkownika (ntp_server, tz_name)
// nie mogły uszkodzić struktury odpowiedzi JSON.
String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 4);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

// ---------------------------------------------------------------------
// Uwierzytelnianie z podstawową ochroną przed atakiem brute-force:
// po kilku nieudanych próbach wprowadzane jest rosnące opóźnienie.
// ---------------------------------------------------------------------
bool checkAuth() {
  if (millis() < lockUntilMillis) {
    server.send(429, "text/plain", "Zbyt wiele nieudanych prob logowania. Sprobuj ponownie za chwile.");
    return false;
  }

  if (!server.authenticate(www_user.c_str(), www_pass.c_str())) {
    failedAttempts++;
    if (failedAttempts >= 5) {
      lockUntilMillis = millis() + (10000UL * failedAttempts); // rosnąca blokada
    }
    server.requestAuthentication(BASIC_AUTH, "DCF77_Panel");
    return false;
  }

  failedAttempts = 0;
  return true;
}

void handleRoot() {
  if (!checkAuth()) return;

  // Wymuszenie braku cache dla głównego panelu
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  // Nowy token CSRF generowany przy każdym wejściu do panelu
  csrfToken = generateCsrfToken();

  String page = FPSTR(MAIN_page);
  page.replace("%CSRF_TOKEN%", csrfToken);

  server.send(200, "text/html; charset=utf-8", page);
}

void handleStatus() {
  if (!checkAuth()) return;

  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char timeStr[64];

  if (timeinfo->tm_year > 100) {
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
  } else {
    snprintf(timeStr, sizeof(timeStr), "Oczekiwanie na synchronizacje NTP...");
  }

  long rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
  String wifiStatus = getSignalQuality(rssi);
  String localIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : F("Rozłączony");
  String apIP = ap_active ? WiFi.softAPIP().toString() : F("Wyłączone");

  String ntpEsc = jsonEscape(ntp_server);
  String tzEsc  = jsonEscape(tz_name);

  // Optymalizacja pamięci za pomocą bufora char i snprintf (zamiast String concat)
  char jsonBuf[512];
  snprintf(jsonBuf, sizeof(jsonBuf),
    "{\"time\":\"%s\",\"wifi\":\"%s\",\"local_ip\":\"%s\",\"ap_ip\":\"%s\",\"ntp_server\":\"%s\",\"tz_name\":\"%s\"}",
    timeStr, wifiStatus.c_str(), localIP.c_str(), apIP.c_str(), ntpEsc.c_str(), tzEsc.c_str()
  );

  server.send(200, "application/json; charset=utf-8", jsonBuf);
}

void handleSave() {
  if (!checkAuth()) return;

  // --- Ochrona CSRF: żądanie musi zawierać token wygenerowany dla tej sesji panelu ---
  String submittedToken = server.arg("csrf_token");
  if (submittedToken.length() == 0 || submittedToken != csrfToken) {
    server.send(403, "text/plain", "Nieprawidlowy token CSRF. Odswiez strone panelu i sprobuj ponownie.");
    return;
  }
  // Token jednorazowy - po użyciu unieważniamy, wymusza ponowne załadowanie strony
  csrfToken = "";

  String new_ssid        = sanitizeField(server.arg("n_ssid"), MAX_SSID_LEN);
  String new_pass        = sanitizeField(server.arg("n_password"), MAX_PASS_LEN);
  String new_ntp         = sanitizeField(server.arg("n_ntp"), MAX_NTP_LEN);
  String new_tz_raw      = sanitizeField(server.arg("n_tz"), 128);
  String new_admin_pass  = sanitizeField(server.arg("n_admin_pass"), MAX_PASS_LEN);
  String new_ap_pass     = sanitizeField(server.arg("n_ap_pass"), MAX_PASS_LEN);

  bool networkChanged = false;
  String warnings = "";

  if (new_ssid.length() > 0) {
    sta_ssid = new_ssid;
    sta_password = new_pass;
    networkChanged = true;
  }

  if (new_admin_pass.length() > 0) {
    if (new_admin_pass.length() < MIN_ADMIN_PASS_LEN) {
      warnings += F("<p style='color:#dc3545;'>Nowe hasło administratora zbyt krótkie (min. 8 znaków) - nie zostało zmienione.</p>");
    } else {
      www_pass = new_admin_pass;
    }
  }

  if (new_ap_pass.length() > 0) {
    if (new_ap_pass.length() < MIN_AP_PASS_LEN) {
      warnings += F("<p style='color:#dc3545;'>Nowe hasło AP zbyt krótkie (min. 8 znaków) - nie zostało zmienione.</p>");
    } else {
      ap_password = new_ap_pass;
      if (ap_active) {
        WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
      }
    }
  }

  if (new_tz_raw.length() > 0) {
    int splitIndex = new_tz_raw.indexOf('|');
    if (splitIndex != -1) {
      time_zone = new_tz_raw.substring(0, splitIndex);
      tz_name = new_tz_raw.substring(splitIndex + 1, min((size_t)new_tz_raw.length(), splitIndex + 1 + MAX_TZNAME_LEN));
    }
  }

  if (new_ntp.length() > 0) {
    ntp_server = new_ntp;
  }

  // Zapis do pamięci trwałej LittleFS
  saveConfig();
  configTime(time_zone.c_str(), ntp_server.c_str());

  // Konstruowanie odpowiedzi (dane wyświetlane to statyczny tekst, brak echo danych wejściowych -> brak ryzyka XSS)
  String resp = F("<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body style='font-family:Arial; text-align:center; padding-top:50px;'>");
  resp += F("<h2>Konfiguracja została zaktualizowana i zapisana!</h2>");
  resp += warnings;

  if (networkChanged) {
    resp += F("<p>Zmieniono dane Wi-Fi. Urządzenie próbuje połączyć się z nową siecią.</p>");
    resp += F("</body></html>");
    server.send(200, "text/html; charset=utf-8", resp);
    delay(1000);
    WiFi.begin(sta_ssid.c_str(), sta_password.c_str());
  } else {
    resp += F("<p>Zmiany zastosowane pomyślnie.</p>");
    resp += F("<script>setTimeout(function(){ window.location.href = '/'; }, 2500);</script></body></html>");
    server.send(200, "text/html; charset=utf-8", resp);
  }
}

void handleLogout() {
  // Strona informacyjna po wylogowaniu - zwracana ZAWSZE ze statusem 200.
  // Wcześniej ten adres zwracał 401 z nowym nagłówkiem WWW-Authenticate, ale
  // Safari ma udokumentowany, wieloletni błąd: przy odpowiedzi 401 potrafi
  // NIE wyświetlić dołączonej treści HTML i pokazać zamiast niej pusty/biały
  // ekran. Dlatego ta strona jest teraz zwykłą, publiczną odpowiedzią 200 -
  // dzięki temu wyświetli się poprawnie w każdej przeglądarce. Faktyczne
  // unieważnianie zapamiętanych danych logowania odbywa się osobno, w tle,
  // pod adresem /logout-auth (patrz forceLogout() w panelu HTML).
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");

  String resp = F("<!DOCTYPE html><html><head><meta charset='UTF-8'><style>body{font-family:Arial;text-align:center;padding-top:50px;background:#f4f6f9;color:#333;}.card{background:white;padding:30px;border-radius:8px;box-shadow:0 4px 10px rgba(0,0,0,0.1);max-width:400px;margin:0 auto;}</style></head><body>");
  resp += F("<div class='card'><h2>Wylogowano pomyślnie</h2><p>Twoja sesja została zakończona.</p>");
  resp += F("<a href='/' style='display:inline-block;background:#0056b3;color:white;padding:10px 20px;text-decoration:none;border-radius:4px;'>Zaloguj ponownie</a></div></body></html>");

  server.send(200, "text/html; charset=utf-8", resp);
}

void handleLogoutAuth() {
  // Endpoint pomocniczy, odpytywany W TLE przez JavaScript (forceLogout()).
  // Chroniony TYM SAMYM realm co panel ("DCF77_Panel") - dzięki temu
  // przeglądarka wiąże go z tymi samymi zapamiętanymi danymi logowania.
  // Front-end celowo wysyła tu błędne dane logowania; otrzymana w
  // odpowiedzi 401 ma skłonić Safari (i inne przeglądarki) do porzucenia
  // zapisanego wcześniej hasła dla tego realm.
  //
  // Celowo NIE korzysta z checkAuth() i nie wpływa na licznik nieudanych
  // prób logowania (failedAttempts) - w przeciwnym razie samo kliknięcie
  // "Wyloguj" kilka razy z rzędu mogłoby uruchomić ochronę przed
  // brute-force i zablokować właściwe logowanie do panelu.
  if (!server.authenticate(www_user.c_str(), www_pass.c_str())) {
    server.requestAuthentication(BASIC_AUTH, "DCF77_Panel");
    return;
  }
  server.send(200, "text/plain", "ok");
}

void setup() {
  Serial.begin(115200);
  pinMode(dcfPin, OUTPUT);
  digitalWrite(dcfPin, LOW);
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, HIGH);

  // Ziarno generatora liczb losowych (potrzebne do tokenów CSRF)
  randomSeed(RANDOM_REG32 ^ micros());

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

  // Metody HTTP jawnie ograniczone: /save przyjmuje wyłącznie POST,
  // co eliminuje CSRF poprzez proste linki/obrazki (GET).
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/logout", HTTP_GET, handleLogout);
  server.on("/logout-auth", HTTP_GET, handleLogoutAuth);

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
