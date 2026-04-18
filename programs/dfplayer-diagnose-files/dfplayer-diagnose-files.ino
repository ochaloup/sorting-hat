/*
 * ESP32 + DFPlayer Mini - DIAGNOSTIKA SOUBORŮ
 * ---------------------------------------------
 * Zjistí co DFPlayer vidí na SD kartě:
 *   - Celkový počet souborů
 *   - Počet složek
 *   - Počet souborů v každé složce
 *   - Zkouší přehrát různými metodami (play, playFolder, playMp3Folder)
 *   - Čte číslo právě přehrávaného souboru
 *
 * Účel: diagnostikovat proč WiFi program nevidí soubory,
 *       zatímco jednoduchý diagnostický program je přehraje.
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
    case DFPlayerCardInserted:    Serial.println("    >> SD karta vlozena"); break;
    case DFPlayerCardRemoved:     Serial.println("    >> SD karta vyjmuta"); break;
    case DFPlayerCardOnline:      Serial.println("    >> SD karta online"); break;
    case DFPlayerPlayFinished:    Serial.print("    >> Skladba dohrana: #"); Serial.println(value); break;
    case DFPlayerError:
      Serial.print("    >> CHYBA: ");
      switch (value) {
        case Busy:               Serial.println("Modul zaneprazdnen"); break;
        case Sleeping:           Serial.println("Modul spi"); break;
        case SerialWrongStack:   Serial.println("Spatny format dat"); break;
        case CheckSumNotMatch:   Serial.println("Checksum nesedi"); break;
        case FileIndexOut:       Serial.println("Soubor mimo rozsah"); break;
        case FileMismatch:       Serial.println("Soubor nenalezen"); break;
        case Advertise:          Serial.println("Reklama"); break;
        default:                 Serial.print("Kod "); Serial.println(value); break;
      }
      break;
    case DFPlayerFeedBack:
      Serial.print("    >> Feedback: "); Serial.println(value);
      break;
    default:
      Serial.print("    >> Typ: "); Serial.print(type);
      Serial.print(" Hodnota: "); Serial.println(value);
      break;
  }
}

// Vyprazdni frontu zprav z DFPlayeru
void drainMessages(unsigned long waitMs) {
  unsigned long start = millis();
  while (millis() - start < waitMs) {
    if (dfPlayer.available()) {
      uint8_t type = dfPlayer.readType();
      int value = dfPlayer.read();
      printDetail(type, value);
    }
    delay(10);
  }
}

// Zkusi prehrat a overi vysledek pres readCurrentFileNumber
void tryPlay(const char* label, void (*playFunc)()) {
  // Vyprazdni frontu pred testem
  drainMessages(300);

  Serial.print("  Zkousim: ");
  Serial.println(label);
  playFunc();
  delay(1000);  // DFPlayer potrebuje cas na start prehravani

  // Vyprazdni pripadne zpravy (ACK, status)
  while (dfPlayer.available()) {
    uint8_t type = dfPlayer.readType();
    int value = dfPlayer.read();
    if (type == DFPlayerError) {
      Serial.print("    CHYBA od DFPlayeru: ");
      printDetail(type, value);
      Serial.println("    -> SELHALO");
      dfPlayer.stop();
      delay(300);
      return;
    }
    // Ostatni zpravy (ACK, feedback) jen zalogujeme
    Serial.print("    zprava: ");
    printDetail(type, value);
  }

  // Hlavni test: ptame se DFPlayeru co prave hraje
  delay(200);
  int currentFile = dfPlayer.readCurrentFileNumber();
  int state = dfPlayer.readState();
  if (currentFile > 0) {
    Serial.print("    -> OK: hraje soubor #");
    Serial.print(currentFile);
    Serial.print(" (stav: ");
    Serial.print(state);
    Serial.println(")");
  } else {
    Serial.print("    -> readCurrentFileNumber=");
    Serial.print(currentFile);
    Serial.print(", readState=");
    Serial.println(state);
    Serial.println("    -> NEPREHRAVA (soubor nenalezen nebo chyba)");
  }

  dfPlayer.stop();
  delay(500);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("==============================================");
  Serial.println("  DFPlayer - DIAGNOSTIKA SOUBORU na SD karte");
  Serial.println("==============================================");
  Serial.println();

  // ===== INIT UART =====
  Serial.println("[1] Inicializace UART2 (9600 baud, GPIO16/17)");
  dfSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("  OK");
  Serial.println();

  // ===== INIT DFPlayer =====
  Serial.println("[2] Cekam 3s na boot DFPlayeru...");
  delay(3000);

  Serial.println("[3] Inicializuji DFPlayer knihovnu...");
  if (!dfPlayer.begin(dfSerial)) {
    Serial.println("  SELHALO! DFPlayer nenalezen.");
    Serial.println("  Zkontroluj zapojeni (3V3, GND, TX/RX).");
    while (true) delay(1000);
  }
  Serial.println("  DFPlayer nalezen!");
  Serial.println();

  // Nastav timeout a hlasitost
  dfPlayer.setTimeOut(1000);  // 1s timeout pro spolehlivejsi cteni
  dfPlayer.volume(0);         // ztisit pro diagnostiku
  delay(200);

  // Vyprazdni uvodní zpravy (card online apod.)
  Serial.println("[4] Vyprazdnuji uvodni zpravy DFPlayeru...");
  drainMessages(1000);
  Serial.println();

  // ===== CTENI SOUBORU =====
  Serial.println("==============================================");
  Serial.println("  INFORMACE O SD KARTE");
  Serial.println("==============================================");
  Serial.println();

  // Celkovy pocet souboru
  Serial.println("[5] readFileCounts() - celkovy pocet souboru na SD:");
  delay(100);
  int totalFiles = dfPlayer.readFileCounts();
  Serial.print("  Vysledek: ");
  Serial.println(totalFiles);
  Serial.println();

  // Pocet slozek
  Serial.println("[6] readFolderCounts() - pocet slozek na SD:");
  delay(100);
  int totalFolders = dfPlayer.readFolderCounts();
  Serial.print("  Vysledek: ");
  Serial.println(totalFolders);
  Serial.println();

  // Soubory v kazde slozce
  Serial.println("[7] readFileCountsInFolder() - soubory v kazde slozce:");
  if (totalFolders > 0) {
    for (int folder = 1; folder <= totalFolders && folder <= 99; folder++) {
      delay(200);
      int filesInFolder = dfPlayer.readFileCountsInFolder(folder);
      Serial.print("  Slozka ");
      if (folder < 10) Serial.print("0");
      Serial.print(folder);
      Serial.print("/: ");
      Serial.print(filesInFolder);
      Serial.println(" souboru");
    }
  } else {
    Serial.println("  Zadne slozky nalezeny (nebo readFolderCounts selhalo)");
    // Zkus to i tak pro slozku 01
    Serial.println("  Zkousim primo readFileCountsInFolder(1)...");
    delay(200);
    int f1 = dfPlayer.readFileCountsInFolder(1);
    Serial.print("  Slozka 01/: ");
    Serial.print(f1);
    Serial.println(" souboru");
  }
  Serial.println();

  // Retry readFileCountsInFolder s delsim delay
  Serial.println("[8] Retry readFileCountsInFolder(1) s delsim cekanim:");
  delay(1000);  // delsi pauza pred dalsim dotazem
  int retry1 = dfPlayer.readFileCountsInFolder(1);
  Serial.print("  Po 1s delay: ");
  Serial.println(retry1);
  delay(2000);
  int retry2 = dfPlayer.readFileCountsInFolder(1);
  Serial.print("  Po 2s delay: ");
  Serial.println(retry2);
  Serial.println();

  // Stav DFPlayeru
  Serial.println("[9] readState() - stav prehravace:");
  delay(100);
  int state = dfPlayer.readState();
  Serial.print("  Vysledek: ");
  Serial.println(state);
  Serial.println();

  // ===== ZKOUSKY PREHRAVANI =====
  Serial.println("==============================================");
  Serial.println("  ZKOUSKY PREHRAVANI (hlasitost 5/30)");
  Serial.println("==============================================");
  Serial.println();

  dfPlayer.volume(5);
  delay(200);

  // Metoda 1: play(1) - root, fyzicke poradi
  tryPlay("play(1) - prvni soubor v rootu (fyzicke poradi)", []() {
    dfPlayer.play(1);
  });
  Serial.println();

  // Metoda 2: playFolder(1, 1) - slozka 01, soubor 001
  tryPlay("playFolder(1, 1) - slozka 01, soubor 001", []() {
    dfPlayer.playFolder(1, 1);
  });
  Serial.println();

  // Metoda 3: playLargeFolder(1, 1)
  tryPlay("playLargeFolder(1, 1) - slozka 01, soubor 001 (large)", []() {
    dfPlayer.playLargeFolder(1, 1);
  });
  Serial.println();

  // Metoda 4: playMp3Folder(1) - specialni /MP3 slozka
  tryPlay("playMp3Folder(1) - specialni slozka /MP3, soubor 0001", []() {
    dfPlayer.playMp3Folder(1);
  });
  Serial.println();

  // Zkus prehrat i soubor 2, 3 pokud existuji
  if (totalFiles >= 2) {
    tryPlay("play(2) - druhy soubor v rootu", []() {
      dfPlayer.play(2);
    });
    Serial.println();
  }

  if (totalFiles >= 3) {
    tryPlay("play(3) - treti soubor v rootu", []() {
      dfPlayer.play(3);
    });
    Serial.println();
  }

  dfPlayer.volume(0);
  dfPlayer.stop();

  // ===== SHRNUTI =====
  Serial.println();
  Serial.println("==============================================");
  Serial.println("  SHRNUTI");
  Serial.println("==============================================");
  Serial.println();
  Serial.print("  readFileCounts():            ");
  Serial.println(totalFiles);
  Serial.print("  readFolderCounts():          ");
  Serial.println(totalFolders);
  Serial.print("  readFileCountsInFolder(1):   ");
  Serial.print(retry2);
  Serial.println(" (posledni retry)");
  Serial.println();

  if (totalFiles <= 0) {
    Serial.println("  PROBLEM: DFPlayer nevidi zadne soubory!");
    Serial.println("  Zkontroluj SD kartu (FAT32, soubory 001.mp3... v rootu).");
  } else if (totalFolders <= 0 && retry2 <= 0) {
    // readFolderCounts nefunguje (-1) a readFileCountsInFolder taky ne
    // = typicky stav na MH2024K-24SS se soubory v rootu
    Serial.println("  DOPORUCENI: readFileCounts() funguje, folder metody ne.");
    Serial.println("  WiFi program: pouzit readFileCounts() a play(N).");
    Serial.println("  SD karta: soubory 001.mp3, 002.mp3... v rootu + fatsort.");
  } else if (retry2 > 0) {
    Serial.println("  POZNAMKA: readFileCountsInFolder(1) funguje (s delay).");
    Serial.println("  Lze pouzit readFileCounts() + play(N)");
    Serial.println("  nebo readFileCountsInFolder(1) + playFolder(1, N).");
  } else {
    Serial.println("  DOPORUCENI: readFileCounts() funguje.");
    Serial.println("  WiFi program: pouzit readFileCounts() a play(N).");
  }

  Serial.println();
  Serial.println("Diagnostika dokoncena. Monitoruji udalosti...");
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
