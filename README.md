# Průvodce: ESP32-LPKit + DFPlayer Mini MP3 přehrávač
## Pro začátečníky – krok za krokem

---

## 1. Co máš za hardware

### ESP32-LPKit v2.4 (LA100057P)
- Vývojová deska od Laskakit s čipem ESP32-WROOM-32
- **USB-C konektor slouží POUZE k nabíjení baterie – NELZE přes něj nahrávat program!**
- Program se nahrává přes **programovací header na spodní straně desky**
- Na desce jsou dvě tlačítka: **FLASH** (boot) a **RESET**
- LED kontrolky: červená bliká = deska dostává napájení (nabíjí baterii), NEZNAMENÁ že běží program

### ⚠️ DŮLEŽITÉ: VCC pin vs 3V3 pin
- **Pin 1 (3V3)** = výstup interního LDO regulátoru → **vždy dává ~3.3V** ✅ Použij tento!
- **Pin 15 (VCC)** = průchozí USB napájení → **dává 0V když napájíš přes programátor!** ❌
- Pokud napájíš ESP32 přes CH9102 programátor, **VCC pin je mrtvý** (0V)
- DFPlayer a další periferie napájej z **pinu 3V3 (pin 1)**, ne z VCC!

### CH9102 Programmer (LA161061)
- USB programátor pro nahrávání kódu do ESP32
- Má **přepínač napětí 3.3V / 5V** – vždy přepnout na **3.3V!**
- Má **přepínač VCC OUT** – musí být **ON** aby napájel ESP32
- Tři LED: napájení (svítí stále), TX a RX (blikají při komunikaci)
- Zapojuje se do 6-pinového headeru na **spodní straně** ESP32-LPKit
- Pořadí pinů na programátoru: RTS – DTR – RX – TX – GND – VCC
- Pořadí pinů na ESP32-LPKit: RTS – DTR – TX – RX – GND – 3V3
- TX↔RX jsou záměrně překřížené – to je správně!

### DFPlayer Mini MH2024K-24SS (LA133022)
- MP3 přehrávač modul s UART komunikací
- UART rychlost: fixně **9600bps**, nelze změnit
- Napájení: 3.2–5V (optimum 4.2V), **3.3V z ESP32 funguje** (je na spodní hranici)
- Po zapnutí čekat **2.5–3 sekundy** než začneš komunikovat
- **RX a TX jsou na LEVÉ fyzické straně modulu** (piny 2 a 3)
- Maximálně 99 složek, 255 souborů/složka
- DFPlayer neumí posílat názvy souborů – komunikuje jen binárními příkazy

---

## 2. Zapojení pinů ESP32-LPKit

```
Levá řada    #    Pravá řada
─────────────────────────────────────
3V3 ★        1    GND
EN           2    GPIO23
GPIO36       3    GPIO22
GPIO39       4    GPIO1   (U0TXD)
GPIO34       5    GPIO3   (U0RXD)
GPIO35       6    GPIO21
GPIO32       7    GND
GPIO33       8    GPIO19
GPIO25       9    GPIO18
GPIO26       10   GPIO5
GPIO27       11   GPIO17  (UART2_TX → DFPlayer RX) ★
GPIO14       12   GPIO16  (UART2_RX ← DFPlayer TX) ★
GPIO12       13   GPIO4
GND ★        14   GPIO13
VCC (5V USB) 15   GPIO2

★ = používané piny pro DFPlayer
```

> ⚠️ **GPIO6–11 NIKDY nepoužívat** – jsou interně propojeny s flash pamětí ESP32!

> ⚠️ **VCC pin (15) ≠ 3V3 pin (1)!** VCC dává 0V při napájení přes programátor. Vždy používej 3V3!

---

## 3. Zapojení DFPlayer Mini

