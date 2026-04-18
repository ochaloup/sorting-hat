/*
 * ESP32 WiFi MP3 Player
 * ---------------------
 * ESP32-LPKit + DFPlayer Mini
 * Vytvoří WiFi AP, po připojení a zadání hesla
 * zobrazí webové rozhraní pro přehrávání MP3 z SD karty.
 *
 * Zapojení:
 *   ESP32 GPIO17 (TX2) --[1kΩ]--> DFPlayer RX
 *   ESP32 GPIO16 (RX2) <--------- DFPlayer TX
 *   ESP32 5V (VBUS)    ---------> DFPlayer VCC
 *   ESP32 GND           ---------> DFPlayer GND
 *   DFPlayer SPK1/SPK2  ---------> Reproduktor 3W 4Ω
 *
 * Knihovny (nainstaluj přes Arduino Library Manager):
 *   - DFRobotDFPlayerMini
 *   - ESPAsyncWebServer (+ AsyncTCP)
 *
 * Board: ESP32 Dev Module (nebo LaskaKit ESP32-LPKit)
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HardwareSerial.h>
#include "DFRobotDFPlayerMini.h"

// ====== KONFIGURACE ======

// WiFi Access Point
const char* AP_SSID     = "MP3-Player";
const char* AP_PASSWORD = "mp3heslo123";  // WiFi heslo pro připojení k AP

// Heslo pro webové rozhraní
const char* WEB_PASSWORD = "tajneheslo";  // <-- ZDE ZMĚŇ HESLO

// DFPlayer Serial
#define DFPLAYER_TX 17  // ESP32 TX -> DFPlayer RX (přes 1kΩ)
#define DFPLAYER_RX 16  // ESP32 RX <- DFPlayer TX

// ====== GLOBÁLNÍ PROMĚNNÉ ======

HardwareSerial dfSerial(2);  // UART2
DFRobotDFPlayerMini dfPlayer;
AsyncWebServer server(80);

// Stav přehrávání
enum PlayState { STOPPED, PLAYING, PAUSED };
PlayState currentState = STOPPED;
int currentTrack = 0;
int totalTracks = 0;
int currentVolume = 20;  // 0-30, výchozí ~66%
unsigned long playStartTime = 0;
unsigned long pauseOffset = 0;

// Jednoduché session tokeny (basic auth náhrada)
String activeToken = "";

// ====== HTML STRÁNKA ======
// Stránka je uložena v PROGMEM (flash paměť)

const char LOGIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="cs">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MP3 Player - Login</title>
<style>
  *{margin:0;padding:0;box-sizing:border-box}
  body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
    background:#0a0a0f;color:#e0e0e0;min-height:100vh;
    display:flex;align-items:center;justify-content:center}
  .login-box{background:#14141f;border:1px solid #2a2a3a;border-radius:16px;
    padding:40px;width:320px;text-align:center}
  h1{font-size:1.4rem;margin-bottom:8px;color:#fff}
  p{font-size:.85rem;color:#888;margin-bottom:24px}
  input{width:100%;padding:12px 16px;border:1px solid #2a2a3a;border-radius:8px;
    background:#0a0a0f;color:#e0e0e0;font-size:1rem;outline:none;margin-bottom:16px}
  input:focus{border-color:#5b5bff}
  button{width:100%;padding:12px;border:none;border-radius:8px;
    background:#5b5bff;color:#fff;font-size:1rem;cursor:pointer;
    transition:background .2s}
  button:hover{background:#4a4ae0}
  .error{color:#ff5b5b;font-size:.85rem;margin-bottom:12px;display:none}
</style>
</head>
<body>
<div class="login-box">
  <h1>MP3 Player</h1>
  <p>Zadejte heslo pro přístup</p>
  <div class="error" id="err">Nesprávné heslo</div>
  <input type="password" id="pwd" placeholder="Heslo" autofocus
    onkeydown="if(event.key==='Enter')login()">
  <button onclick="login()">Přihlásit</button>
</div>
<script>
async function login(){
  const r=await fetch('/api/login',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({password:document.getElementById('pwd').value})});
  const d=await r.json();
  if(d.ok){localStorage.setItem('token',d.token);location.href='/player?token='+d.token}
  else{document.getElementById('err').style.display='block'}
}
</script>
</body>
</html>
)rawliteral";

const char PLAYER_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="cs">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>MP3 Player</title>
<style>
  *{margin:0;padding:0;box-sizing:border-box}
  body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
    background:#0a0a0f;color:#e0e0e0;min-height:100vh;padding:20px}
  .header{display:flex;justify-content:space-between;align-items:center;
    padding:16px 0;border-bottom:1px solid #1a1a2a;margin-bottom:20px}
  .header h1{font-size:1.2rem;color:#fff}
  .volume-wrap{display:flex;align-items:center;gap:10px}
  .volume-wrap label{font-size:.8rem;color:#888}
  .volume-wrap input[type=range]{width:100px;accent-color:#5b5bff}
  .vol-val{font-size:.8rem;color:#aaa;min-width:28px;text-align:right}

  .now-playing{background:#14141f;border:1px solid #2a2a3a;border-radius:12px;
    padding:20px;margin-bottom:20px}
  .np-title{font-size:1rem;color:#fff;margin-bottom:12px;min-height:1.2em}
  .progress-bar{width:100%;height:6px;background:#2a2a3a;border-radius:3px;
    margin-bottom:8px;overflow:hidden}
  .progress-fill{height:100%;background:#5b5bff;border-radius:3px;width:0%;
    transition:width .5s linear}
  .time-row{display:flex;justify-content:space-between;font-size:.75rem;color:#888;
    margin-bottom:16px}
  .controls{display:flex;gap:12px;justify-content:center}
  .ctrl-btn{width:48px;height:48px;border:1px solid #2a2a3a;border-radius:50%;
    background:#14141f;color:#e0e0e0;font-size:1.2rem;cursor:pointer;
    display:flex;align-items:center;justify-content:center;transition:all .2s}
  .ctrl-btn:hover{background:#2a2a3a;border-color:#5b5bff}
  .ctrl-btn.active{background:#5b5bff;border-color:#5b5bff;color:#fff}
  .ctrl-btn:disabled{opacity:.3;cursor:not-allowed}

  .track-list{list-style:none}
  .track-item{display:flex;align-items:center;gap:12px;padding:12px 16px;
    border:1px solid transparent;border-radius:10px;cursor:pointer;
    transition:all .15s;margin-bottom:4px}
  .track-item:hover{background:#14141f;border-color:#2a2a3a}
  .track-item.active{background:#1a1a2f;border-color:#5b5bff}
  .track-num{font-size:.75rem;color:#555;min-width:28px;text-align:right}
  .track-name{flex:1;font-size:.9rem}
  .track-play{width:32px;height:32px;border:none;border-radius:50%;
    background:#5b5bff;color:#fff;font-size:.9rem;cursor:pointer;
    opacity:0;transition:opacity .15s;display:flex;align-items:center;
    justify-content:center}
  .track-item:hover .track-play,.track-item.active .track-play{opacity:1}

  .status{text-align:center;padding:40px;color:#555;font-size:.9rem}
</style>
</head>
<body>
<div class="header">
  <h1>MP3 Player</h1>
  <div class="volume-wrap">
    <label>Vol</label>
    <input type="range" min="0" max="30" value="20" id="vol"
      oninput="setVolume(this.value)">
    <span class="vol-val" id="vol-val">20</span>
  </div>
</div>

<div class="now-playing">
  <div class="np-title" id="np-title">Nic se nepřehrává</div>
  <div class="progress-bar"><div class="progress-fill" id="prog"></div></div>
  <div class="time-row">
    <span id="time-cur">0:00</span>
    <span id="time-rem">0:00</span>
  </div>
  <div class="controls">
    <button class="ctrl-btn" id="btn-stop" onclick="sendCmd('stop')" disabled title="Stop">&#9632;</button>
    <button class="ctrl-btn" id="btn-pause" onclick="sendCmd('pause')" disabled title="Pause">&#10074;&#10074;</button>
    <button class="ctrl-btn" id="btn-resume" onclick="sendCmd('resume')" disabled title="Pokračovat">&#9654;</button>
  </div>
</div>

<ul class="track-list" id="tracks">
  <li class="status">Načítám seznam skladeb...</li>
</ul>

<script>
const token=new URLSearchParams(location.search).get('token')||localStorage.getItem('token')||'';
if(!token)location.href='/';

let tracks=[];
let state={playing:false,paused:false,track:0,elapsed:0,volume:20};
let pollTimer=null;

async function api(path,body){
  const opts={headers:{'Authorization':'Bearer '+token}};
  if(body){opts.method='POST';opts.headers['Content-Type']='application/json';opts.body=JSON.stringify(body)}
  const r=await fetch('/api'+path,opts);
  if(r.status===401){location.href='/';return null}
  return r.json();
}

async function loadTracks(){
  const d=await api('/tracks');
  if(!d)return;
  tracks=d.tracks||[];
  const ul=document.getElementById('tracks');
  if(!tracks.length){ul.innerHTML='<li class="status">Na SD kartě nejsou žádné MP3 soubory</li>';return}
  ul.innerHTML=tracks.map((t,i)=>`
    <li class="track-item" id="tr-${i+1}" onclick="playTrack(${i+1})">
      <span class="track-num">${String(i+1).padStart(2,'0')}</span>
      <span class="track-name">${escHtml(t)}</span>
      <button class="track-play" title="Přehrát">&#9654;</button>
    </li>`).join('');
}

function escHtml(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')}

async function playTrack(num){
  await api('/play',{track:num});
  pollStatus();
}

async function sendCmd(cmd){
  await api('/'+cmd);
  pollStatus();
}

async function setVolume(v){
  document.getElementById('vol-val').textContent=v;
  await api('/volume',{volume:parseInt(v)});
}

async function pollStatus(){
  const d=await api('/status');
  if(!d)return;
  state=d;

  // Update now playing
  const title=document.getElementById('np-title');
  const prog=document.getElementById('prog');
  const timeCur=document.getElementById('time-cur');
  const timeRem=document.getElementById('time-rem');
  const btnStop=document.getElementById('btn-stop');
  const btnPause=document.getElementById('btn-pause');
  const btnResume=document.getElementById('btn-resume');

  if(d.state==='stopped'){
    title.textContent='Nic se nepřehrává';
    prog.style.width='0%';
    timeCur.textContent='0:00';
    timeRem.textContent='0:00';
    btnStop.disabled=true;btnPause.disabled=true;btnResume.disabled=true;
  } else {
    const name=d.track>0&&d.track<=tracks.length?tracks[d.track-1]:'Skladba '+d.track;
    title.textContent=name;
    // DFPlayer nemá přesný elapsed time, používáme odhad
    const elapsed=d.elapsed||0;
    timeCur.textContent=fmtTime(elapsed);
    timeRem.textContent=d.duration?fmtTime(d.duration):'--:--';
    if(d.duration>0)prog.style.width=Math.min(100,(elapsed/d.duration)*100)+'%';
    btnStop.disabled=false;
    btnPause.disabled=d.state==='paused';
    btnResume.disabled=d.state==='playing';
    btnPause.classList.toggle('active',d.state==='playing');
    btnResume.classList.toggle('active',d.state==='paused');
  }

  // Highlight active track
  document.querySelectorAll('.track-item').forEach(el=>el.classList.remove('active'));
  if(d.track>0){const el=document.getElementById('tr-'+d.track);if(el)el.classList.add('active')}

  // Volume
  document.getElementById('vol').value=d.volume;
  document.getElementById('vol-val').textContent=d.volume;
}

function fmtTime(s){
  s=Math.floor(s);
  const m=Math.floor(s/60);
  return m+':'+(s%60<10?'0':'')+(s%60);
}

// Poll every 1s
loadTracks().then(()=>pollStatus());
pollTimer=setInterval(pollStatus,1000);
</script>
</body>
</html>
)rawliteral";

// ====== FUNKCE ======

String generateToken() {
  String t = "";
  for (int i = 0; i < 32; i++) {
    t += String(random(0, 16), HEX);
  }
  return t;
}

bool checkAuth(AsyncWebServerRequest *request) {
  if (activeToken.isEmpty()) return false;
  // Check header
  if (request->hasHeader("Authorization")) {
    String auth = request->header("Authorization");
    if (auth.startsWith("Bearer ")) {
      return auth.substring(7) == activeToken;
    }
  }
  // Check query param
  if (request->hasParam("token")) {
    return request->getParam("token")->value() == activeToken;
  }
  return false;
}

void setupServer() {
  // Login page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", LOGIN_HTML);
  });

  // Player page (token check via JS)
  server.on("/player", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", PLAYER_HTML);
  });

  // API: Login
  server.on("/api/login", HTTP_POST, [](AsyncWebServerRequest *request) {},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String body = "";
      for (size_t i = 0; i < len; i++) body += (char)data[i];

      // Jednoduché parsování JSON (password field)
      int idx = body.indexOf("\"password\"");
      if (idx < 0) {
        request->send(200, "application/json", "{\"ok\":false}");
        return;
      }
      int q1 = body.indexOf(':', idx);
      int q2 = body.indexOf('"', q1 + 1);
      int q3 = body.indexOf('"', q2 + 1);
      String pwd = body.substring(q2 + 1, q3);

      if (pwd == WEB_PASSWORD) {
        activeToken = generateToken();
        request->send(200, "application/json",
          "{\"ok\":true,\"token\":\"" + activeToken + "\"}");
      } else {
        request->send(200, "application/json", "{\"ok\":false}");
      }
    });

  // API: Get tracks
  server.on("/api/tracks", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) {
      request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
      return;
    }
    String json = "{\"tracks\":[";
    for (int i = 1; i <= totalTracks; i++) {
      if (i > 1) json += ",";
      json += "\"Skladba " + String(i) + "\"";
    }
    json += "],\"total\":" + String(totalTracks) + "}";
    request->send(200, "application/json", json);
  });

  // API: Play track
  server.on("/api/play", HTTP_POST, [](AsyncWebServerRequest *request) {},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!checkAuth(request)) {
        request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
        return;
      }
      String body = "";
      for (size_t i = 0; i < len; i++) body += (char)data[i];

      int idx = body.indexOf("\"track\"");
      int colon = body.indexOf(':', idx);
      int end = body.indexOf('}', colon);
      String numStr = body.substring(colon + 1, end);
      numStr.trim();
      int track = numStr.toInt();

      if (track >= 1 && track <= totalTracks) {
        dfPlayer.play(track);
        currentTrack = track;
        currentState = PLAYING;
        playStartTime = millis();
        pauseOffset = 0;
        request->send(200, "application/json", "{\"ok\":true}");
      } else {
        request->send(400, "application/json", "{\"error\":\"invalid track\"}");
      }
    });

  // API: Pause
  server.on("/api/pause", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) {
      request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
      return;
    }
    if (currentState == PLAYING) {
      dfPlayer.pause();
      currentState = PAUSED;
      pauseOffset += millis() - playStartTime;
    }
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // API: Resume
  server.on("/api/resume", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) {
      request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
      return;
    }
    if (currentState == PAUSED) {
      dfPlayer.start();
      currentState = PLAYING;
      playStartTime = millis();
    }
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // API: Stop
  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) {
      request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
      return;
    }
    dfPlayer.stop();
    currentState = STOPPED;
    currentTrack = 0;
    pauseOffset = 0;
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // API: Volume
  server.on("/api/volume", HTTP_POST, [](AsyncWebServerRequest *request) {},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!checkAuth(request)) {
        request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
        return;
      }
      String body = "";
      for (size_t i = 0; i < len; i++) body += (char)data[i];

      int idx = body.indexOf("\"volume\"");
      int colon = body.indexOf(':', idx);
      int end = body.indexOf('}', colon);
      String numStr = body.substring(colon + 1, end);
      numStr.trim();
      int vol = numStr.toInt();

      if (vol >= 0 && vol <= 30) {
        dfPlayer.volume(vol);
        currentVolume = vol;
      }
      request->send(200, "application/json", "{\"ok\":true}");
    });

  // API: Status
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) {
      request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
      return;
    }

    String stateStr = "stopped";
    unsigned long elapsed = 0;

    if (currentState == PLAYING) {
      stateStr = "playing";
      elapsed = pauseOffset + (millis() - playStartTime);
    } else if (currentState == PAUSED) {
      stateStr = "paused";
      elapsed = pauseOffset;
    }

    String json = "{\"state\":\"" + stateStr + "\","
                  "\"track\":" + String(currentTrack) + ","
                  "\"elapsed\":" + String(elapsed / 1000) + ","
                  "\"duration\":0,"
                  "\"volume\":" + String(currentVolume) + ","
                  "\"totalTracks\":" + String(totalTracks) + "}";
    request->send(200, "application/json", json);
  });

  server.begin();
  Serial.println("Web server spuštěn");
}

// ====== SETUP ======

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 MP3 Player ===");

  // Inicializace DFPlayer
  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  delay(1000);

  Serial.println("Inicializuji DFPlayer...");
  if (!dfPlayer.begin(dfSerial)) {
    Serial.println("CHYBA: DFPlayer nenalezen! Zkontroluj zapojení.");
    Serial.println("1) ESP32 GPIO17 --[1kΩ]--> DFPlayer RX");
    Serial.println("2) ESP32 GPIO16 <--------- DFPlayer TX");
    Serial.println("3) 5V a GND připojeny?");
    Serial.println("4) SD karta vložena a naformátovaná FAT32?");
    while (true) { delay(1000); }  // Zastaví se zde
  }

  Serial.println("DFPlayer OK!");

  // Nastavení DFPlayeru
  dfPlayer.volume(currentVolume);
  dfPlayer.setTimeOut(500);
  delay(200);

  // Zjisti počet souborů
  totalTracks = dfPlayer.readFileCounts();
  if (totalTracks < 0) totalTracks = 0;
  Serial.println("Počet skladeb na SD: " + String(totalTracks));

  // Spuštění WiFi AP
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.println("WiFi AP spuštěn: " + String(AP_SSID));
  Serial.println("IP adresa: " + ip.toString());
  Serial.println("Web heslo: " + String(WEB_PASSWORD));

  // Spuštění webserveru
  setupServer();

  Serial.println("Připojte se k WiFi '" + String(AP_SSID) +
                 "' a otevřete http://" + ip.toString());
}

// ====== LOOP ======

void loop() {
  // Kontrola, zda DFPlayer dohrál
  if (currentState == PLAYING) {
    if (dfPlayer.available()) {
      uint8_t type = dfPlayer.readType();
      if (type == DFPlayerPlayFinished) {
        Serial.println("Skladba dohrána.");
        currentState = STOPPED;
        currentTrack = 0;
        pauseOffset = 0;
      }
    }
  }

  delay(50);  // Krátká pauza pro stabilitu
}
