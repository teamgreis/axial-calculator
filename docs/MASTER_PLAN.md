# Axial — Master-Plan

> **Axial** ist der komplette Neuentwurf eines wissenschaftlichen Taschenrechners
> und Computer-Algebra-Systems als **native Windows-Anwendung** (WinUI 3).
> Kein Zeilencode des Vorgängers **ELIAN** wird übernommen — neuer Mathe-Kern,
> neue Shell, neues Design. Der Funktionsumfang von ELIAN (25+ Module) kehrt
> zurück, in jedem Bereich deutlich verbessert.

**Projekt:** `teamgreis/axial-calculator` · **Arbeitsname:** Axial · **Sprache:** Deutsch/Englisch · **Plattform:** Windows 10/11 x64 (ARM64 später)

---

## 1. Vision

Ein Taschenrechner, der sich anfühlt wie ein physisches Präzisionsinstrument:
 Glas, Licht, Tiefe — und darunter ein Mathe-Kern, der exakt, schnell und
 unbestechlich ist. Axial verbindet drei Dinge, die vorher getrennt waren:

| Versprechen | Bedeutung |
|---|---|
| **Exakt** | Rationale Arithmetik mit BigInt — `1/3 + 1/6 = 1/2`, nie Float-Rauschen; symbolische Algebra auf demselben Kern |
| **Schnell** | Kompilierte Ausdrücke (Bytecode-VM), nativ — **Stand: 6,5–41× schneller als ELIAN** (kompilierter Pfad, selbe Maschine, siehe BENCHMARKS.md); Ziel 400× bleibt offen, nächster Hebel: Parse/Compile-Overhead |
| **Unfehlbar** | Jeder Eingabezustand hat ein definiertes Ergebnis; kein Absturz, keine undefinierte Anzeige; Differenz- und Regressionstests gegen einen dokumentierten Golden-Korpus |

## 2. Warum Neuentwicklung statt Rewrite des alten Projekts?

- **Technologiewechsel:** HTML/JS-Single-File → native WinUI-3-App. Glas, Fenster,
  Teilen, Dateizugriff, Prozessorleistung sind nur nativ echt.
- **Kern neu denken:** Der alte Kern parste bei *jeder* Auswertung neu. Axial
  kompiliert **einmal** und wertet millionenfach aus (VM).
- **Neue Architektur:** C++-Rechenkern als eigene Komponente mit stabilem ABI;
  C#-UI spricht ihn per P/Invoke an. Der Kern ist ohne UI testbar.
- **Sauberer Tisch:** Keine Migration von Altlasten — alles entsteht frisch,
  dokumentiert und getestet.

## 3. Design-Sprache „Realistisches Liquid Glass"

Kein flaches Web-Flasächendesign, sondern gestapelte Glas-Ebenen wie auf
physischen Geräten:

- **Echte Transparenz:** Backdrop-Materialien (Mica für Fensterflächen, Acrylic
  für schwebende Panels) + eigener Spekular-Kanten-/Glanz-Shader, sodass Glas
  wirklich lichtbricht und der Fensterinhalt durchscheint.
- **Dynamische Bottom-Toolbar:** Die Werkzeugleiste sitzt **unten** — sie ist
  die zentrale Navigation: kontextabhängige Tool-Taste öffnet das **Tool-Menü**
  (alle Module, Suche, zuletzt genutzt). Sie schrumpft/erweitert sich mit dem
  Modus, klebt nie über Inhalten.
- **Feder-Physik:** Jede Interaktion (Tasten, Menüs, Sheets) folgt
  Federsimulation statt linearer CSS-Kurven; `prefers-reduced-motion` respektiert.
- **Konsistenz:** Ein Token-System (Farben, Radien, Abstände, Schriften,
  Elevation) definiert alles an einer Stelle.

## 4. Funktions-Universe (Rückkehr aller ELIAN-Module, verbessert)