### ⚠️ KRITICKÉ: Správné napájecí zapojení
```
LEVÁ strana DFPlayeru    pin    pravá strana (nepotřebujeme)
────────────────────────────────────────────────────────────
VCC  ← 3V3 z ESP32 ★    1      16: BUSY
RX   ← GPIO17           2      15: USB-
TX   → GPIO16            3      14: USB+
DAC_R (nepoužito)        4      13: ADKEY2
DAC_L (nepoužito)        5      12: ADKEY1
SPK1 (+) → repro         6      11: IO2
GND  ← GND z ESP32       7      10: GND
SPK2 (-) ← repro         8       9: IO1

★ POZOR: Napájet z 3V3 pinu (pin 1 ESP32), NE z VCC pinu (pin 15)!
```

### Klíčové pravidlo propojení:
- **ESP32 GPIO17 (TX2) → DFPlayer RX (pin 2, LEVÁ strana) – přímo, BEZ odporu**
- **DFPlayer TX (pin 3, LEVÁ strana) → ESP32 GPIO16 (RX2) – přímo, BEZ odporu**
- TX a RX jsou překříženy – co jeden vysílá, druhý přijímá

### ⚠️ Odpor 1kΩ – NEPOTŘEBUJEŠ
- Datasheet DFPlayeru doporučuje 1kΩ odpor jen pro **5V MCU systémy**
- ESP32 pracuje na **3.3V logice** → odpor není nutný
- Při testování odpor způsoboval problémy s komunikací → **zapoj přímo bez odporu**

### Reproduktory v sérii (2× 3W 4Ω):
```
DFPlayer SPK1(+) → Repro1(+)
Repro1(–) → Repro2(+)
Repro2(–) → DFPlayer SPK2(–)
```
Dva 4Ω reproduktory v sérii = 8Ω celkem (tišší, ale bezpečné pro DFPlayer)

---

## 4. Struktura SD karty v DFPlayeru

SD karta musí být formátována jako **FAT32**.

```
SD karta/
└── 01/
    ├── 001_slytherin.mp3
    ├── 002_gryffindor.mp3
    ├── 003_hufflepuff.mp3
    └── 004_ravenclaw.mp3
```

- Složka musí být pojmenována **01** (dvouciferné číslo)
- Soubory musí začínat **třímístným číslem** (001, 002, ...)
- Za číslem může být identifikátor: `001_slytherin.mp3` ✅
- DFPlayer řadí soubory podle pořadí zápisu na kartu, ne abecedně!
- Bezpečný postup: formátuj kartu, pak zkopíruj soubory v pořadí 001, 002, 003...

### Příprava SD karty na Linuxu:
```bash
# Zkontroluj filesystem
sudo blkid /dev/sda1
# Musí obsahovat TYPE="vfat"

# Pokud potřebuješ formátovat:
sudo mkfs.vfat -F 32 /dev/sda1

# Zkontroluj zdraví karty:
sudo fsck /dev/sda1

# Vytvoř strukturu:
mkdir /media/$USER/SD/01
cp 001_slytherin.mp3 /media/$USER/SD/01/
cp 002_gryffindor.mp3 /media/$USER/SD/01/

# DŮLEŽITÉ: Bezpečně odeber SD kartu před vyjmutím!
sudo umount /dev/sda1
```

### API metody knihovny DFRobotDFPlayerMini (v1.0.6):

#### Dotazy na SD kartu:
- `dfPlayer.readFileCounts()` – celkový počet souborů na SD kartě (cmd 0x48)
- `dfPlayer.readFolderCounts()` – celkový počet složek na SD kartě (cmd 0x4F)
- `dfPlayer.readFileCountsInFolder(1)` – počet souborů ve složce 01/ (cmd 0x4E)
  - ⚠️ **Nespolehlivé!** Často vrací 0 i když soubory existují (potřebuje delší init)
- `dfPlayer.readCurrentFileNumber()` – číslo právě přehrávaného souboru (cmd 0x4C)
- `dfPlayer.readState()` – stav přehrávače (cmd 0x42)
- `dfPlayer.readVolume()` – aktuální hlasitost (cmd 0x43)

