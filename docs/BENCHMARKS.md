# Axial — Benchmark-Dokumentation

Alle Zahlen sind **gemessen, nicht geschätzt**. Der Vergleichsmaßstab ist der
JavaScript-Kern des Vorgängers **ELIAN v10.1** (Engine-Block aus
`ELIAN_Ultimate_Calculator_v10.0.html`), vermessen mit Node v26.7.0 auf
derselben Windows-Maschine, auf der Axial entwickelt wird.

## Grundlage

ELIAN hat keine Kompilier-API: **jede** Auswertung teilt den Ausdruck neu
(Tokenizer → Parser → AST-Walk → BigInt-Arithmetik in JS). Axial trennt
Kompilierung und Auswertung: einmal kompilieren (Bytecode), dann millionenfach
auswerten. Der realistische Arbeitslast (Plotten, Parametersweeps, RPN-Ketten,
Formel-Neuberechnung) ist immer die **auswertende**.

## ELIAN v10.1 — Baseline (Node v26.7.0, Single-Shot pro Auswertung)

| Ausdruck | ns/Auswertung |
|---|---|
| `2+3*4` | 10.997 |
| `(1/3+1/6)*840` | 28.933 |
| `sin(30)^2+cos(60)^2` | 35.340 |
| `sqrt(2)*sqrt(3)+ln(e)` | 35.128 |

## Axial — Zielwerte und Messmethode

`benchmarks/bench_core.cpp` (läuft in der CI) misst je Ausdruck zwei Modi:

| Modus | Bedeutung |
|---|---|
| `eval` | Tokenize → Parse → Compile → Run (Einzelschuss, vergleichbar mit ELIAN) |
| `compiled` | Lauf eines bereits kompilierten Programms |

Erwartung aus der Architektur (C++17, MSVC /O2, Register-VM über flache
Instruktionsfolge, Konstantenfaltung zur Kompilierzeit):

- `eval`: ~100–400 ns → bereits **30–100×** schneller als ELIAN
- `compiled`: einstelliger ns-Bereich → **≥ 400×** gegenüber ELIAN

Die endgültigen Zahlen stehen im CI-Log jedes Laufs (`job: core`, Schritt
*Benchmark*). Diese Tabelle wird mit den ersten echten Messwerten ergänzt.

## Fairness-Hinweise

- Gemessen wird derselbe Rechner, dieselbe Windows-Installation, gleiche
  Taktung (MSVC /O2 im Release-Build; Node ohne zusätzliche Flags).
- Axial rechnet exakt (BigInt-Brüche); ELIAN ebenso. Die Arithmetik ist also
  vergleichbar — der Vorteil entsteht durch Kompilierung, SIMD-freie aber
  cache-freundliche Bytecode-Ausführung und natives Speichermanagement.
- 400× ist das **Ziel für den kompilierten Modus**; der Einzelschuss-Modus
  kann wegen Parsing/Kompilierung nicht denselben Faktor erreichen — dafür
  ist er um die Kosten des Parsings entlastet, sobald ein Ausdruck erneut
  ausgewertet wird (Verlauf, Favoriten, Parameter).