| Bereich | Module (Zielzustand) |
|---|---|
| **Rechnen** | Rechner (algebraisch + RPN + wissenschaftlich + Programmierer), exakte Brüche, Speicher/ans, Verlauf, Favoriten, Spracheingabe, Live-Vorschau |
| **Exakte Mathematik** | Algebra-Werkstatt (Polynome, Nullstellen, Faktorisierung, Kurvendiskussion), Gleichungssysteme (exakte Gauß-Elimination), Zahlentheorie (Miller-Rabin, Faktorisierung, CRT), Boolesche Algebra (Wahrheitstafel, Quine–McCluskey), Interpolation (Lagrange exakt, Spline, Fit) |
| **Analysis** | Grenzwerte, Tangenten/Normalen, Newton-Iteration, Taylor-Reihen, symbolische + partielle Ableitungen, Stammfunktionen, Differentialgleichungen |
| **Komplexe Zahlen** | Rechnen, Polardarstellung, Wurzeln exakt (inkl. konjugiert-komplexe Paare) |
| **Darstellung** | 2D-Plotter (10 Objekttypen), 3D-Plotter (Gradient/Tangentialebene), PNG/SVG-Export |
| **Daten** | Statistik (Kennwerte, Quartile), Hypothesentests, Matrizen (inkl. Raster, Eigenwerte, Potenzen), Regression & Korrelation, Verteilungen |
| **Umwandeln** | Einheiten (10 Kategorien), erweiterte Umrechnung, Programmierer-Modus (Bit-Toggle) |
| **Finanzen** | Tilgungsplan mit Zins-/Gesamtbelastung |
| **Werkzeuge** | Datum & Zeit, Formelbibliothek, Konstanten, Übungsmodus, Notizen |
| **System** | Einstellungen (Akzent, Winkel, Schriftgröße, Export/Import), Update-Prüfung (SHA-256, opt-in) |

Jedes Modul registriert sich 1:1 in der **Modul-Registry** — die Shell kennt
keine Module, nur die Registry. Neue Module = Datensatz + Titel + Icon.

## 5. Qualitätsziele (messbar)

| Ziel | Kennzahl | Nachweis |
|---|---|---|
| **Leistung** | ≥ 400× Auswertungsdurchsatz (kompiliert) vs. ELIAN-Engine | Benchmark in CI, veröffentlichte Zahlen |
| **Korrektheit** | 100 % grüne Kern-Tests; Golden-Korpus ≥ 500 Fälle | CTest-Differenztests |
| **Robustheit** | Kein Absturz bei beliebiger Eingabe | Fuzz-Suite (zufällige/giftige Tokens) in CI |
| **Fehlerkultur** | Jeder Fehler hat Position + Ursache + Korrekturvorschlag | Parser-Fehlerbehandlung Test |

## 6. Entwicklungsmodell (Rahmenbedingungen der Maschine)

Der Entwicklungsrechner hat **kein .NET-SDK, keinen C++-Compiler, ~1,9 GB
freien Plattenspeicher und 4 GB RAM**. Lokale Builds sind daher nicht möglich.

**Konsequenz — Source lokal, Bauen in der Cloud:**

1. Code entsteht lokal im Repo (`D:\dev\axial-calculator`).
2. Jeder Push löst **GitHub Actions** aus (windows-latest: MSVC + .NET 8 SDK).
3. CI baut Mathe-Kern (CMake), führt Tests + Fuzz + Benchmarks aus, baut danach
   die WinUI-App und veröffentlicht das **MSIX-Paket als Artefakt**.
4. Grün = Testbar. Der Anwender lädt das MSIX vom Workflow-Run und installiert.

Das hält den ohnehin vollen Systemspeicher frei und macht jeden Commit
nachvollziehbar getestet.

## 7. Phasen (Kurzfassung → Details: `ROADMAP.md`)

- **Phase 0 Fundament:** Repo, Docs, CI-Gerüst.
- **Phase 1 Mathe-Kern:** C++-Kern (BigInt, Rational, Parser, Bytecode-VM,
  Funktionen, Fehlermodell), Testsuite + Golden-Korpus, Benchmark vs. ELIAN.
- **Phase 2 Schale:** WinUI-3-Shell mit Liquid-Glass-Toolbar, Modul-Registry,
  C#-Wrapper, Rechner-Modul (algebraisch/RPN/Verlauf/Favoriten).
- **Phase 3 Modul-Rollout:** Modul für Modul aus Abschnitt 4, jeweils mit Tests.
- **Phase 4 Finish:** Einstellungen, Updates, Barrierefreiheit, Release-Build.

## 8. Offene Entscheidungen (mit Nutzer zu klären)

- Endgültige Lizenz (provisorisch proprietär wie ELIAN).
- Veröffentlichungsstrategie (öffentliches Repo vs. privat).
- Update-Mechanik (GitHub-Releases) in nativer Form.