#### Přehrávání – pozor na rozdíly:
- `dfPlayer.play(1)` – přehraje soubor #1 z **rootu** SD karty (pořadí dle FAT tabulky, cmd 0x03)
- `dfPlayer.playFolder(1, 1)` – přehraje soubor 001 ze složky 01/ (cmd 0x0F, max 255 souborů/složka)
- `dfPlayer.playLargeFolder(1, 1)` – jako playFolder ale podporuje až 3000 souborů/složka (cmd 0x14)
- `dfPlayer.playMp3Folder(1)` – přehraje soubor 0001.mp3 ze speciální složky /MP3 (cmd 0x12)

#### Ovládání:
- `dfPlayer.pause()` / `dfPlayer.start()` – pauza / pokračování
- `dfPlayer.stop()` – zastavení
- `dfPlayer.next()` / `dfPlayer.previous()` – další / předchozí skladba
- `dfPlayer.volume(0-30)` – nastavení hlasitosti
- `dfPlayer.loop(fileNumber)` – opakování jedné skladby
- `dfPlayer.loopFolder(folderNumber)` – opakování celé složky
- `dfPlayer.randomAll()` – náhodné přehrávání
- `dfPlayer.enableLoopAll()` / `dfPlayer.disableLoopAll()` – smyčka všech skladeb
- `dfPlayer.EQ(0-5)` – ekvalizér (Normal, Pop, Rock, Jazz, Classic, Bass)

#### Timeout a spolehlivost:
- Výchozí timeout: **500ms** (nastavitelné přes `dfPlayer.setTimeOut(ms)`)
- Všechny read* metody jsou **blokující** – čekají na odpověď nebo timeout
- Knihovna **nemá retry** – při selhání vrací -1, opakování musí řešit volající
- V ACK módu (výchozí) se příkazy serializují – předchozí musí dostat odpověď než se pošle další

#### Limity DFPlayeru:
- Root: max 3000 souborů (pojmenované 0001.mp3 až 3000.mp3)
- Složky: max 99 (pojmenované 01 až 99)
- Soubory ve složce: max 255 přes `playFolder()`, max 3000 přes `playLargeFolder()`
- DFPlayer **neumí posílat názvy souborů** – jen počty a čísla
- Pořadí souborů = pořadí zápisu do FAT tabulky, ne abecední

#### Otestované výsledky na MH2024K-24SS (duben 2026):

Testováno s 2 MP3 soubory (`001.mp3`, `002.mp3`) v **rootu** SD karty, seřazeno přes `fatsort`.

**Dotazy – co funguje:**

| Metoda | Výsledek | Poznámka |
|---|---|---|
| `readFileCounts()` | **2** ✅ | Spolehlivé, funguje hned po init |
| `readFolderCounts()` | **-1** ❌ | Nefunguje na tomto čipu (timeout) |
| `readFileCountsInFolder(1)` | **-1 / 0** ❌ | Nespolehlivé i s delším delay |
| `readCurrentFileNumber()` | **OK** ✅ | Vrací číslo právě přehrávaného souboru |
| `readState()` | **Nespolehlivé** ⚠️ | Vrací -1 i během přehrávání |

**Přehrávání – co funguje:**

| Metoda | Soubory v rootu | Soubory ve složce 01/ |
|---|---|---|
| `play(N)` | ✅ **Funguje** | ✅ Přehraje (ale číslování nespolehlivé) |
| `playFolder(1, N)` | ❌ Nefunguje | ✅ Funguje |
| `playLargeFolder(1, N)` | ❌ Nefunguje | Netestováno |
| `playMp3Folder(N)` | ❌ Nefunguje | ❌ (vyžaduje /MP3 složku) |

**Doporučené řešení pro WiFi přehrávač:**
- SD karta: soubory `001.mp3`, `002.mp3`, ... přímo v **rootu** (bez složek)
- Po zápisu seřadit pořadí: `sudo fatsort /dev/sdX1`
- Zjistit počet: `readFileCounts()` (spolehlivé)
- Přehrát: `play(N)` kde N = 1 až počet souborů
- Ověřit přehrávání: `readCurrentFileNumber()` (spolehlivé)

---

## 5. Nastavení Arduino IDE

