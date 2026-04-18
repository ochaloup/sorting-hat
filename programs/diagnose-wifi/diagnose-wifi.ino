/*
 * ESP32 - DIAGNOSTIKA WiFi AP
 * ----------------------------
 * Testuje vytvoření WiFi Access Pointu:
 *   - Validace SSID a hesla
 *   - Výsledek softAP() volání
 *   - Skutečná konfigurace AP (read-back)
 *   - Monitorování připojených klientů
 *
 * Cíl: ověřit proč program-v2 vytvořil AP s generickým
 *      názvem a bez hesla místo "Sorting-Hat" / "m1spul3".
 *
 * Serial Monitor: 115200 baud
 */

#include <WiFi.h>
#include <esp_wifi.h>

// ====== KONFIGURACE - STEJNÁ JAKO V PROGRAM-V2 ======
const char* AP_SSID     = "Sorting-Hat";
const char* AP_PASSWORD = "mispulehat";  // min 8 znaku pro WPA2!

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("==============================================");
  Serial.println("  ESP32 - DIAGNOSTIKA WiFi AP");
  Serial.println("==============================================");
  Serial.println();

  // ===== TEST 1: Validace parametru =====
  Serial.println("[1] Validace parametru:");
  Serial.print("  SSID:  \"");
  Serial.print(AP_SSID);
  Serial.print("\" (delka: ");
  Serial.print(strlen(AP_SSID));
  Serial.println(")");

  Serial.print("  Heslo: \"");
  Serial.print(AP_PASSWORD);
  Serial.print("\" (delka: ");
  Serial.print(strlen(AP_PASSWORD));
  Serial.println(")");

  bool ssidOk = (AP_SSID != NULL && strlen(AP_SSID) > 0 && strlen(AP_SSID) <= 32);
  bool passOk = (AP_PASSWORD == NULL || strlen(AP_PASSWORD) == 0 || strlen(AP_PASSWORD) >= 8);

  Serial.print("  SSID validni (1-32 znaku):   ");
  Serial.println(ssidOk ? "OK" : "CHYBA!");
  Serial.print("  Heslo validni (0 nebo 8+):   ");
  Serial.println(passOk ? "OK" : "CHYBA! WPA2 vyzaduje min 8 znaku!");

  if (!passOk) {
    Serial.println();
    Serial.println("  *** TOTO JE PRICINA PROBLEMU V PROGRAM-V2! ***");
    Serial.println("  *** softAP() vrati false a AP se nevytvori    ***");
    Serial.println("  *** s pozadovanym SSID a heslem.              ***");
  }
  Serial.println();

  // ===== TEST 2: WiFi mode =====
  Serial.println("[2] Nastavuji WiFi mode na WIFI_AP...");
  bool modeOk = WiFi.mode(WIFI_AP);
  Serial.print("  WiFi.mode(WIFI_AP): ");
  Serial.println(modeOk ? "OK" : "SELHALO");
  Serial.println();

  // ===== TEST 3: softAP =====
  Serial.println("[3] Volam WiFi.softAP(\"" + String(AP_SSID) + "\", \"" + String(AP_PASSWORD) + "\")...");
  bool apOk = WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("  Navratova hodnota: ");
  Serial.println(apOk ? "true (uspech)" : "false (SELHALO!)");

  if (!apOk) {
    Serial.println();
    Serial.println("  softAP() selhalo! Mozne priciny:");
    Serial.println("    - Heslo kratsi nez 8 znaku (WPA2 minimum)");
    Serial.println("    - SSID prazdne nebo delsi nez 32 znaku");
    Serial.println("    - Interni chyba ESP32 WiFi");
    Serial.println();
    Serial.println("  ESP32 muze stale byt v AP modu s vychozim nastavenim!");
  }
  Serial.println();

  // ===== TEST 4: Cekani na nastartovani =====
  Serial.println("[4] Cekam 2s na nastartovani AP...");
  delay(2000);
  Serial.println();

  // ===== TEST 5: Read-back skutecne konfigurace =====
  Serial.println("[5] Skutecna konfigurace AP (read-back z ESP32):");

  // SSID
  String actualSSID = WiFi.softAPSSID();
  Serial.print("  Skutecny SSID:      \"");
  Serial.print(actualSSID);
  Serial.println("\"");

  bool ssidMatch = (actualSSID == String(AP_SSID));
  Serial.print("  Shoduje se s pozadovanym: ");
  Serial.println(ssidMatch ? "ANO" : "NE!");

  // IP
  IPAddress ip = WiFi.softAPIP();
  Serial.print("  IP adresa:          ");
  Serial.println(ip.toString());

  // MAC
  Serial.print("  MAC adresa:         ");
  Serial.println(WiFi.softAPmacAddress());

  // Pocet klientu
  Serial.print("  Pripojenych klientu: ");
  Serial.println(WiFi.softAPgetStationNum());

  Serial.println();

  // ===== TEST 6: Overeni ze AP je dostupny =====
  Serial.println("[6] Test ze AP odpovida:");
  wifi_config_t conf;
  esp_err_t err = esp_wifi_get_config(WIFI_IF_AP, &conf);
  if (err == ESP_OK) {
    Serial.print("  Config SSID:     \"");
    Serial.print((char*)conf.ap.ssid);
    Serial.println("\"");
    Serial.print("  Config password: \"");
    Serial.print((char*)conf.ap.password);
    Serial.println("\"");
    Serial.print("  Auth mode:       ");
    switch (conf.ap.authmode) {
      case WIFI_AUTH_OPEN:         Serial.println("OPEN (bez hesla!)"); break;
      case WIFI_AUTH_WPA_PSK:      Serial.println("WPA_PSK"); break;
      case WIFI_AUTH_WPA2_PSK:     Serial.println("WPA2_PSK"); break;
      case WIFI_AUTH_WPA_WPA2_PSK: Serial.println("WPA/WPA2_PSK"); break;
      case WIFI_AUTH_WPA3_PSK:     Serial.println("WPA3_PSK"); break;
      default: Serial.print("Jiny ("); Serial.print(conf.ap.authmode); Serial.println(")"); break;
    }
    Serial.print("  Kanal:           ");
    Serial.println(conf.ap.channel);
    Serial.print("  Max pripojeni:   ");
    Serial.println(conf.ap.max_connection);
  } else {
    Serial.print("  esp_wifi_get_config selhalo: ");
    Serial.println(err);
  }
  Serial.println();

  // ===== SHRNUTI =====
  Serial.println("==============================================");
  Serial.println("  SHRNUTI");
  Serial.println("==============================================");
  Serial.println();

  if (apOk && ssidMatch) {
    Serial.println("  WiFi AP FUNGUJE SPRAVNE!");
    Serial.print("  Pripoj se k WiFi \"");
    Serial.print(actualSSID);
    Serial.println("\"");
    Serial.print("  Heslo: ");
    Serial.println(AP_PASSWORD);
    Serial.print("  Adresa: http://");
    Serial.println(ip.toString());
  } else if (apOk && !ssidMatch) {
    Serial.println("  VAROVANI: softAP vratilo true, ale SSID nesouhlasi!");
    Serial.print("  Pozadovany: \""); Serial.print(AP_SSID); Serial.println("\"");
    Serial.print("  Skutecny:   \""); Serial.print(actualSSID); Serial.println("\"");
  } else {
    Serial.println("  CHYBA: WiFi AP SE NEVYTVORILO SPRAVNE!");
    Serial.println();
    Serial.println("  Nejcastejsi pricina: heslo kratsi nez 8 znaku.");
    Serial.println("  Oprava: zmen AP_PASSWORD na retezec s 8+ znaky.");
    Serial.println("  Priklad: \"m1spul3hat\" (10 znaku)");
  }

  Serial.println();
  Serial.println("Monitoruji pripojene klienty...");
  Serial.println("(Zkus se pripojit telefonem k WiFi)");
  Serial.println();
}

unsigned long lastCheck = 0;
int lastStationCount = -1;

void loop() {
  // Kazdych 5s zkontroluj pocet klientu
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    int stations = WiFi.softAPgetStationNum();
    if (stations != lastStationCount) {
      Serial.print("[");
      Serial.print(millis() / 1000);
      Serial.print("s] Pripojenych klientu: ");
      Serial.println(stations);
      lastStationCount = stations;
    }
  }
  delay(100);
}
