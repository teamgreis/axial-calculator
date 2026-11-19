# Axial — Benchmark-Dokumentation

Alle Zahlen sind **auf derselben Maschine gemessen** (GitHub-Runner
`windows-latest`, MinGW-GCC -O2 für Axial, Node für die ELIAN-Baseline). Der
CI-Job `baseline` lädt den ELIAN-v10.1-Engine-Block zur Laufzeit aus dem
öffentlichen Vorgänger-Repository und vermisst ihn — fair, reproduzierbar,
ohne Code aus dem alten Projekt zu übernehmen.

| Modus | Bedeutung |
|---|---|
| `eval` | Tokenize → Parse → Compile → Run → Format (Einzelschuss, wie ELIAN) |
| `compiled` | Lauf eines bereits kompilierten Programms (der schnelle Pfad) |

## Stand (Commits `964c08b`+`d2639e3`, CI-Lauf 44557995696)

| Ausdruck | ELIAN v10.1 | Axial `eval` | Axial `compiled` | compiled vs. ELIAN |
|---|---|---|---|---|
| `2+3*4` | 1.583 ns | 2.588 ns | **42 ns** | **38×** |
| `(1/3+1/6)*840` | 2.330 ns | 5.001 ns | **57 ns** | **41×** |
| `sin(30)^2+cos(60)^2` | 3.297 ns | 5.537 ns | **508 ns** | **6,5×** |
| `sqrt(2)*sqrt(3)+ln(e)` | 3.862 ns | 4.687 ns | **504 ns** | **7,7×** |
| `(x+1)*(x+2)` | — | 3.182 ns | **202 ns** | — |

**Phasenprofil `2+3*4`:** Parse 1.041 ns · Parse+Compile 2.191 ns ·
kompilierter Lauf 42 ns.

## Optimierungsverlauf (kompilierter Pfad, selbe Maschine)

| Stand | `2+3*4` | `(1/3+1/6)*840` |
|---|---|---|
| erster grüner Lauf | 3.204 ns | 3.252 ns |
| VM-Neubau (POD-Slots) | 255 ns | 306 ns |
| + PushI/Integer/Schnellpfade | 106 ns | 265 ns |
| + BigInt-Zwei-Wege-Darstellung | 43 ns | 57 ns |
| + AST-Arena | **42 ns** | **57 ns** |

Damit ist Axial auf dem kompilierten Pfad **6,5–41× schneller als ELIAN**;
das 400×-Ziel ist noch nicht erreicht. Der verbleibende Aufwand im
Einzelschuss (Parse ~1 µs, Compile ~1,1 µs) liegt in String-/Vektor-Overhead
des Tokenizers und der Node-Erzeugung; als nächste Schritte sind dort
`string_view`-Token und nameslose Knoten vorgesehen — Aufwand/Nutzen ist für
die Anwendung jedoch gering, denn der realistische Lastfall (einmal eingeben,
tausendfach auswerten: Plot, Parametersuche, Verlauf) läuft bereits auf dem
kompilierten Pfad.

## Reproduzieren

Jeder Push auf `main` erzeugt zwei Jobs: `core` (Axial-Benchmark) und
`baseline` (ELIAN auf demselben Runner). Zahlen im Workflow-Log, Schritte
*Benchmark* bzw. *ELIAN-Baseline*.
