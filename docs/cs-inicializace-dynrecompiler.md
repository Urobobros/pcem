# Inicializace emulovaného PC s dynamickou rekompilací

Tento dokument stručně popisuje, co se v PCem děje při spuštění emulovaného počítače, pokud je zapnutá dynamická rekompilace procesoru.

## 1. Spuštění PCem
Po startu aplikace se načte zvolená konfigurace stroje. Emulator připraví paměť, inicializuje jednotlivá zařízení a zvolí backend pro CPU. Pokud je dostupná dynamická rekompilace, nastaví se jako aktivní.

## 2. Načtení BIOSu
PCem načte obsah BIOSu z adresáře `roms` do paměti emulovaného systému. Následně se procesor resetuje do výchozího stavu a začne vykonávat kód BIOSu na adrese `0xFFFF0` (segment `F000:FFF0`).

## 3. Průběh BIOSu
BIOS inicializuje základní hardware – například časovač, řadiče přerušení, grafickou kartu a vstupně/výstupní porty. Provádí také kontrolu paměti a vyhledá zaváděcí zařízení.

## 4. Dynamická rekompilace CPU
Při běhu kódu CPU předává instrukce dynamickému překladači. Ten převádí bloky x86 instrukcí na instrukce hostitelského procesoru a výsledek ukládá do cache. Díky tomu je emulace výrazně rychlejší než při čisté interpretaci. Pokud se narazí na instrukci, kterou recompiler nepodporuje, provede se její emulace klasickou interpretací.

## 5. Předání řízení z BIOSu
Po dokončení inicializace BIOS předá řízení zaváděcímu záznamu (například z disku nebo diskety). Odtud již běží operační systém a emulace pokračuje stejným způsobem – dynamická rekompilace stále překládá nové bloky kódu a staré využívá z cache.

Tím je základní proces spuštění emulovaného počítače ukončen. Další běh systému již závisí na konkrétním operačním systému a aplikacích.

