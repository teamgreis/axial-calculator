# Enhancement: Solver-Architektur für lineare Gleichungssysteme

**Datum:** 2026-11-27  
**Status:** Geplant  
**Zugehöriges Issue:** [#3 – Solver-Architektur](https://github.com/teamgreis/axial-calculator/issues/3)

## Ziel

Axial erhält eine erweiterbare, nachvollziehbare Solver-Architektur für lineare Gleichungssysteme. Für 2×2-Systeme werden Einsetzungs-, Gleichsetzungs- und Additionsverfahren angeboten. Axial empfiehlt deterministisch ein geeignetes Verfahren, lässt aber die manuelle Auswahl aller drei Verfahren zu. Jede eindeutige Lösung wird unabhängig geprüft.

Die Mathematik bleibt in `src/Axial.MathCore` (C++17). Die WinUI-Anwendung kommuniziert ausschließlich über `src/Axial.MathCore.Net` und die bestehende C-ABI/JSON-Schnittstelle mit dem Kern.

## Produktanforderungen

### 1. Drei nachvollziehbare Lösungsverfahren

- **Einsetzungsverfahren:** Eine Gleichung nach einer Variablen umstellen, in die andere einsetzen, lösen und rückeinsetzen.
- **Gleichsetzungsverfahren:** Beide Gleichungen nach derselben Variablen auflösen, gleichsetzen, lösen und rückeinsetzen.
- **Additionsverfahren:** Gleichungen bei Bedarf skalieren, addieren/subtrahieren, eine Variable eliminieren und rückeinsetzen.

Jedes Verfahren liefert strukturierte Rechenschritte. Die UI stellt diese Schritte dar und führt selbst keine mathematischen Berechnungen aus.

### 2. Automatische Verfahrenswahl

Der Method Selector bewertet unter anderem:

- bereits isolierte Variablen,
- notwendige Umformungen und Multiplikationen,
- Koeffizientenverhältnisse,
- mögliches Bruchwachstum,
- numerische Stabilität.

Die Auswahl ist deterministisch. Axial liefert zusätzlich eine verständliche Begründung. Bei Gleichstand gilt eine stabile Tie-Break-Regel. Die automatische Empfehlung ersetzt niemals die manuelle Verfahrensauswahl.

### 3. Korrektheit und Lösungsstatus

Jede eindeutige Lösung wird unabhängig in die ursprünglichen Gleichungen eingesetzt.

Unterstützte Ergebniszustände:

- **Eindeutige Lösung**
- **Keine Lösung** (widersprüchliches System)
- **Unendlich viele Lösungen** (abhängiges/unterbestimmtes System)
- **Numerisch unsicher / nicht zuverlässig klassifizierbar**

Exakte rationale Ergebnisse werden exakt geprüft. Numerische Ergebnisse werden anhand skalierter Residuen und einer dokumentierten Toleranz bewertet. Nahezu singuläre Systeme dürfen nicht mit falscher Sicherheit klassifiziert werden.

### 4. Ergebnisqualität statt Prozent-Wahrscheinlichkeit

Für deterministische mathematische Ergebnisse wird keine unbegründete Erfolgswahrscheinlichkeit in Prozent angezeigt.

Stattdessen kennzeichnet Axial:

- **Exakt:** rationale/symbolische Lösung, exakt verifiziert.
- **Numerisch:** Näherung mit ausgewiesener Genauigkeit bzw. Residuum.
- **Nicht verifiziert:** Ergebnis konnte nicht zuverlässig bestätigt werden.

Alternative Lösungen werden nur angezeigt, wenn die Mathematik der jeweiligen Funktion sie tatsächlich zulässt. Jeder Kandidat muss unabhängig validiert werden. Bei unendlich vielen Lösungen eines linearen Systems wird eine Parametrisierung ausgegeben – keine willkürlich erzeugten Lösungspaare.

## Zielarchitektur

```text
Axial.App (WinUI 3)
  └─ Axial.MathCore.Net (C# DTOs / Wrapper)
       └─ stabile C-ABI + JSON
            └─ Axial.MathCore (C++17)
                 ├─ LinearSystem / Normalisierung
                 ├─ Solver API + gemeinsames Ergebnis-Modell
                 ├─ SubstitutionSolver
                 ├─ EquatingSolver
                 ├─ AdditionSolver
                 ├─ GaussianSolver (n×n, separat erweiterbar)
                 ├─ MethodSelector
                 └─ SolutionVerifier
```

Vorgeschlagene Quelldateien; bei der Umsetzung gelten die vorhandenen Repository-Konventionen:

- `include/axial/linear_system.hpp` und `src/linear_system.cpp`
- `include/axial/solver.hpp` und `src/solver.cpp`
- `src/solvers/substitution_solver.cpp`
- `src/solvers/equating_solver.cpp`
- `src/solvers/addition_solver.cpp`
- `src/solvers/gaussian_solver.cpp`
- `src/method_selector.cpp`
- `src/solution_verifier.cpp`

Die drei Schulverfahren werden zunächst für 2×2-Systeme implementiert. Größere Systeme werden über Gauß-Elimination behandelt und nicht künstlich auf die drei 2×2-Verfahren abgebildet.

## Exakter Implementierungsweg

### Phase 1 – Datenmodell und Normalisierung

- [ ] Eingabeformat für zwei lineare Gleichungen mit zwei Unbekannten definieren.
- [ ] Gleichungen in die Normalform \(ax + by = c\) überführen.
- [ ] Exakte Koeffizienten als vorhandenen `Rational`-Typ speichern.
- [ ] Numerische Eingaben explizit vom exakten Modus unterscheiden.
- [ ] Ungültige, nichtlineare und unvollständige Eingaben typisiert zurückweisen.
- [ ] Modell für Variablen, System, Status und Rechenschritte definieren.

**Abnahme:** Äquivalente Schreibweisen werden korrekt normalisiert; ungültige Eingaben führen zu typisierten Fehlern.

### Phase 2 – Gemeinsame Solver-API

- [ ] Gemeinsame Solver-Schnittstelle (`CanSolve`/`Solve` oder äquivalent) erstellen.
- [ ] Einheitliches Ergebnisobjekt mit Status, Lösung/Lösungsmenge, Methode, Schritten, Exaktheit und Verifikationsdaten definieren.
- [ ] Schritte maschinenlesbar modellieren: Operation, betroffene Gleichungen, Vorher/Nachher und Erklärung.
- [ ] Fehler- und Abbruchzustände ohne Prozessabsturz abbilden.

**Abnahme:** Alle Solver liefern dasselbe Ergebnisformat.

### Phase 3 – Drei Solver

- [ ] Einsetzungsverfahren implementieren.
- [ ] Gleichsetzungsverfahren implementieren.
- [ ] Additionsverfahren inklusive geeigneter Skalierung implementieren.
- [ ] Vollständige Umformungsschritte protokollieren.
- [ ] Nullkoeffizienten, isolierte Variablen, Brüche, negative Werte, identische und widersprüchliche Gleichungen behandeln.

**Abnahme:** Alle drei Verfahren lösen dieselben Referenzfälle korrekt und liefern vollständige Rechenwege.

### Phase 4 – Method Selector

- [ ] Umformungsaufwand, Koeffizientenstruktur, Bruchwachstum und Stabilität bewerten.
- [ ] Deterministisches Ranking implementieren.
- [ ] Empfehlung samt maschinenlesbarer Begründung zurückgeben.
- [ ] Manuelle Auswahl aller drei Verfahren erhalten.
- [ ] Stabile Tie-Break-Regel definieren.

**Abnahme:** Gleiche Eingaben ergeben dieselbe Empfehlung samt nachvollziehbarer Begründung.

### Phase 5 – Verifikation und Qualitätsmodell

- [ ] Eindeutige Lösungen in Originalgleichungen rückeinsetzen.
- [ ] Exakte Rational-Ergebnisse exakt prüfen.
- [ ] Numerische Residuen skaliert berechnen und Toleranz dokumentieren.
- [ ] Verifikationsstatus und numerische Unsicherheit zurückgeben.
- [ ] Ungültige Kandidaten verwerfen.
- [ ] Keine unbegründeten Prozent-Wahrscheinlichkeiten anzeigen.
- [ ] Unendlich viele Lösungen parametrisch darstellen.

**Abnahme:** Kein Ergebnis wird als verifiziert angezeigt, bevor der unabhängige Prüfer erfolgreich war.

### Phase 6 – C-ABI, JSON und C#-Wrapper

- [ ] Solver-Funktionen über die bestehende C-ABI exponieren.
- [ ] JSON-Eingabe und -Ergebnisformat dokumentieren.
- [ ] Speicherfreigabe und Fehlerbehandlung gemäß bestehender ABI-Regeln umsetzen.
- [ ] DTOs und Wrapper in `Axial.MathCore.Net` ergänzen.
- [ ] Golden-Tests für Serialisierung und C#-Mapping hinzufügen.

**Abnahme:** Kern und Wrapper verarbeiten dieselben stabilen Resultate; Mathematik bleibt im C++-Kern.

### Phase 7 – WinUI-Modul

- [ ] Gleichungssystem-Modul in die bestehende Modul-Registry integrieren.
- [ ] Eingabeoberfläche erstellen.
- [ ] Empfehlung und Begründung anzeigen.
- [ ] Umschaltung zwischen allen drei Verfahren anbieten.
- [ ] Strukturierte Schritte mit mathematischer Notation darstellen.
- [ ] Lösungsstatus, Exaktheit und Verifikation verständlich anzeigen.
- [ ] Parametrisierung bei unendlich vielen Lösungen darstellen.
- [ ] Tastatur, Touch und Screenreader berücksichtigen.

**Abnahme:** Die UI zeigt exakt die vom Kern gelieferten Schritte und Ergebnisse.

### Phase 8 – Tests, CI und Dokumentation

- [ ] Unit-Tests für Normalisierung, jeden Solver und Verifikation schreiben.
- [ ] Golden-Korpus für eindeutige, widersprüchliche, abhängige und numerisch schwierige Systeme erweitern.
- [ ] Differenztests: Alle drei Verfahren müssen bei eindeutig lösbaren 2×2-Systemen dieselbe Lösung liefern.
- [ ] Sicherstellen, dass jede eindeutige Lösung den unabhängigen Prüfer besteht.
- [ ] Nullkoeffizienten, Brüche, große Ganzzahlen, negative Zahlen und nahezu singuläre Matrizen testen.
- [ ] Parser- und Solver-Fuzzing ergänzen.
- [ ] Kern-Build, CTest, Golden-/Fuzz-Tests und WinUI-Build in CI ausführen.
- [ ] Architektur, Roadmap und README aktualisieren.

**Abnahme:** Alle relevanten CI-Prüfungen sind grün und die Dokumentation entspricht dem implementierten Verhalten.

## Mindest-Testmatrix

| Testfall | Erwartetes Ergebnis |
|---|---|
| Eindeutiges System | Alle drei Verfahren liefern dieselbe exakt verifizierte Lösung |
| Gebrochene Koeffizienten | Exakte Rational-Lösung |
| Bereits isolierte Variable | Einsetzungsverfahren wird sinnvoll bewertet |
| Geeignete Umformungsform | Gleichsetzungsverfahren ist korrekt auswählbar |
| Gegensätzliche Koeffizienten | Additionsverfahren eliminiert mit geringem Aufwand |
| Widerspruch | Status „keine Lösung“, kein Lösungspaar |
| Abhängige Gleichungen | Unendlich viele Lösungen mit Parameterdarstellung |
| Degenerierte Gleichung | Korrekte Klassifikation oder typisierter Eingabefehler |
| Große Zahlen / Vorzeichen | Keine Überläufe oder Vorzeichenfehler im exakten Pfad |
| Nahezu singuläres numerisches System | Unsicherheitsstatus statt falscher Gewissheit |
| Ungültiger Kandidat | Verifikation schlägt fehl; Kandidat wird nicht ausgegeben |

## Architekturregeln und Nicht-Ziele

- Keine mathematische Solver-Logik in C#, XAML oder UI.
- Keine Exceptions über die C-ABI-Grenze.
- Exakte Rational-Arithmetik bevorzugen, wenn die Eingabe exakt darstellbar ist.
- Numerische Näherungen und Toleranzen explizit kennzeichnen.
- Keine unbegründeten Prozent-Wahrscheinlichkeiten.
- Keine ungeprüften oder nachweislich falschen Lösungen anzeigen.
- Keine drei künstlichen Alternativen bei eindeutigem Ergebnis.
- Keine Übernahme von Code aus ELIAN.
- Keine zweite, parallele Mathematik-Engine neben der bestehenden Architektur.

## Abhängigkeiten und Reihenfolge

```text
Datenmodell / Normalisierung
        ↓
Gemeinsames Solver- und Ergebnis-Modell
        ↓
Drei 2×2-Verfahren
        ↓
Automatische Verfahrenswahl
        ↓
Unabhängige Verifikation
        ↓
C-ABI / JSON / C#-Wrapper
        ↓
WinUI-Modul + Schrittansicht
        ↓
Tests / CI / Dokumentation
```

## Definition of Done

- [ ] Drei Verfahren sind unabhängig implementiert und manuell auswählbar.
- [ ] Axial empfiehlt deterministisch ein Verfahren und erklärt die Auswahl.
- [ ] Alle Rechenschritte sind strukturiert und nachvollziehbar.
- [ ] Eindeutige Lösungen werden unabhängig verifiziert.
- [ ] Keine unbegründeten Wahrscheinlichkeiten werden ausgegeben.
- [ ] Keine ungültigen oder ungeprüften Kandidaten werden als gültig angezeigt.
- [ ] Keine und unendlich viele Lösungen werden korrekt klassifiziert.
- [ ] C-ABI, C#-Wrapper und WinUI-Modul sind integriert.
- [ ] Tests und CI sind grün.
- [ ] Architektur- und Roadmap-Dokumentation sind aktualisiert.

---

Dieses Enhancement beschreibt die geplante Umsetzung. Der verbindliche Aufgaben- und Fortschrittsstand wird in [Issue #3](https://github.com/teamgreis/axial-calculator/issues/3) gepflegt.