### Instalace board package:
1. File → Preferences → Additional Board Manager URLs:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Tools → Board Manager → hledat **esp32** → nainstalovat **esp32 by Espressif Systems** (verze 3.x)

### Nastavení pro nahrávání:
- **Board:** ESP32 Dev Module
- **Port:** /dev/ttyACM0 (Linux) nebo COMx (Windows)
- **Upload Speed:** 115200

### Potřebné knihovny:

> ⚠️ **KRITICKÉ: Musíš použít forky od ESP32Async!** Staré verze knihoven od me-no-dev
> způsobují crash `tcp_alloc` s ESP32 core 3.x. Viz sekce 7.

```bash
# Smaž staré verze
rm -rf ~/Arduino/libraries/ESPAsyncWebServer
rm -rf ~/Arduino/libraries/AsyncTCP

# Nainstaluj správné forky
cd ~/Arduino/libraries
git clone https://github.com/ESP32Async/ESPAsyncWebServer.git
git clone https://github.com/ESP32Async/AsyncTCP.git
```

Další knihovny (přes Library Manager v IDE):
- `DFRobotDFPlayerMini`

---

## 6. Nahrávání programu – krok za krokem

### Před každým nahráváním:
1. **Zavři Serial Monitor** – pokud je otevřený, blokuje port!
2. Zkontroluj že programátor je přepnutý na **3.3V**
3. Zkontroluj že **VCC OUT je ON**
4. Zkontroluj správný port v Tools → Port

### Postup nahrávání (Linux):
```bash
# Jednorázové nastavení práv (po přidání do skupiny se odhlás a přihlas)
sudo usermod -a -G dialout $USER

# Pokud to nestihneš a potřebuješ nahrát hned:
sudo chmod 666 /dev/ttyACM0
```

### Pokud se zobrazí "Failed to connect" nebo "No serial data received":

**Metoda 1 – automatická (měla by fungovat s CH9102):**
Klikni Upload a počkej

**Metoda 2 – ruční bootloader mód:**
1. Drž tlačítko **FLASH** na ESP32-LPKit
2. Klikni **Upload** v Arduino IDE
3. Až uvidíš `Connecting......` – pusť **FLASH**

**Metoda 3 – reset sekvence:**
1. Drž **FLASH**
2. Stiskni a pusť **RESET**
3. Pusť **FLASH**
4. Klikni Upload

### Restart programu bez nového uploadu:
- Stiskni tlačítko **RESET** na ESP32-LPKit → program se restartuje

---

## 7. Časté chyby a jejich řešení

### "Missing FQBN (Fully Qualified Board Name)"
**Příčina:** Nemáš vybranou desku v IDE
**Řešení:** Tools → Board → esp32 → ESP32 Dev Module

### "Path '/dev/ttyACM0' is not readable"
**Příčina:** Nemáš práva k sériovému portu
**Řešení:**
```bash
sudo chmod 666 /dev/ttyACM0
# nebo trvale:
sudo usermod -a -G dialout $USER
```

### "Failed to connect to ESP32: No serial data received"
**Příčiny a řešení:**
1. ESP32 nedostává napájení → zkontroluj programátor (VCC OUT ON, přepínač 3.3V)
2. Špatně zasunutý konektor programátoru → vysuň a zasuň znovu
3. Potřeba ručního bootloader módu → viz sekce 6
4. Modem Manager blokuje port (Linux) → `sudo systemctl stop ModemManager`
5. Serial Monitor je otevřený → zavři ho

### "DFPlayer nenalezen" v Serial Monitoru
**Příčiny (v pořadí pravděpodobnosti):**
1. **DFPlayer nemá napájení** → nejčastější problém! Zkontroluj že napájíš z **3V3 pinu (pin 1)**, NE z VCC pinu (pin 15)!
2. Prohozené TX/RX → zkontroluj že GPIO17 jde na RX DFPlayeru a GPIO16 na TX
3. Špatná strana DFPlayeru → RX a TX jsou na LEVÉ straně modulu!
4. SD karta není vložena → některé klony DFPlayeru nebootnou bez SD karty
5. Špatný kontakt v nepájivém poli → zkontroluj vodiče

