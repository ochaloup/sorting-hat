# ESP32 WiFi MP3 Player

## Co potřebuješ

### Hardware
- ESP32-LPKit (LaskaKit)
- DFPlayer Mini (MH2024K-24SS)
- Reproduktor 3W 4Ω
- micro SD karta (FAT32, max 32GB)
- 1kΩ odpor (hnědá-černá-červená-zlatá)
- Breadboard + Dupont kabely
- USB-C kabel + nabíječka (pro prototyp)

### Software
- Arduino IDE 2.x (https://www.arduino.cc/en/software)
- Nebo PlatformIO

## Instalace Arduino IDE

1. Otevři Arduino IDE
2. **Přidej ESP32 board:**
   - File → Preferences → Additional Board Manager URLs:
   - Vlož: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Board Manager → hledej "esp32" → nainstaluj "ESP32 by Espressif"
3. **Nainstaluj knihovny** (Sketch → Include Library → Manage Libraries):
   - `DFRobotDFPlayerMini`
   - `ESPAsyncWebServer` (od me-no-dev) - nutné stáhnout z GitHubu:
     - https://github.com/me-no-dev/ESPAsyncWebServer (ZIP → Sketch → Include Library → Add .ZIP)
     - https://github.com/me-no-dev/AsyncTCP (ZIP → Sketch → Include Library → Add .ZIP)
4. **Vyber board:** Tools → Board → ESP32 Arduino → "ESP32 Dev Module"
5. **Vyber port:** Tools → Port → (vyber COM port s ESP32)

## Příprava SD karty

1. Naformátuj micro SD na FAT32
2. Vytvoř složku `01` v kořeni karty
3. Nakopíruj MP3 soubory do složky `01`:
   ```
   /01/001-nazev-skladby.mp3
   /01/002-dalsi-skladba.mp3
   /01/003-jina-skladba.mp3
   ```
   DŮLEŽITÉ: DFPlayer řadí soubory podle pořadí, v jakém byly na kartu nakopírovány,
   NE podle jména! Pro jistotu kopíruj soubory jeden po druhém v požadovaném pořadí.

## Zapojení

```
ESP32-LPKit          DFPlayer Mini
-----------          -------------
GPIO17 (TX2) --[1kΩ]--> RX
GPIO16 (RX2) <--------- TX
5V (VBUS)    ---------> VCC
GND          ---------> GND

DFPlayer Mini          Reproduktor
-------------          -----------
SPK1         ---------> + (kladný)
SPK2         ---------> - (záporný)
```

### Proč 1kΩ odpor?
ESP32 vysílá na 3.3V logice, DFPlayer přijímá 3.3V signál, ale odpor
chrání vstup DFPlayeru před případným přepětím. Je to ochranný prvek.

## Konfigurace

V souboru `esp32-mp3-player.ino` uprav:

```cpp
const char* AP_SSID     = "MP3-Player";      // Název WiFi sítě
const char* AP_PASSWORD = "mp3heslo123";      // Heslo k WiFi
const char* WEB_PASSWORD = "tajneheslo";      // Heslo do webového rozhraní
```

## Použití

1. Nahraj firmware na ESP32 (Upload v Arduino IDE)
2. Vlož SD kartu s MP3 do DFPlayeru
3. Zapni ESP32
4. Na telefonu se připoj k WiFi "MP3-Player" (heslo: mp3heslo123)
5. Otevři prohlížeč a jdi na `http://192.168.4.1`
6. Zadej heslo webového rozhraní
7. Vyber skladbu a přehrávej!

## Řešení problémů

- **DFPlayer nenalezen**: Zkontroluj zapojení, hlavně TX/RX (je to překřížené!
  TX z ESP jde do RX DFPlayeru). Zkontroluj, že SD karta je vložená.
- **Žádné skladby**: SD karta musí být FAT32, soubory ve složce /01/,
  pojmenované 001-xxx.mp3 atd.
- **Ticho**: Zkontroluj zapojení reproduktoru (SPK1/SPK2), zkus zvýšit
  hlasitost přes webové rozhraní.
- **WiFi se neobjeví**: Počkej ~5 sekund po zapnutí. Zkontroluj Serial
  Monitor (115200 baud) pro chybové hlášky.
