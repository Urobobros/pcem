# Plán sjednocení logovacích výstupů

Níže uvedený seznam shrnuje kroky nutné k dosažení shodných logů mezi WHPX a dynamickou rekompilací.
Jednotlivé položky je možné postupně odškrtávat.

## Fáze 1 – sjednocení základních výpisů

- [x] **Revize aktuálních výpisů**
  - [x] Prostudovat kód (WHPX i recompiler) a ověřit, kde se generují výstupy registrových hodnot, PAM zápisů a PC.
  - [x] Doplnit modul/rozhraní společné pro obě varianty, aby se logovalo stejným způsobem.
- [x] **Implementace výpisu segmentu CS**
  - [x] Přidat do struktury pro logování registrů položky `CS: base`, `limit` a `attr`.
  - [x] Napojit na obsluhu změn segmentových registrů.
- [x] **Paměťová adresa přístupu (GPA)**
  - [x] Při každém hlášeném zápisu ukládat a tisknout fyzickou adresu.
  - [x] Ošetřit shodné chování v backendu WHPX i recompileru.

## Fáze 2 – detailnější informace o běhu

- [x] **Dekódování instrukce při zápisu**
  - [x] Použít (nebo doplnit) dekodér instrukcí k identifikaci instrukce způsobující zápis.
  - [x] Logovat mnemotechnický název a parametry instrukce.
- [x] **Shadow RAM & změny CR0/CR3/CR4**
  - [x] Zaznamenat staré a nové hodnoty při každé změně těchto registrů a při přemapování Shadow RAMu.
  - [x] Stejně implementovat ve WHPX i dynamické rekompilaci.
- [x] **Režim (real vs. protected)**
  - [x] Podle stavu CR0 a segmentových registrů evidovat aktuální režim CPU.
  - [x] Výpis doplňovat při každé změně.

## Fáze 3 – sledování obsahu paměti

- [x] **Detekce výstupu z paměti**
  - [x] Ošetřit přístup mimo mapovanou oblast a opuštění chráněné zóny (např. BIOS).
- [x] **Výpis paměti při přemapování**
  - [x] Při remapování v Shadow RAMu zapisovat danou oblast (typicky ROM) do logu nebo dump souboru.
  - [x] Umožnit srovnání obsahu před a po změně.
- [x] **Kontrola obsahu BIOS paměti**
  - [x] Porovnávat BIOS (ROM) s referenční kopií při startu a po každém přemapování.
  - [x] Vypisovat rozdíly pro odhalení nečekaných úprav.

## Fáze 4 – sjednocený formát výstupu

- [x] **Definice společného formátu logu**
  - [x] Navrhnout jednotnou strukturu (text/JSON) pro všechny výpisy.
  - [x] Implementovat adaptér v kódu WHPX i dynamické rekompilace.
- [x] **Porovnávací nástroje**
  - [x] Vytvořit skript, který seřadí a porovná logy z obou režimů a zvýrazní rozdíly.

## Výsledný stav

Díky výše uvedeným krokům bude možné generovat shodné výpisy registrů, paměťových operací i dalších klíčových informací v obou backendech. S jednotným formátem logů lze přesně sledovat, ve kterém okamžiku se chování odlišuje, a podle toho ladit další diagnostiku.