### Echo problém (ESP32 slyší sám sebe)
**Příznaky:** V diagnostice RX data = přesná kopie TX dat
**Příčina:** GPIO16 a GPIO17 vodiče jsou propojeny navzájem, nebo RX pin ESP32 není zapojen na DFPlayer TX a chytá signál z blízkého TX vodiče
**Řešení:** Zkontroluj že vodiče nejsou zkřížené na breadboardu a že GPIO16 vede na pin 3 DFPlayeru (TX)

### Šum na UART lince (nekonečné FF 00 FF 00...)
**Příčina:** Nezapojený RX pin ESP32 chytá elektrický šum
**Řešení:** Zkontroluj že GPIO16 je fyzicky zapojen na TX pin DFPlayeru (pin 3)

### ⚠️ WiFi server crash: "assert failed: tcp_alloc" (reboot loop)
**Chybová hláška:**
```
assert failed: tcp_alloc /IDF/components/lwip/lwip/src/core/tcp.c:1854
(Required to lock TCPIP core functionality!)
```

**Příčina:** Nekompatibilní verze knihoven ESPAsyncWebServer a AsyncTCP s ESP32 core 3.x.
Staré verze od me-no-dev a mathieucarbou nesprávně alokují TCP sockety bez TCPIP locku.

**Řešení:** Nainstalovat novější forky od **ESP32Async**:
```bash
# Smaž VŠECHNY staré verze
rm -rf ~/Arduino/libraries/ESPAsyncWebServer
rm -rf ~/Arduino/libraries/AsyncTCP
rm -rf ~/Arduino/libraries/ESPAsyncWebSrv

# Zkontroluj že nezbývají staré kopie
ls ~/Arduino/libraries/ | grep -i async

# Nainstaluj správné forky (minimálně ESPAsyncWebServer 3.6.2, AsyncTCP 3.3.2)
cd ~/Arduino/libraries
git clone https://github.com/ESP32Async/ESPAsyncWebServer.git
git clone https://github.com/ESP32Async/AsyncTCP.git

# Zavři a znovu otevři Arduino IDE!
```

**Alternativní řešení:** Downgrade ESP32 core na verzi 3.0.7:
Tools → Board Manager → esp32 → změnit verzi na 3.0.7

**Zdroje:**
- https://github.com/me-no-dev/ESPAsyncWebServer/issues/1455
- https://github.com/espressif/arduino-esp32/issues/10781
- https://rntlab.com/question/solvedassert-failed-tcp_alloc-idf-components-lwip-lwip-src-core-tcp-c1851-required-to-lock-tcpip-core-functionality/

### Kompilační chyba ESPAsyncWebServer (mbedtls_md5_starts_ret)
**Příčina:** Stará verze knihovny od me-no-dev nekompatibilní s ESP32 core 3.x
**Řešení:** Nainstalovat fork od ESP32Async (viz výše) – řeší oba problémy najednou

### Zmatené znaky v Serial Monitoru
**Příčina:** Špatná rychlost Serial Monitoru
**Řešení:** Nastav **115200 baud** vpravo dole v Serial Monitoru

### Reproduktor cvaká při startu/uploadu
**Příčina:** DFPlayer dostává šum na RX pin během bootu ESP32
**Řešení:** Neškodné, kosmetický problém. Lze zmírnit pull-down odporem 10kΩ mezi DFPlayer RX (pin 2) a GND.

### readFileCountsInFolder(1) vrací 0 i když soubory existují
**Příčina:** DFPlayer někdy potřebuje delší dobu na inicializaci SD karty
**Řešení:** Přidej delší delay (3000ms) před čtením, nebo použij `readFileCounts()` pro celkový počet souborů

---

## 8. Měření multimetrem

### Nastavení multimetru:
- Přepínač na **V⎓** (DC voltage) – rovná čárka s třemi tečkami dole
- **NE na V~ (AC)** – to je střídavý proud a ukáže nesmysly!
- Rozsah **20** (= 0–20V)
- **Červená sonda = kladný pól (+)**
- **Černá sonda = záporný pól / GND (–)**

