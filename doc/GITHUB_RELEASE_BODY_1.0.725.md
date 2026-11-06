# Decodium 4 v1.0.725

## English (British)

This release is built on v1.0.724 and corrects three log messages in the pile-up (MAM) sequencer. The behaviour is unchanged.

- A station whose exchange is kept for a late report or final 73 is logged as "held for a late report, not logged yet", not as "dropped without log".
- The duplicate check reads "skip recently completed or abandoned partner", because it covers both.

## Italiano

Questa versione è basata sulla v1.0.724 e corregge tre messaggi di log del sequencer pile-up (MAM). Il comportamento non cambia.

- Una stazione il cui scambio viene conservato per un rapporto tardivo o un 73 finale viene registrata come "held for a late report, not logged yet", non come "dropped without log".
- Il controllo dei duplicati scrive "skip recently completed or abandoned partner", perché copre entrambi i casi.
