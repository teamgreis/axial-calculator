# Axial Calculator

**Axial** ist ein wissenschaftlicher Taschenrechner, Funktionsplotter und
Computer-Algebra-System als **native Windows-Anwendung** (WinUI 3) — die
komplette Neuentwicklung des ELIAN Ultimate Calculator. Kein Code des
Vorgängers wird übernommen.

> Status: **Baustein 0 (Fundament)** — Mathe-Kern in Arbeit.
> Aktuell lauffähige Builds entstehen ausschließlich über GitHub Actions
> (Entwicklungsrechner ohne SDK/Compiler). Download des MSIX: letzter
> Workflow-Run → Artefakte.

## Was Axial können wird (Kurzfassung)

- **Exakt rechnen:** Brüche mit BigInt — `1/3 + 1/6 = 1/2`; symbolische Algebra,
  exakte Nullstellen, Kurvendiskussion, Zahlentheorie, Boolesche Algebra.
- **Schnell:** Ausdrücke werden einmal kompiliert (Bytecode-VM) und millionenfach
  ausgewertet; Ziel ≥ 400× Leistung gegenüber dem JS-Kern des Vorgängers —
  gemessen in der CI, dokumentiert in `docs/BENCHMARKS.md`.
- **Realistisches Liquid Glass:** echte Backdrop-Materialien (Mica/Acrylic),
  Spekular-Kanten, Feder-Physik, dynamische Bottom-Toolbar mit Tool-Menü.
- **Unfehlbar:** typisierte Fehler mit Position und Korrekturhinweis statt
  Absturz oder stillschweigendem Unsinn; Fuzz- und Golden-Tests in der CI.
- **Alle Module des Vorgängers**, verbessert: Rechner (algebraisch + RPN),
  Algebra, Gleichungssysteme, Zahlentheorie, Boolesche Algebra, Interpolation,
  komplexe Zahlen, 2D/3D-Plotter, Statistik, Matrizen, Regression, Analysis,
  Einheiten, Programmierer, Finanzen, Datum, Formelbibliothek, Notizen …

## Aufbau des Repositorys

```
src/Axial.MathCore/     C++-Rechenkern (DLL): BigInt, Rational, Parser, VM
src/Axial.MathCore.Net/ C#-Wrapper (P/Invoke + JSON-ABI)
src/Axial.App/          WinUI-3-Anwendung (Shell, Toolbar, Module)
docs/                   MASTER_PLAN, TECHNICAL_ARCHITECTURE, ROADMAP
.github/workflows/       CI: Kern, Tests, Fuzz, Benchmark, MSIX-Build
```

Dokumentation: [`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md) ·
[`docs/TECHNICAL_ARCHITECTURE.md`](docs/TECHNICAL_ARCHITECTURE.md) ·
[`docs/ROADMAP.md`](docs/ROADMAP.md)

## Lizenz

Proprietär (provisorischer Entwurf, siehe [`LICENSE`](LICENSE)) — Nutzung
unveränderter offizieller Builds für private, nicht-kommerzielle Zwecke.