### Test že multimetr funguje:
Změř baterii uvnitř multimetru (9V) nebo jakoukoliv baterii – musí ukázat rozumnou hodnotu.

### Co měřit a co očekávat:

| Místo měření | Červená sonda | Černá sonda | Očekávaná hodnota |
|---|---|---|---|
| Ověření multimetru | + baterie 9V | – baterie 9V | ~7–9V |
| Programátor výstup | VCC pin | GND pin | ~3.3V |
| ESP32 3V3 pin | pin 1 (3V3) | pin 14 (GND) | ~3.3V |
| ESP32 VCC pin ⚠️ | pin 15 (VCC) | pin 14 (GND) | ~0V (při napájení přes programátor!) |
| DFPlayer VCC | pin 1 (VCC) | pin 7 (GND) | ~3.3V |

> ⚠️ Pokud změříš 0.07–0.4V místo 3.3V → napájení se nedostává!
> Nejčastější příčina: napájíš DFPlayer z VCC pinu místo 3V3 pinu.

> ⚠️ Pokud měříš nesmyslné hodnoty → zkontroluj zda nemáš přepínač na AC (střídavý proud ~) místo DC (stejnosměrný ⎓)!

### Tipy pro měření na malých pinech:
- Přimáčkni sondu **zboku** na pin – ne shora
- Použij **krokosvorky** pokud máš
- Na **nepájivém poli** zapíchni sondu do stejného sloupce jako měřený pin
- Pokud měříš na drátech – přimáčkni je ke stolu pro lepší kontakt

---

## 9. Diagnostika zapojení

### Jak poznat že ESP32 dostává napájení:
- Červená LED na desce bliká nebo svítí
- Multimetr ukáže ~3.3V mezi **3V3 (pin 1)** a **GND (pin 14)**

### Jak poznat že programátor funguje:
- LED napájení na programátoru svítí
- Po připojení k PC se v `/dev/` objeví `ttyACM0`
- TX/RX LED blikají při nahrávání

### Jak poznat že DFPlayer funguje:
- V Serial Monitoru (115200 baud) po resetu uvidíš:
  ```
  DFPlayer OK!
  Hraje skladba 1...
  ```
- Pokud vidíš `CHYBA: DFPlayer nenalezen` → zkontroluj zapojení (hlavně napájení z 3V3!)
- Při diagnostickém sketchi: CMD `0x3F` s hodnotou `02` = DFPlayer našel SD kartu ✅
- CMD `0x48` s hodnotou počtu souborů = SD karta se čte správně ✅

### Diagnostický postup při problémech:
1. Odpoj GPIO16 a GPIO17 od DFPlayeru
2. Spusť diagnostiku – mělo by být ticho (žádná data)
3. Pokud stále přicházejí data → echo problém (vodiče propojeny)
4. Připoj zpět jen VCC (z 3V3!) a GND na DFPlayer
5. Změř napětí na DFPlayeru (pin 1 vs pin 7) → musí být ~3.3V
6. Připoj GPIO16 a GPIO17
7. Spusť diagnostiku znovu

---

## 10. WiFi přehrávač – použití

### Přihlášení:
1. Připoj ESP32 k napájení (baterie nebo programátor)
2. Na telefonu nebo PC se připoj k WiFi síti **Sorting-Hat**
3. Heslo: **mispulehat** (změnit v kódu na řádku `AP_PASSWORD`, min 8 znaků pro WPA2!)
4. Otevři prohlížeč na adrese **http://192.168.4.1**

### Ovládání:
- Klikni na skladbu v seznamu → přehraje se
- Tlačítka: ■ Stop, ‖ Pauza, ▶ Pokračovat
- Slider hlasitosti: 0–30

---

## 11. Playlist – správa názvů skladeb

### Aktuální řešení – LittleFS (flash ESP32):
Playlist je uložen přímo v ESP32, není potřeba druhá SD karta.

Formát souboru `playlist.txt`:
```
001,Slytherin
002,Gryffindor
003,Hufflepuff
004,Ravenclaw
```

