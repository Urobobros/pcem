# Ladicí logy dynamické rekompilace

Tento soubor stručně popisuje nové logovací hlášky, které byly přidány do PCem pro spuštění bez WHPX. Hlášky pomohou odhalit, ve které fázi inicializace může docházet k problému.

## Přehled hlášek

- **WHPX support not compiled; using dynamic recompiler backend** – Emulátor nebyl přeložen s podporou WHPX a automaticky zvolí dynamickou rekompilaci.
- **[CPU] Dynamic recompiler enabled/disabled** – Informuje, zda je překladač zapnut podle nastavení v konfiguraci.
- **[MEM] Allocating XXX KB of RAM** – Probíhá alokace hlavní paměti emulovaného PC.
- **[BOOT] Starting PC initialization** – Hned na počátku funkce `initpc`.
- **[ROM] Loading system BIOS** – Načítání BIOSu před jeho namapováním do paměti.
- **[ROM] Mapping BIOS at 0xF0000** – BIOS je dostupný v paměti emulovaného stroje.
- **[VGA] Mapping VGA window 0xA0000-0xBFFFF** – Zviditelnění oblasti framebufferu.
- **[CHECK] VGA BIOS signature valid** – Potvrzení, že VGA BIOS je správně načten.
- **[CPU] Starting virtual CPU** – První spuštění hlavního emulačního cyklu.
- **Using dynamic recompiler backend** – Zobrazuje se i v případě, že inicializace WHPX selže a PCem spustí klasickou rekompilaci.

Logy se zapisují pomocí funkce `pclog()` a jsou viditelné pouze v debug buildu. Spuštění v terminálu tak poskytne detailní výpis kroků při startu emulovaného stroje.
