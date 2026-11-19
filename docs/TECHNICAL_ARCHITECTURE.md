# Axial — Technische Architektur

## 1. Komponentenübersicht

```
┌────────────────────────────────────────────────────────────┐
│  Axial.App  (WinUI 3, C#, .NET 8)                                   │
│  ┌──────────────┐ ┌───────────────┐ ┌────────────────────┐  │
│  │ Glass-Shell    │ │    Modul-Registry│ │    Module (Views)     │ │
│  │ Toolbar/Menü   │ │    Commands      │ │    Rechner, Plot, …   │ │
│  └──────┬───────┘ └───────┬───────┘ └─────────┬──────────┘ │
│          │                    │                     │              │
│  ┌──────┴─────────────────┴───────────────────┴──────────┐ │
│  │ Axial.MathCore.Net — C#-Wrapper (P/Invoke, JSON-ABI)           │ │
│  └──────────────────────────┬────────────────────────────┘ │
├─────────────────────────────┼──────────────────────────────┤
│  Axial.MathCore (C++17, DLL)    │                                   │
│  ┌───────────┐ ┌─────────┐ ┌──────────┐ ┌──────────────┐    │
│  │ BigInt      │ │Rational  │ │ Tokenizer  │ │ Parser (AST)   │    │
│  └───────────┘ └─────────┘ └──────────┘ └──────────────┘    │
│  ┌───────────┐ ┌─────────┐ ┌──────────┐ ┌──────────────┐    │
│  │ VM          │ │ Funktion │ │ Format     │ │ Error-Modell   │    │
│  └───────────┘ └─────────┘ └──────────┘ └──────────────┘    │
└────────────────────────────────────────────────────────────┘
```

**Regel:** Die UI kennt den Kern nur durch `Axial.MathCore.Net`. Der Kern kennt
die UI nicht. Alles Mathematische ist im Kern testbar ohne Windows-UI.

## 2. Mathe-Kern (`src/Axial.MathCore`, C++17)

### 2.1 Werte-Modell (`value.hpp`)

| Typ | Bedeutung |
|---|---|
| `BigInt` | beliebig große Ganzzahlen (Sign-Magnitude, Basis 10^9-Limbs) |
| `Rational` | exakter Bruch (BigInt Zähler/Nenner, normalisiert via gcd) |
| `Real` | IEEE-754 double (num. Modus, Transzendentes) |
| `Complex` | komplexe Zahl (Real, Real) |
| `Poly` | Polynom mit Rational-Koeffizienten (später, symbolisch) |

`Value` ist ein getaggtes Union-ähnliches struct (kein Exceptions-Pfad im
Ergebnis: Fehler sind Werte, keine Sprünge).

### 2.2 Verarbeitungskette

```
Text → Tokenizer → Parser → AST
                          ├─→ symbolischer Pfad (spätere Phasen: Ableitung, LGS…)
                          └─→ Kompiler → Bytecode → VM  (numerischer Schnellpfad)
```

- **Tokenizer:** Zahlen (dezimal, wissenschaftlich, Brüche `1/2`, `%`), Bezeichner
  (`sin`, `pi`…), Operatoren inkl. Unicode (`×`, `÷`, `−`, `√`, `²`, `³`),
  Klammern, implizite Multiplikation.
- **Parser:** Präzedenz-Parser (Pratt): `^` rechts-assoziativ, implizite
  Multiplikation (`2(3+4)`, `2π`), Fakultät, Prozent-Semantik wie ELIAN,
  automatische Klammerergänzung.
- **Kompiler:** Faltet Konstanten (`FOLD`-Instruktion), registriert Funktions-
  Calls; erzeugt lineares Bytecode-Programm (kein AST-Walk pro Auswertung).
- **VM:** Registermaschine, `double`- und exakt-`Rational`-Pfade; Ans-Variable,
  Winkelmodus (DEG/GON/RAD) als globales Flag; Millionen Auswertungen/ Sekunde.

### 2.3 Fehlermodell („unfehlbar")

