# WHPX FAR plán

Tento dokument shrnuje kroky, které je potřeba provést, aby backend WHPX
správně mapoval paměť a umožnil spuštění BIOSu po resetu.

## Úkoly

- [x] Ověřit, že ROM segmenty (0xC0000–0xFFFFF a případně FFFF0000h) jsou
  mapovány přes `WHvMapGpaRange` s příznaky READ/WRITE/EXECUTE podle potřeby a
  že první bajty odpovídají očekávanému BIOSu (např. `EA 5B E0 00`).
- [x] Dopsat logiku do `i430vx_write()` a `pam_update()`, aby změny PAMx
  registrů okamžitě přemapovaly GPA na RAM nebo ROM i při použití WHPX.
- [x] Zajistit, aby při `resetx86()` a `init_real_mode_registers()` byl segment
  `F000:FFF0` platně namapován a procesor ho mohl číst.
- [ ] Zkontrolovat, že BIOS nepřepisuje sám sebe, pokud je RAM odpojena
  (například když je `PAM0` nastaveno na `0x00`).
- [x] Implementovat přemapování (`WHvMapGpaRange`) při každé změně PAM
  registrů tak, aby se správně přepínalo mezi ROM a RAM.
- [ ] Při volání `WHvRunVirtualProcessor()` logovat GPA, hodnotu CS:IP,
  informaci o použité oblasti (RAM/ROM) a `exit_reason`.
- [ ] Před spuštěním CPU vždy namapovat celou RAM od GPA 0x000000 po její
  velikost.
- [ ] Ověřit, že se během běhu s WHPX nepouští jit/dynarec backend a že mezi
  režimy nedochází ke konfliktu.

Tento plán budeme postupně naplňovat a odškrtávat po implementaci a otestování
jednotlivých bodů.
