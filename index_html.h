#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Konfiguracja DCF77</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f4f6f9; color: #333; }
    .card { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); max-width: 400px; margin: 20px auto; }
    h2 { color: #0056b3; margin-top: 0; border-bottom: 2px solid #eee; padding-bottom: 10px; margin-bottom: 15px; }
    h3 { color: #555; margin-top: 15px; margin-bottom: 5px; font-size: 16px; border-bottom: 1px dashed #ccc; padding-bottom: 5px; }
    p { margin: 8px 0; font-size: 14px; }
    .status-val { font-weight: bold; color: #28a745; }
    .ip-val { font-weight: bold; color: #0056b3; }
    input[type=text], input[type=password], select { width: 100%; padding: 10px; margin: 8px 0 15px 0; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; }
    input[type=submit] { width: 100%; background: #28a745; color: white; padding: 12px; border: none; border-radius: 4px; font-size: 16px; cursor: pointer; margin-top: 10px; }
    input[type=submit]:hover { background: #218838; }
    .btn-logout { display: block; text-align: center; background: #dc3545; color: white; padding: 10px; border-radius: 4px; text-decoration: none; margin-top: 20px; font-size: 14px; font-weight: bold; }
    .btn-logout:hover { background: #bd2130; }
  </style>
  <script>
    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('time').innerText = data.time;
          document.getElementById('wifi').innerText = data.wifi;
          document.getElementById('local_ip').innerText = data.local_ip;
          document.getElementById('ap_ip').innerText = data.ap_ip;
          document.getElementById('cur_ntp').innerText = data.ntp_server;
          document.getElementById('cur_tz').innerText = data.tz_name;
        }).catch(err => console.error('Błąd pobierania statusu:', err));
    }
    setInterval(updateStatus, 1000);
    window.onload = updateStatus;
  </script>
</head>
<body>
  <div class='card'>
    <h2>Status Emulatora</h2>
    <p>Czas urządzenia:<br><span id='time' class='status-val'>Ładowanie...</span></p>
    <p>Sygnał Wi-Fi:<br><span id='wifi' class='status-val'>Ładowanie...</span></p>
    <p>Lokalny adres IP (STA):<br><span id='local_ip' class='ip-val'>Ładowanie...</span></p>
    <p>Adres IP konfiguracji (AP):<br><span id='ap_ip' class='ip-val'>Ładowanie...</span></p>
    <p>Serwer NTP:<br><span id='cur_ntp' class='status-val'>Ładowanie...</span></p>
    <p>Strefa czasowa:<br><span id='cur_tz' class='status-val'>Ładowanie...</span></p>
  </div>

  <div class='card'>
    <h2>Ustawienia Urządzenia</h2>
    <form action='/save' method='POST'>
      <h3>Połączenie Wi-Fi</h3>
      Nazwa nowej sieci (SSID):<br><input type='text' name='n_ssid' placeholder='Pozostaw puste, by nie zmieniać Wi-Fi'><br>
      Hasło sieci:<br><input type='password' name='n_password' placeholder='Hasło Wi-Fi'><br>
      
      <h3>Czas i Synchronizacja</h3>
      Serwer czasu NTP:<br><input type='text' name='n_ntp' value='pool.ntp.org'><br>
      Strefa czasowa:<br>
      <select name='n_tz'>
        <option value='CET-1CEST,M3.5.0,M10.5.0/3|Europa Środkowa (Warszawa)'>Europa Środkowa (Warszawa, Berlin)</option>
        <option value='GMT0BST,M3.5.0/1,M10.5.0/2|Wielka Brytania (Londyn)'>Wielka Brytania (Londyn)</option>
        <option value='EET-2EEST,M3.5.0/3,M10.5.0/4|Europa Wschodnia (Kijów, Wilno)'>Europa Wschodnia (Kijów, Helsinki)</option>
        <option value='UTC0|Czas uniwersalny (UTC)'>Czas uniwersalny (UTC)</option>
      </select><br>

      <h3>Bezpieczeństwo i Dostęp</h3>
      Nowe hasło administratora (admin):<br><input type='password' name='n_admin_pass' placeholder='Pozostaw puste, by nie zmieniać'><br>
      Nowe hasło do AP (ratunkowego):<br><input type='password' name='n_ap_pass' placeholder='Min. 8 znaków (puste = bez zmian)'><br>
      
      <input type='submit' value='Zapisz i Zastosuj'>
    </form>
    <a href='/logout' class='btn-logout'>Wyloguj z panelu</a>
  </div>
</body>
</html>
)=====";

#endif