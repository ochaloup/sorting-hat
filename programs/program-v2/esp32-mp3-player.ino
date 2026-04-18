/*
 * ESP32 WiFi MP3 Player
 * ---------------------
 * ESP32-LPKit v2.4 + DFPlayer Mini (MH2024K-24SS)
 * Vytvoří WiFi AP, po připojení zobrazí webové rozhraní
 * pro přehrávání MP3 z SD karty. Heslo k WiFi = jediné zabezpečení.
 *
 * Zapojení:
 *   ESP32 GPIO17 (TX2) ------------> DFPlayer RX (pin 2, LEVÁ strana) - přímo, BEZ odporu
 *   ESP32 GPIO16 (RX2) <------------ DFPlayer TX (pin 3, LEVÁ strana) - přímo, BEZ odporu
 *   ESP32 3V3 (pin 1)  ------------> DFPlayer VCC (pin 1) - POZOR: 3V3, NE VCC pin!
 *   ESP32 GND (pin 14)  -----------> DFPlayer GND (pin 7)
 *   DFPlayer SPK1 (pin 6) --> Repro1(+) --> Repro1(-) --> Repro2(+) --> Repro2(-) --> SPK2 (pin 8)
 *
 * ⚠️ DŮLEŽITÉ:
 *   - Napájet DFPlayer z 3V3 pinu (pin 1), NE z VCC pinu (pin 15)!
 *     VCC pin je 0V při napájení přes programátor.
 *   - Odpor 1kΩ NENÍ potřeba – ESP32 pracuje na 3.3V logice.
 *   - RX a TX DFPlayeru jsou na LEVÉ fyzické straně modulu.
 *
 * Programování: přes LaskaKit CH9102 programmer (přepínač na 3.3V!)
 *
 * Knihovny (nainstaluj přes Arduino Library Manager):
 *   - DFRobotDFPlayerMini
 *   - ESPAsyncWebServer (fork: github.com/ESP32Async/ESPAsyncWebServer)
 *   - AsyncTCP (fork: github.com/ESP32Async/AsyncTCP)
 *
 * Board v Arduino IDE: ESP32 Dev Module
 * Upload Speed: 115200
 * DFPlayer UART: fixně 9600bps (nelze změnit u MH2024K-24SS)
 */

#include <WiFi.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HardwareSerial.h>
#include "DFRobotDFPlayerMini.h"

// ====== KONFIGURACE ======

const char* AP_SSID     = "Sorting-Hat";
const char* AP_PASSWORD = "mispulehat";  // min 8 znaku pro WPA2!

#define DFPLAYER_TX 17  // ESP32 TX -> DFPlayer RX (přímo, bez odporu)
#define DFPLAYER_RX 16  // ESP32 RX <- DFPlayer TX (přímo, bez odporu)

// ====== GLOBÁLNÍ PROMĚNNÉ ======

HardwareSerial dfSerial(2);  // UART2
DFRobotDFPlayerMini dfPlayer;
AsyncWebServer server(80);
DNSServer dnsServer;

enum PlayState { STOPPED, PLAYING, PAUSED };
PlayState currentState = STOPPED;
int currentTrack = 0;
int totalTracks = 0;
int currentVolume = 20;  // 0-30, výchozí ~66%

// ====== HTML STRÁNKA ======

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
  .np-title{font-size:1rem;color:#fff;margin-bottom:16px;min-height:1.2em}
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
let tracks=[];

async function api(path,body){
  try{
    const opts={};
    if(body){opts.method='POST';opts.headers={'Content-Type':'application/json'};opts.body=JSON.stringify(body)}
    const r=await fetch('/api'+path,opts);
    return await r.json();
  }catch(e){return null}
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
  await api('/'+cmd,{});
  pollStatus();
}

let volTimer=null;
async function setVolume(v){
  document.getElementById('vol-val').textContent=v;
  clearTimeout(volTimer);
  volTimer=setTimeout(()=>api('/volume',{volume:parseInt(v)}),150);
}

async function pollStatus(){
  const d=await api('/status');
  if(!d)return;

  const title=document.getElementById('np-title');
  const btnStop=document.getElementById('btn-stop');
  const btnPause=document.getElementById('btn-pause');
  const btnResume=document.getElementById('btn-resume');

  if(d.state==='stopped'){
    title.textContent='Nic se nepřehrává';
    btnStop.disabled=true;btnPause.disabled=true;btnResume.disabled=true;
  } else {
    const name=d.track>0&&d.track<=tracks.length?tracks[d.track-1]:'Skladba '+d.track;
    title.textContent=name;
    btnStop.disabled=false;
    btnPause.disabled=d.state==='paused';
    btnResume.disabled=d.state==='playing';
    btnPause.classList.toggle('active',d.state==='playing');
    btnResume.classList.toggle('active',d.state==='paused');
  }

  document.querySelectorAll('.track-item').forEach(el=>el.classList.remove('active'));
  if(d.track>0){const el=document.getElementById('tr-'+d.track);if(el)el.classList.add('active')}

  document.getElementById('vol').value=d.volume;
  document.getElementById('vol-val').textContent=d.volume;
}

loadTracks().then(()=>pollStatus());
setInterval(pollStatus,1000);
</script>
</body>
</html>
)rawliteral";

// ====== FUNKCE ======

