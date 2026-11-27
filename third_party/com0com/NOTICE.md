# com0com — Null-modem emulator

DecoPort usa com0com per creare le porte COM virtuali dietro cui condivide una
radio nel suo protocollo nativo. Lo distribuisce **invariato**, come programma
separato: DecoPort non ne include codice e non si collega a nessuna sua
libreria, lo installa e lo comanda (`setupc.exe`) come farebbe una persona.

| | |
|---|---|
| Progetto | com0com, *Null-modem emulator* — <https://com0com.sourceforge.net/> |
| Autore | **Vyacheslav Frolov** — Copyright © 2004-2012 Vyacheslav Frolov |
| Versione | 3.0.0.0 (2012-06-04, vedi `Changes.txt` nell'archivio sorgente) |
| Licenza | GNU General Public License, versione 2 **o successiva** (`license.txt`; la dicitura "or (at your option) any later version" è negli header dei sorgenti) |
| Download | <https://sourceforge.net/projects/com0com/files/com0com/3.0.0.0/> |
| Firma dei driver e degli installatori | CyberCircuits, Auckland (NZ) — certificato COMODO RSA Code Signing CA, valido dal 2016-01-04 al 2018-01-27, con marca temporale COMODO |

## I file, e come verificarli

Scaricati il 2026-10-14 dalla pagina sopra. Gli MD5 sono quelli che SourceForge
pubblica per ogni file (feed `https://sourceforge.net/projects/com0com/rss?path=/com0com/3.0.0.0`);
gli SHA-256 sono calcolati qui, e sono quelli che DecoPort controlla prima di
eseguire un installatore con i privilegi di amministratore.

| File | Origine | MD5 (SourceForge) | SHA-256 |
|---|---|---|---|
| `Setup_com0com_v3.0.0.0_W7_x64_signed.exe` | dentro `com0com-3.0.0.0-i386-and-x64-signed.zip` | zip: `a07aa80024cca837dacc75d3573285ff` | `26486b28604b49a9008c54feb11b9ece0008a8287ee5caf0bcf2a62f4317128f` |
| `Setup_com0com_v3.0.0.0_W7_x86_signed.exe` | dentro `com0com-3.0.0.0-i386-and-x64-signed.zip` | zip: `a07aa80024cca837dacc75d3573285ff` | `de917425e022839f6374b509e3d34f3f9e9773915a44996194442337eadf4a5a` |
| `com0com-3.0.0.0.zip` | i sorgenti, come pubblicati | `b59ebb83effd10ddb210fa164aa540aa` | `6751e911f73980b23cc878a456eb99d1dc6d0603c11c1d3ced109abe1c556380` |
| `license.txt` | dall'archivio sorgente | — | `9ccb32bcf0183b09d884dacdd6106fb553075e2dc4483da96597283a6712582e` |
| `README-upstream.txt` | `README.txt` della stessa cartella di download | `e2af7445344e677477d95d46f5d25524` | `310cb0c35347261401ee4a32fe52832d072b7f202508d70f14f0eaa47cf0870e` |

I sorgenti viaggiano insieme ai binari perché la GPL chiede che chi riceve un
programma possa averne anche il codice.

## Cosa fa DecoPort con com0com

- `decoport-hub --install-com0com`, oppure la prima volta che serve con
  `--vcom auto`: esegue l'installatore della propria architettura con `/S`
  (installazione silenziosa, che per com0com non crea coppie da sola).
  Windows chiede l'amministratore.
- Per ogni radio crea una coppia con
  `setupc --silent install PortName=COM# dcd=ropen`: la parte visibile prende
  il primo numero COM libero; su quella nascosta (`CNCB<n>`) il DCD segue
  l'apertura dell'altra parte, ed è così che DecoPort si accorge che un
  programma ha chiuso la porta anche quando non usa DTR. Anche questo chiede
  l'amministratore, una volta per radio: le coppie restano anche dopo il
  riavvio.
- Elenca le coppie leggendo i dispositivi del sistema, senza `setupc`: dalla
  3.0 `setupc.exe` chiede sempre l'amministratore, anche solo per elencare.

## Un limite da conoscere

Il driver è firmato con un certificato emesso nel 2016, senza la firma di
Microsoft. Da Windows 10 1607, **con Secure Boot attivo**, Windows carica i
driver del kernel nuovi solo se firmati da Microsoft (l'eccezione vale per i
certificati emessi prima del 29 luglio 2015). Su quei PC la coppia viene
creata ma il driver non parte, e Gestione dispositivi mostra il codice 52:
DecoPort lo riconosce e lo dice. Le alternative sono un driver con firma
Microsoft (commerciale) indicato con `--vcom radio=estremo`, oppure le prese
`rigctld` e `--raw`, che non hanno bisogno di nessun driver.