- Jede öffentliche Kernfunktion liefert einen **Statuscode + strukturierte
  Fehlermeldung** (Position/Token/Ursache/Korrekturhinweis) — nie Abbruch.
- Division durch 0, Überlauf, ungültige Domain (`ln(-1)`), unbekannte Funktion,
  falsche Klammern → typisierte Fehler mit `AxialErrorKind`.
- Der Parser akzeptiert unvollständige Eingabe als **partiellen Zustand**
  (für Live-Vorschau), statt abzubrechen.

### 2.4 ABI (stabil, C-ähnlich)

```
axial_version()                              -> const char*
axial_eval(text, options_json)               -> result_json (char*, axial_free)
axial_compile(text, options)                -> handle (compiled program)
axial_run(handle, variables_json)           -> result_json
axial_free(ptr)
```

Übertragung als **JSON-Text** (eigener Serializer im Kern, keine Abhängigkeit):
UI, Tests und Benchmarks lesen dieselbe Struktur.

```json
{"ok":true,"kind":"rational","display":"1/2","value":{"num":1,"den":2}}
{"ok":false,"error":{"kind":"DivideByZero","pos":4,"token":"/","msg":"Division durch Null","hint":"Nenner prüfen"}}
```

## 3. UI (`src/Axial.App`, WinUI 3 / .NET 8 / Windows App SDK 1.6)

- **Shell:** ein Fenster; Inhalt = aktives Modul; unten die **Glass-Toolbar**
  (Mica-Fenster, Acrylic-Panels, Spekular-Kanten per Win2D/Composition).
- **Toolbar:** `[ Tool-Menü ] [ Modus-Umschalter ] [ kontextabhängige Aktionen ]`
  — Tool-Menü öffnet als Flyout/Sheet: Suche + Modulgrid + „Zuletzt".
- **Modul-Registry:** jedes Modul liefert `IModule { Id, Titel, Icon, View,
  Kommandos }`; Registrierung 1:1 — neue Module ohne Shell-Änderung.
- **Design-Tokens:** `Themes/AxialTokens.xaml` (Farben, Radien, Abstände,
  Elevation, Federn); Akzentfarben umschaltbar; Light/Dark folgt System.
- **Eingaben:** physische Tastatur (Shortcuts, navigate, Enter, Esc), Touch/Stift.

## 4. Build & CI (`.github/workflows`)

| Job | Inhalt |
|---|---|
| `core` | CMake + MSVC: `Axial.MathCore` als DLL, CTest (Unit + Golden + Fuzz) |
| `bench` | Kern-Benchmark: Durchsatz/Op; Vergleichszahl vs. ELIAN-Basis (Doku) |
| `app` | .NET 8: Restore → Build WinUI-App → MSIX-Paket → Artefakt-Upload |

Runner: `windows-latest` (MSVC 2022, .NET 8 SDK, Windows App SDK via NuGet).

## 5. Teststrategie

| Ebene | Was | Wo |
|---|---|---|
| Unit | BigInt, Rational, Tokenizer, Parser, VM, Funktionen | CTest im Kern |
| Golden | Dokumentierter Korpus bekannter Ergebnisse (aus ELIAN-Verhalten abgeleitet, **kein Code übernommen** — nur erwartete Werte) | CTest |
| Differenz | Axial vs. dokumentierte Erwartungen auf 500+ Ausdrücken | CTest |
| Fuzz | Zufalls-/Gift-Tokens: nur typisierte Fehler, kein Absturz | CTest |
| UI | Rauch-Test: App startet, Modulwechsel | App-Job (später) |

## 6. Performance-Budget

- Ausdruck parsen + kompilieren: < 50 µs (einmalig).
- VM-Auswertung einfacher Formel: < 10 ns/Op.
- Plot-Abtastung: SIMD-`Vector<double>`-Blöcke; ≥ 1 M Punkt/Sekunde.
- Zahlenformat Rational↔String: ohne moderne BigInt-Library, eigene Limbs
  (Basis 10⁹, Divid-and-Conquer nur wenn nötig).