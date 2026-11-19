# Axial — Benchmark-Dokumentation

Alle Zahlen sind **auf derselben Maschine gemessen** (GitHub-Runner
`windows-latest`, MSVC/MinGW-GCC -O2 für Axial, Node v22 für die
ELIAN-Baseline). Der CI-Job `baseline` lädt den ELIAN-v10.1-Engine-Block zur
Laufzeit aus dem öffentlichen Vorgänger-Repository und vermisst ihn — fair,
reproduzierbar, ohne Code aus dem alten Projekt zu übernehmen.

Gemessen wird pro Ausdruck in zwei Modi:

| Modus | Bedeutung |
|---|---|
| `eval` | Tokenize → Parse → Compile → Run → Format (Einzelschuss, wie ELIAN) |
| `compiled` | Lauf eines bereits kompilierten Programms (der schnelle Pfad) |

## Stand (Commit `35296a9`, CI-Läufe 44450494953 / 44454867072; Messwerte
## schwanken läuft-zu-lauf ~10 %, Tabelle = letzter grüner Lauf)

| Ausdruck | ELIAN v10.1 (ns) | Axial `eval` (ns) | Axial `compiled` (ns) | compiled vs. ELIAN |
|---|---|---|---|---|
| `2+3*4` | 1.690 | 10.862 | **87** | **19,4×** |
| `(1/3+1/6)*840` | 2.701 | 29.776 | **265** | **10,2×** |
| `sin(30)^2+cos(60)^2` | 3.582 | 19.156 | **1.080** | **3,3×** |
| `sqrt(2)*sqrt(3)+ln(e)` | 4.134 | 13.081 | **972** | **4,3×** |
| `(x+1)*(x+2)` | — | 10.261 | **699** | — |

**Phasenprofil `2+3*4` (100.000 Läufe):** Parse 2.287 ns · Parse+Compile
6.002 ns · kompletter Eval 10.862 ns · kompilierter Lauf 87 ns.

## Einordnung (ehrlich)

- Das **400×-Ziel ist noch nicht erreicht**. Stand: 3–19× auf dem
  kompilierten Pfad; der Einzelschuss liegt aktuell **hinter** ELIAN
  (V8 JIT gegen naive Allokation).
- Der kompilierte Lauf ist bereits heute der schnellste Pfad: für den
  Anwendungsfall „einmal eingeben, tausendfach auswerten" (Plot,
  Parametersuche, Verlauf, Favoriten) ist Axial deutlich vorne.
- **Identifizierte Ursachen und nächste Schritte** (Priorität):
  1. **BigInt-SBO** (Small-Buffer-Optimization): heute kostet jede kleine
     Zahl ~20 Heap-Allokationen (Vektor-Anlage, gcd mit Division). Inline-
     Speicher für ≤ 2 Limbs eliminiert den Großteil — erwartete Wirkung:
     Parse und exakte Arithmetik 5–20× schneller.
  2. **AST-Arena** statt `shared_ptr`-Knoten (Parse-Pfad).
  3. `Value`/`EvalResult` ohne String-Kopien im Rückkanal.
  4. Danach: gleiche Suite erneut vermessen; Zielkorridor 100–400× auf dem
     kompilierten Pfad, ≥ 2× einzeln.

## Fortschritt dieser Serie (selbe Maschine, kompilierter Pfad)

| Stand | `2+3*4` compiled |
|---|---|
| erster grüner Lauf | 3.204 ns |
| lazy Slot/Value-Konversion | 1.369 ns |
| VM-Neubau (POD-Slots) | 255–390 ns |
| + PushI/Integer/Schnellpfade | **87 ns** |

## Reproduzieren

Jeder Push auf `main` erzeugt zwei Jobs: `core` (Axial-Benchmark) und
`baseline` (ELIAN auf demselben Runner). Zahlen im Workflow-Log, Schritte
*Benchmark* bzw. *ELIAN-Baseline*.