Nahrání playlistu do ESP32:
1. Nainstaluj plugin **Arduino ESP32 LittleFS Data Upload**
2. Vytvoř složku `data/` vedle `.ino` souboru
3. Vlož `playlist.txt` do složky `data/`
4. Tools → ESP32 LittleFS Data Upload

### Alternativa – hardcoded v kódu:
```cpp
const char* trackNames[] = {"Slytherin", "Gryffindor", "Hufflepuff", "Ravenclaw"};
```
Jednodušší ale vyžaduje upload při každé změně.

---

## 12. Alternativní řešení – jedna SD karta pro vše

Pokud chceš číst playlist i přehrávat MP3 z jedné SD karty (bez přenosu playlistu přes USB), nejčistší řešení je:

**ESP32 + SD card reader + MAX98357A zesilovač** (bez DFPlayeru)

```
SD karta → ESP32 (čte playlist.txt + dekóduje MP3)
        → I2S → MAX98357A → reproduktor
```

Zapojení MAX98357A:
```
ESP32 GPIO25 (DOUT) → DIN
ESP32 GPIO26 (BCLK) → BCLK
ESP32 GPIO27 (LRC)  → LRC
3.3V                → VDD
GND                 → GND
```

Potřebné součástky z Laskakit:
- MAX98357 I2S mono zesilovač 3W
- LaskaKit microSD Card modul (LA161078, ~48 Kč)

Knihovna: **ESP8266Audio** (funguje i na ESP32)

Nevýhoda: MAX98357 je mono – dva reproduktory buď v sérii (tišší) nebo potřebuješ dva moduly.

---

## 13. Minimální testovací sketch

Pokud potřebuješ rychle ověřit že DFPlayer funguje:

```cpp
#include <HardwareSerial.h>
#include "DFRobotDFPlayerMini.h"

HardwareSerial dfSerial(2);
DFRobotDFPlayerMini dfPlayer;

void setup() {
  Serial.begin(115200);
  dfSerial.begin(9600, SERIAL_8N1, 16, 17);
  delay(3000);  // DFPlayer potřebuje čas na boot

  if (dfPlayer.begin(dfSerial)) {
    Serial.println("DFPlayer OK!");
    dfPlayer.volume(20);
    delay(500);
    dfPlayer.play(1);
    Serial.println("Hraje skladba 1...");
  } else {
    Serial.println("DFPlayer nenalezen!");
  }
}

void loop() {}
```

Očekávaný výstup:
```
DFPlayer OK!
Hraje skladba 1...
```

---

## 14. Shrnutí nejdůležitějších pravidel

1. ✅ Programátor vždy na **3.3V**
2. ✅ DFPlayer napájet z **3V3 pinu (pin 1)**, NE z VCC pinu (pin 15)!
3. ✅ RX a TX DFPlayeru jsou na **LEVÉ** straně modulu
4. ✅ GPIO17 → DFPlayer RX, GPIO16 ← DFPlayer TX – **přímo, BEZ odporu**
5. ✅ SD karta formát **FAT32**, soubory **001.mp3, 002.mp3...** v rootu, seřazené přes `fatsort`
6. ✅ Po formátování SD karty: `sudo umount` před vyjmutím!
7. ✅ Serial Monitor zavřít před nahráváním
8. ✅ Board v IDE: **ESP32 Dev Module**
9. ✅ Serial Monitor rychlost: **115200 baud**
10. ✅ Linux: `sudo usermod -a -G dialout $USER` (jednorázově)
11. ✅ Pokud upload selže: držet **FLASH**, kliknout Upload, při `Connecting...` pustit FLASH
12. ✅ Multimetr na **V⎓** (DC, rovná čárka s tečkami), NE na V~ (AC)!
13. ✅ Červená sonda multimetru = **+**, černá = **GND**
14. ✅ DFPlayer čeká 3 sekundy po zapnutí než začne komunikovat
15. ✅ Knihovny ESPAsyncWebServer a AsyncTCP musí být od **ESP32Async** (ne me-no-dev!)

