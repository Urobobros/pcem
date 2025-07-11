# Správné načítání paměti pro WHPX

Tento dokument popisuje, jak PCem mapuje svou paměť do Windows Hypervisor Platform (WHPX). Uvádí klíčové funkce a oblasti RAM/ROM, které se při aktivním WHPX musí namapovat, aby emulace probíhala správně.

## Mapování hlavní RAM
- V `cpu_backend.c` se po inicializaci hypervizoru volá `whpx_map_memory(ram, mem_size * 1024)`. Tato funkce z `src/whpx.c`:
  - Uloží ukazatel a velikost RAM do globálních proměnných.
  - Pomocí `WHvMapGpaRange` mapuje celou RAM na GPA adresu 0 se všemi právy (READ/WRITE/EXECUTE).
  - Pokud byla RAM mapována již dříve, nejdříve se staré mapování zruší přes `WHvUnmapGpaRange`.
  - Po úspěchu se zároveň vytvoří mapování oblasti 0xA0000–0xBFFFF, pokud je RAM dostatečně velká. Této oblasti se využívá pro VGA paměť.

## ROM a ostatní oblasti
- V `src/memory/mem.c` se BIOSy mapují pomocí makra `WHPX_MAP_ROM`. To volá `whpx_map_rom` a předá adresu v hostitelské paměti a cílovou GPA.
- Soubor `src/flash/rom.c` při načítání rozšiřujících ROM využije `whpx_map_range`, aby se buffer ROMu zkopíroval do RAM a zároveň se hypervisoru zpřístupnil daný rozsah.
- VGA ovladač (`src/video/vid_svga.c`) při aktivním WHPX nepoužívá vlastní vyhrazené VRAM – ukazatel framebufferu se nastaví na `ram + 0xA0000` a funkce `whpx_map_range` zaručí, že tento rozsah hypervisor vidí.

## Kontrola mapování
- Stav namapované paměti lze ověřit pomocí nástrojů v adresáři `tools`. Soubor `check-whpx.c` zkusí vytvořit partition, namapovat malou paměť a spustit jednoduchý kód.
- Při běhu PCem se na konzoli (`pclog`) zapisují hlášky o volání `whpx_map_*`; lze tak sledovat, zda všechny kroky proběhly.

## Co musí být v hypervizorové paměti
1. **Celá RAM** emulovaného stroje od adresy 0. Bez ní by CPU nemohlo číst ani zapisovat data.
2. **BIOS** a případné rozšiřující ROMy – mapují se jen pro čtení a vykonávání.
3. **VGA okno 0xA0000–0xBFFFF** – slouží jako framebuffer a pro některé grafické režimy se často zapisuje přímo.
4. **Další speciální oblasti** (např. paměť sítových karet) se při jejich inicializaci mohou mapovat přes `whpx_map_range` podobně jako ROM.

Správné namapování všech těchto částí je nezbytné, aby WHPX backend fungoval korektně a emulace byla stabilní.