void setupServer() {
  // Hlavní stránka — rovnou přehrávač
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", PLAYER_HTML);
  });

  // API: Get tracks
  server.on("/api/tracks", HTTP_GET, [](AsyncWebServerRequest *request) {
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
        request->send(200, "application/json", "{\"ok\":true}");
      } else {
        request->send(400, "application/json", "{\"error\":\"invalid track\"}");
      }
    });

  // API: Pause
  server.on("/api/pause", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (currentState == PLAYING) {
      dfPlayer.pause();
      currentState = PAUSED;
    }
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // API: Resume
  server.on("/api/resume", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (currentState == PAUSED) {
      dfPlayer.start();
      currentState = PLAYING;
    }
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // API: Stop
  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
    dfPlayer.stop();
    currentState = STOPPED;
    currentTrack = 0;
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // API: Volume
  server.on("/api/volume", HTTP_POST, [](AsyncWebServerRequest *request) {},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
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
    String stateStr = "stopped";
    if (currentState == PLAYING) stateStr = "playing";
    else if (currentState == PAUSED) stateStr = "paused";

    String json = "{\"state\":\"" + stateStr + "\","
                  "\"track\":" + String(currentTrack) + ","
                  "\"volume\":" + String(currentVolume) + ","
                  "\"totalTracks\":" + String(totalTracks) + "}";
    request->send(200, "application/json", json);
  });

  // Captive portal: presmeruj vsechny nezname URL na hlavni stranku
  // (Android/iOS/Windows testuje specificke URL pro detekci captive portalu)
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("http://192.168.4.1/");
  });

  server.begin();
  Serial.println("Web server spuštěn");
}

// ====== SETUP ======

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 MP3 Player ===");

  // Inicializace DFPlayer - opakuj dokud se neohlasi (boot trva 1-3s)
  dfSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  Serial.println("Inicializuji DFPlayer...");
  bool dfOk = false;
  for (int attempt = 1; attempt <= 5; attempt++) {
    Serial.print("  Pokus ");
    Serial.print(attempt);
    Serial.print("/5... ");
    if (dfPlayer.begin(dfSerial)) {
      Serial.println("OK!");
      dfOk = true;
      break;
    }
    Serial.println("neni odpoved");
    delay(1000);
  }
  if (!dfOk) {
    Serial.println("CHYBA: DFPlayer nenalezen po 5 pokusech!");
    while (true) { delay(1000); }
  }

  Serial.println("DFPlayer OK!");
  dfPlayer.volume(currentVolume);
  dfPlayer.setTimeOut(1000);

  // Cekej az DFPlayer ohlasi SD kartu (DFPlayerCardOnline)
  // misto slepeho delay - reagujeme na skutecny signal pripravenosti
  Serial.println("Cekam na SD kartu...");
  bool cardReady = false;
  unsigned long waitStart = millis();
  while (millis() - waitStart < 5000) {  // max 5s timeout
    if (dfPlayer.available()) {
      uint8_t type = dfPlayer.readType();
      if (type == DFPlayerCardOnline) {
        Serial.println("SD karta online!");
        cardReady = true;
        break;
      }
    }
    delay(10);
  }
  if (!cardReady) {
    Serial.println("SD karta se neohlasila (timeout 5s), zkousim cist...");
  }

  // Čti počet souborů na SD kartě (soubory v rootu, seřazené přes fatsort)
  totalTracks = dfPlayer.readFileCounts();
  if (totalTracks < 0) totalTracks = 0;
  Serial.println("Pocet skladeb na SD: " + String(totalTracks));

  // Validace WiFi hesla (WPA2 vyzaduje min 8 znaku)
  if (strlen(AP_PASSWORD) > 0 && strlen(AP_PASSWORD) < 8) {
    Serial.println("CHYBA: WiFi heslo \"" + String(AP_PASSWORD) + "\" ma jen "
                   + String(strlen(AP_PASSWORD)) + " znaku. WPA2 vyzaduje min 8!");
    Serial.println("Zmen AP_PASSWORD v kodu na retezec s 8+ znaky.");
    while (true) { delay(1000); }
  }

  // Spuštění WiFi AP
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("CHYBA: WiFi.softAP() selhalo!");
    while (true) { delay(1000); }
  }

  // Pockej az AP dostane IP adresu
  Serial.print("Cekam na WiFi AP");
  IPAddress ip;
  unsigned long wifiStart = millis();
  while (millis() - wifiStart < 5000) {
    ip = WiFi.softAPIP();
    if (ip != IPAddress(0, 0, 0, 0)) break;
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  Serial.println("WiFi AP spusten: " + String(AP_SSID));
  Serial.println("IP adresa: " + ip.toString());

  // Captive portal - vsechny DNS dotazy presmeruje na ESP32
  dnsServer.start(53, "*", ip);
  Serial.println("DNS captive portal spusten");

  // Spuštění webserveru
  setupServer();

  Serial.println("\n>>> Pripoj se k WiFi '" + String(AP_SSID) +
                 "' a otevri http://" + ip.toString() + " <<<");
}

// ====== LOOP ======

void loop() {
  dnsServer.processNextRequest();

  if (currentState == PLAYING) {
    if (dfPlayer.available()) {
      uint8_t type = dfPlayer.readType();
      if (type == DFPlayerPlayFinished) {
        Serial.println("Skladba dohrana.");
        currentState = STOPPED;
        currentTrack = 0;
      }
    }
  }

  delay(50);
}
