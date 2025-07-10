# Šablona logu pro inicializaci PC s WHPX

Tento krátký dokument navrhuje strukturu logovacích hlášek, které mohou pomoci sledovat kroky při spouštění emulovaného PC s backendem WHPX. Poslouží jako vodítko, co a kdy vypisovat do `pclog`, aby bylo zřetelné, že se jednotlivé části inicializace provedly správně.

## Příklad pořadí hlášek
1. **[BOOT]** Začíná inicializace PC.
2. **[MEM]** Mapování RAM do WHPX (velikost se vypíše v MB).
3. **[ROM]** Načítám systémový BIOS z adresáře `roms`.
4. **[ROM]** Mapuji BIOS na adresu `0xF0000` pomocí `whpx_map_rom`.
5. **[VGA]** Mapuji oblast `0xA0000`–`0xBFFFF` pro VGA framebuffer.
6. **[CHECK]** Ověření, že se BIOS a VGA oblast opravdu nachází v paměti WHPX.
7. **[CPU]** Spouštím virtuální CPU (`whpx_vcpu_run`).
8. **[BOOT]** Řízení přebírá BIOS, pokračuje start systému.

## Vzor použití v kódu
Níže je ukázka, jak mohou být tyto hlášky zapsány. Při implementaci je vhodné je umístit do relevantních funkcí inicializace.

```c
pclog("[BOOT] Začíná inicializace PC\n");
...
pclog("[MEM] Mapování RAM do WHPX: %u MB\n", mem_size / 1024);
...
pclog("[ROM] Mapuji BIOS na 0xF0000\n");
...
pclog("[VGA] Mapuji VGA okno 0xA0000-0xBFFFF\n");
...
pclog("[CHECK] Ověření mapování BIOSu a VGA\n");
...
pclog("[CPU] Spouštím virtuální CPU\n");
```

Tato šablona slouží pouze jako inspirace. Konkrétní hlášky si můžete upravit podle vlastních potřeb a rozmístit je tam, kde má význam sledovat průběh inicializace.
