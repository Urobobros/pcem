# Přidání podpory WHPX do PCem

Tento krátký návod shrnuje, jaké části zdrojového kódu je třeba upravit při integraci
akcelerace Windows Hypervisor Platform (WHPX). Vše je psáno stručně česky,
abylo zřejmé, kde se musíte dotknout implementace.

## 1. Nastavení build systému
* V kořenovém `CMakeLists.txt` je volba `USE_WHPX`, která se zapíná
  parametrem `-DUSE_WHPX=ON`.
* Ve `src/CMakeLists.txt` se po aktivaci přidá soubor `whpx.c`,
  definice `USE_WHPX` a na Windows se linkuje proti knihovně
  `WinHvPlatform`.

## 2. Modul WHPX
* Hlavíčko `includes/private/whpx.h` deklaruje funkce pro práci s WHPX.
* Implementace těchto funkcí je v `src/whpx.c`. Soubor řeší
  vytvoření partition, namapování paměti a běh virtuálního CPU.
* Modul je využíván jen pokud je zapnutá volba `USE_WHPX`.

## 3. Výběr backendu CPU
* V `includes/private/cpu_backend.h` je rozšířeno enum `CPUBackend`
  o hodnotu `CPU_BACKEND_WHPX`.
* `src/cpu_backend.c` podle přítomnosti WHPX inicializuje backend,
  mapuje paměť (`whpx_map_memory`) a při každém cyklu volá
  `whpx_vcpu_run`. Pokud inicializace selže, spadne zpět na
  interpret.

## 4. Úpravy subsystému paměti
* `src/memory/mem.c` a `src/memory/mem_bios.c` volají
  `whpx_map_rom` a `whpx_map_range` pro namapování BIOSu a RAM.
* Na Windows je nutné alokovat paměť jako spustitelnou, jinak
  hypervisor odmítne mapování.
* Funkce `whpx_get_ram_base()` umožňuje CPU backendu přímý přístup
  k namapované RAM.

## 5. Video a VGA paměť
* V `src/video/vid_svga.c` je při aktivním WHPX využita RAM
  jako zdroj framebufferu a oblast 0xA0000 se namapuje pomocí
  `whpx_map_range`.
* Pro ladění existují pomocné funkce `debug_dump_vga_memory()` a
  `debug_dump_vga_rom_signature()` vyvolané z `pc.c`.

## 6. Ověření funkčnosti
* V adresáři `tools` je utilita `check-whpx.c`, která ověří, zda je
  WHPX na systému dostupné a správně inicializuje jednoduchý VM.
* Při spuštění PCem poznáte úspěšnou inicializaci podle textu
  `[WHPX]` v titulku okna.

Tímto je stručně popsáno, kde se kódu musíte dotknout, pokud chcete
integraci WHPX dále rozšiřovat nebo portovat na jinou verzi systému.
