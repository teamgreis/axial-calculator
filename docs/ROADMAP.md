# Axial — Roadmap

Stand: Baustein 0 (Fundament). Jede Phase endet mit einem grünen CI-Lauf.

## Phase 0 — Fundament ✅ (in Arbeit)
- [x] Repo `teamgreis/axial-calculator` Struktur, .gitignore, Lizenz-Entwurf
- [x] MASTER_PLAN, TECHNICAL_ARCHITECTURE, ROADMAP, README
- [ ] Initialer Push + CI-Gerüst (core-Build läuft)
- [ ] Golden-Korpus generieren (ELIAN lokal vermessen, Erwartungswerte committed)

## Phase 1 — Mathe-Kern (C++) ✅ (Build/Tests grün in CI)
- [x] BigInt (Sign-Magnitude, Limbs, gcd, pow, toString/fromString)
- [x] Rational (normalisiert, Arithmetik, Format Pretty-Print)
- [x] Tokenizer inkl. Unicode, Brüche, Prozent, implizite Multiplikation
- [x] Pratt-Parser: Präzedenz, rechts-assoziatives `^`, Fakultät, Auto-Klammern
- [x] Wertemodell (Rational/Integer/Real/Complex), Winkelmodi, `ans`
- [x] Bytecode-Kompiler + Stack-VM (exakt/numerisch doppelter Pfad)
- [x] Funktionenbibliothek (Trig/Hyper/Log/Rundung/Kombinatorik, gcd/lcm/ncr/npr, min/max/sum/mean)
- [x] Konstanten (pi, e, tau, phi)
- [x] Fehlermodell typisiert (Position, Token, Ursache, Hinweis)
- [x] C-ABI + JSON-Serializer
- [x] Testsuite: Unit + Golden-Parität + Fuzz; Benchmark-Harness (742 Prüfungen grün)
- [ ] **Leistungs-Baustein** (neu): BigInt-SBO + AST-Arena → Zielkorridor 100–400× kompiliert (docs/BENCHMARKS.md)

## Phase 2 — Schale (WinUI 3)
- [ ] Projektgerüst, Windows App SDK 1.6, Paketierung MSIX
- [ ] Glass-Shell: Mica-Fenster, Acrylic-Panels, Spekular-Kanten (Win2D)
- [ ] Dynamische Bottom-Toolbar + Tool-Menü (Modulgrid, Suche, Zuletzt)
- [ ] Modul-Registry + IModule-Schnittstelle
- [ ] C#-Wrapper: P/Invoke, Fehlermapping, Haupt-Thread-Regeln
- [ ] Rechner-Modul: algebraisch + RPN, Verlauf, Favoriten, Speicher, ans
- [ ] Tastatursteuerung (Enter/Esc/Ziffern/Operationen/Strg+K)
- [ ] CI: MSIX-Artefakt

## Phase 3 — Modul-Rollout (je Modul: Kern → Registrierung → View → Tests → grün)
- [ ] Wissenschaftlich (Funktionsliste, Winkelmodus-Umschalter)
- [ ] Algebra-Werkstatt (Polynome, Nullstellen exakt, Faktorisierung, Kurvendiskussion)
- [ ] Gleichungssysteme (exakte Gauß-Elimination, Schritt-Anzeige)
- [ ] Zahlentheorie (Miller-Rabin, Faktorisierung, Teiler, CRT, mod. Inverse)
- [ ] Boolesche Algebra (Wahrheitstafeln, Quine–McCluskey)
- [ ] Interpolation (Lagrange exakt, Spline, Fit mit R²)
- [ ] Komplexe Zahlen (Polardarstellung, Wurzeln)
- [ ] 2D-Plotter (10 Objekttypen, Zoom, PNG/SVG-Export)
- [ ] Statistik & Verteilungen (Kennwerte, Quartile, Φ/Quantile, Hypothesentests)
- [ ] Matrizen (Raster, Determinante, Inverse, Eigenwerte)
- [ ] Regression & Korrelation
- [ ] Analysis (Grenzwerte, Taylor, Newton, partielle Ableitungen, Stammfunktionen)
- [ ] Differentialgleichungen (exakt + Runge-Kutta)
- [ ] Vektoren 2D/3D (Kreuzprodukt, Winkel, Parameterdarstellung)
- [ ] 3D-Plotter (Painter-Algorithmus, Gradient/Tangentialebene)
- [ ] Einheiten (10 Kategorien) + erweiterte Umrechnung
- [ ] Programmierer-Modus (Bit-Toggle, Basen 2/8/10/16)
- [ ] Finanzen (Tilgungsplan)
- [ ] Datum & Zeit
- [ ] Formelbibliothek, Konstanten, Übungsmodus, Notizen
- [ ] Spracheingabe mit Absichtserkennung („nullstellen von x² minus 1")

## Phase 4 — Finish
- [ ] Einstellungen (Akzent, Winkel, Schriftgröße, Export/Import)
- [ ] Update-Prüfung mit SHA-256 (opt-in)
- [ ] Barrierefreiheit (Kontraste, Screenreader, Tastatur vollständig)
- [ ] Performance-Doku (erreichte Faktoren, Kurven)
- [ ] Release-Build (MSIX signiert), CHANGELOG, DATENSCHUTZ