---

## 15. Historie problémů a jejich řešení (pro reference)

| Problém | Příčina | Řešení |
|---|---|---|
| Multimetr ukazoval nesmysly | Přepínač na AC místo DC | Přepnout na V⎓ (DC) |
| Upload selhal – port nečitelný | Chybí oprávnění Linuxu | `sudo chmod 666 /dev/ttyACM0` |
| Upload selhal – No serial data | Programátor špatně zasunut | Vysunout a zasunout znovu |
| Kompilace – mbedtls_md5 error | Stará verze ESPAsyncWebServer | Nainstalovat fork od ESP32Async |
| WiFi crash – tcp_alloc assert | Stará verze AsyncTCP/ESPAsyncWebServer | Nainstalovat forky od ESP32Async |
| DFPlayer nenalezen | Napájení z VCC pinu (0V) | Přepojit na 3V3 pin |
| Echo na UART (RX = TX data) | Vodiče propojeny/příliš blízko | Zkontrolovat zapojení, rozdělit vodiče |
| Šum FF 00 FF 00 na RX | Nezapojený RX pin | Zapojit GPIO16 na DFPlayer TX |
| Odpor 1kΩ blokoval komunikaci | 3.3V systém nepotřebuje odpor | Odpor odstranit, zapojit přímo |
| Reproduktor cvakl ale nehrál | DFPlayer neměl napájení | Napájet z 3V3 místo VCC |
| readFileCountsInFolder vrací 0 | DFPlayer potřebuje delší init | Zvýšit delay nebo použít readFileCounts |
| readFolderCounts vrací -1 | Nepodporováno na MH2024K-24SS | Nepoužívat, spoléhat na readFileCounts |
| WiFi program nevidí soubory | readFileCountsInFolder(1) nespolehlivé | Použít readFileCounts() + play(N), soubory v rootu |
| play(2) hlásí "soubor nenalezen" | Zprávy z předchozích příkazů ve frontě | Vyprázdnit frontu (drainMessages) před dalším příkazem |
| readState vrací -1 za přehrávání | Nespolehlivé na MH2024K-24SS | Použít readCurrentFileNumber() pro ověření stavu |
| Reproduktor cvaká při startu | Šum na UART během bootu ESP32 | Neškodné; lze zmírnit 10kΩ pull-down |
| WiFi AP s generickým názvem a bez hesla | Heslo kratší než 8 znaků → softAP() selže tiše | Heslo min 8 znaků pro WPA2; kontrolovat návratovou hodnotu softAP() |

---

## 16. Užitečné odkazy

- **LaskaKit ESP32-LPKit GitHub:** https://github.com/LaskaKit/ESP32-LPKit
- **LaskaKit CH9102 Programmer:** https://github.com/LaskaKit/CH9102-Programmer
- **ESPAsyncWebServer (správný fork):** https://github.com/ESP32Async/ESPAsyncWebServer
- **AsyncTCP (správný fork):** https://github.com/ESP32Async/AsyncTCP
- **DFPlayer datasheet:** přiložen jako `mp3_manual_dfplayer721.pdf`
- **ESP32-WROOM-32 datasheet:** přiložen jako `esp32-wroom-32_datasheet_en.pdf`
- **ESPAsyncWebServer tcp_alloc issue:** https://github.com/me-no-dev/ESPAsyncWebServer/issues/1455
- **ESP32 tcp_alloc issue:** https://github.com/espressif/arduino-esp32/issues/10781
- **Řešení tcp_alloc (Random Nerd Tutorials):** https://rntlab.com/question/solvedassert-failed-tcp_alloc-idf-components-lwip-lwip-src-core-tcp-c1851-required-to-lock-tcpip-core-functionality/
- **Esptool troubleshooting:** https://docs.espressif.com/projects/esptool/en/latest/esp32/troubleshooting.html

---

*Průvodce vytvořen a průběžně aktualizován na základě reálného ladění projektu. Každá poznámka vychází z konkrétního problému který nastal. Poslední aktualizace: duben 2026.*
