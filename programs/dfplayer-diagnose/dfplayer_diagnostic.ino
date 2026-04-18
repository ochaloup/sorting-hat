/*
 * ESP32 + DFPlayer Mini - DIAGNOSTIKA v2
 * ---------------------------------------
 * Používá knihovnu DFRobotDFPlayerMini (spolehlivější než raw UART).
 * Postupně testuje: komunikaci, SD kartu, hlasitost, přehrávání.
 *
 * Zapojení:
 *   ESP32 GPIO17 (TX2) → DFPlayer RX (pin 2, LEVÁ strana) - přímo, BEZ odporu
 *   ESP32 GPIO16 (RX2) ← DFPlayer TX (pin 3, LEVÁ strana) - přímo, BEZ odporu
 *   ESP32 3V3 (pin 1)  → DFPlayer VCC (pin 1) - POZOR: 3V3, NE VCC pin!
 *   ESP32 GND (pin 14)  → DFPlayer GND (pin 7)
 *
 * Serial Monitor: 115200 baud
 */

#include <HardwareSerial.h>
#include "DFRobotDFPlayerMini.h"

HardwareSerial dfSerial(2);
DFRobotDFPlayerMini dfPlayer;

void printDetail(uint8_t type, int value) {
  switch (type) {
    case DFPlayerCardInserted:    Serial.println("  >> SD karta vložena"); break;
    case DFPlayerCardRemoved:     Serial.println("  >> SD karta vyjmuta"); break;
    case DFPlayerCardOnline:      Serial.println("  >> SD karta online"); break;
    case DFPlayerPlayFinished:    Serial.print("  >> Skladba dohrána: #"); Serial.println(value); break;
    case DFPlayerError:
      Serial.print("  >> CHYBA: ");
      switch (value) {
        case Busy:               Serial.println("Modul zaneprázdněn"); break;
        case Sleeping:           Serial.println("Modul spí"); break;
        case SerialWrongStack:   Serial.println("Špatný formát dat"); break;
        case CheckSumNotMatch:   Serial.println("Checksum nesedí"); break;
        case FileIndexOut:       Serial.println("Soubor mimo rozsah"); break;
        case FileMismatch:       Serial.println("Soubor nenalezen"); break;
        case Advertise:          Serial.println("Reklama"); break;
        default:                 Serial.print("Kód "); Serial.println(value); break;
      }
      break;
    default:
      Serial.print("  >> Typ: "); Serial.print(type);
      Serial.print(" Hodnota: "); Serial.println(value);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("==========================================");
  Serial.println("  ESP32 + DFPlayer DIAGNOSTIKA v2");
  Serial.println("==========================================");
  Serial.println();

  // ===== TEST 1: UART =====
  Serial.println("[TEST 1] Inicializace UART2 (9600 baud)");
  Serial.println("  GPIO17 (TX) → DFPlayer RX (pin 2)");
  Serial.println("  GPIO16 (RX) ← DFPlayer TX (pin 3)");
  dfSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("  UART2 OK");
  Serial.println();

  // ===== TEST 2: DFPlayer init =====
  Serial.println("[TEST 2] Čekám 3s na boot DFPlayeru...");
  delay(3000);

  Serial.println("[TEST 3] Inicializuji DFPlayer knihovnu...");
  if (!dfPlayer.begin(dfSerial)) {
    Serial.println("  ❌ SELHALO! DFPlayer nenalezen.");
    Serial.println("  Zkontroluj:");
    Serial.println("    - Napájení z 3V3 pinu (NE VCC!)");
    Serial.println("    - GPIO17 → DFPlayer pin 2 (RX, LEVÁ strana)");
    Serial.println("    - GPIO16 ← DFPlayer pin 3 (TX, LEVÁ strana)");
    Serial.println("    - GND propojeny");
    Serial.println("    - SD karta vložena");
    Serial.println("    - Žádný odpor na TX/RX linkách");
    Serial.println();
    Serial.println("  Zastavuji diagnostiku.");
    while (true) delay(1000);
  }
  Serial.println("  ✅ DFPlayer nalezen a komunikuje!");
  Serial.println();

  dfPlayer.volume(0);

  // ===== TEST 4: Počet souborů =====
  Serial.println("[TEST 4] Čtu počet souborů na SD kartě...");
  delay(200);
  int fileCount = dfPlayer.readFileCounts();
  Serial.print("  Počet souborů: ");
  Serial.println(fileCount);
  if (fileCount <= 0) {
    Serial.println("  ⚠️ Žádné soubory! Zkontroluj SD kartu:");
    Serial.println("    - FAT32 formát");
    Serial.println("    - Složka 01/ s soubory 001.mp3, 002.mp3...");
  } else {
    Serial.println("  ✅ SD karta čitelná");
  }
  Serial.println();

  // ===== TEST 5: Stav =====
  Serial.println("[TEST 5] Dotaz na stav DFPlayeru...");
  int state = dfPlayer.readState();
  Serial.print("  Stav: ");
  Serial.println(state);
  Serial.println();

  // ===== TEST 6: Hlasitost =====
  Serial.println("[TEST 6] Nastavuji hlasitost na 20/30...");
  dfPlayer.volume(20);
  delay(200);
  int vol = dfPlayer.readVolume();
  Serial.print("  Aktuální hlasitost: ");
  Serial.println(vol);
  if (vol == 20) {
    Serial.println("  ✅ Hlasitost nastavena správně");
  } else {
    Serial.println("  ⚠️ Hlasitost se nenastavila (odpověď: " + String(vol) + ")");
  }
  Serial.println();

  // ===== TEST 7: Přehrávání =====
  Serial.println("[TEST 7] Přehrávám skladbu #1...");
  Serial.println("  (Pokud máš reproduktor, měla by hrát hudba)");
  dfPlayer.play(1);
  Serial.println("  ▶ Příkaz odeslán");
  Serial.println();

  // Počkej a zkontroluj jestli přišla chyba
  delay(2000);
  if (dfPlayer.available()) {
    uint8_t type = dfPlayer.readType();
    int value = dfPlayer.read();
    printDetail(type, value);
  } else {
    Serial.println("  ✅ Žádná chyba – skladba by měla hrát!");
  }
  Serial.println();

  // ===== SHRNUTÍ =====
  Serial.println("==========================================");
  Serial.println("  DIAGNOSTIKA DOKONČENA");
  Serial.println("==========================================");
  Serial.println();
  Serial.println("Pokud reproduktor hraje = vše OK!");
  Serial.println("Pokud nehraje ale testy prošly = problém s reproduktorem nebo MP3 souborem.");
  Serial.println();
  Serial.println("Monitoruji události DFPlayeru...");
  Serial.println();
}

void loop() {
  if (dfPlayer.available()) {
    uint8_t type = dfPlayer.readType();
    int value = dfPlayer.read();
    Serial.print("[");
    Serial.print(millis());
    Serial.print("ms] ");
    printDetail(type, value);
  }
  delay(50);
}
