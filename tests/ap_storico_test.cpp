// ap_storico_test.cpp — collaudo isolato dell'elenco delle stazioni sentite e
// dei bit del mittente, PRIMA di toccare il decoder.
//
// Due cose vanno dimostrate, e sono indipendenti dal decodificatore:
//
//  1. I bit prodotti da ftx_ft8_ap_bits_mittente_c devono coincidere con
//     quelli che la catena esistente gia' produce per lo stesso nominativo.
//     ftx_prepare_ft8_ap_c mette il corrispondente nelle posizioni 29-57 di
//     apsym: se i due non coincidono, l'ipotesi a priori sarebbe sbagliata e
//     il decoder verrebbe portato fuori strada invece che aiutato. E' il
//     controllo che conta di piu': un a priori SBAGLIATO non e' neutro, e'
//     dannoso.
//
//  2. L'elenco a scorrimento deve rispettare finestra, memoria e ordine
//     (dal piu' recente), e non deve restituire doppioni.
//
// uso: ap_storico_test
#include <QCoreApplication>
#include <QTextStream>

#include <array>
#include <cstring>

#include "Detector/FtxApStorico.hpp"

extern "C"
{
  void ftx_prepare_ft8_ap_c (char const mycall[12], char const hiscall[12], int ncontest,
                             int* apsym, int* aph10);
  int ftx_ft8_ap_bits_mittente_c (char const* nominativo, int bits[29]);
}

namespace
{
int falliti = 0;

void esito (QTextStream& out, bool ok, QString const& che)
{
  out << (ok ? "  ok   " : "  NO   ") << che << '\n';
  if (!ok) ++falliti;
}

QByteArray campo12 (char const* s)
{
  QByteArray b {s};
  b.truncate (12);
  b.append (12 - b.size (), ' ');
  return b;
}
}  // namespace

int main (int argc, char* argv[])
{
  QCoreApplication app {argc, argv};
  QTextStream out {stdout};

  out << "1) i bit del mittente coincidono con quelli della catena esistente\n";
  for (char const* call : {"K1ABC", "DL9XYZ", "IU8LMC", "9A4ZM", "VK3ABC", "JA1XYZ"})
    {
      std::array<int, 58> apsym {};
      std::array<int, 10> aph10 {};
      // "il mio nominativo e' KA1ABC, il suo e' <call>": la catena mette <call>
      // nelle posizioni 29-57, che sono esattamente quelle che ci interessano.
      ftx_prepare_ft8_ap_c (campo12 ("KA1ABC").constData (), campo12 (call).constData (),
                            0, apsym.data (), aph10.data ());
      std::array<int, 29> miei {};
      int const ok = ftx_ft8_ap_bits_mittente_c (call, miei.data ());
      bool uguali = ok == 1;
      for (int i = 0; i < 29 && uguali; ++i)
        {
          uguali = miei[static_cast<size_t> (i)] == apsym[static_cast<size_t> (29 + i)];
        }
      esito (out, uguali, QStringLiteral ("%1").arg (call));
    }

  out << "\n2) i nominativi non standard vengono RIFIUTATI, non forzati\n";
  for (char const* call : {"AB", "", "K1ABC/QRP/X"})
    {
      std::array<int, 29> b {};
      esito (out, ftx_ft8_ap_bits_mittente_c (call, b.data ()) == 0,
             QStringLiteral ("\"%1\" rifiutato").arg (call));
    }

  out << "\n3) l'elenco a scorrimento\n";
  using namespace decodium::apstorico;
  azzera ();
  registra (10, 1500.0f, "K1ABC");
  registra (11, 1502.0f, "DL9XYZ");
  registra (12, 1600.0f, "JA1XYZ");        // lontano in frequenza
  registra (13, 1501.0f, "K1ABC");         // doppione, ma piu' recente

  char trovati[4][kLunghezzaCall] {};
  int n = vicini (14, 1500.0f, 5.0f, 10, 4, trovati);
  esito (out, n == 2, QStringLiteral ("entro +-5 Hz se ne trovano 2, trovati %1").arg (n));
  esito (out, n > 0 && std::strcmp (trovati[0], "K1ABC") == 0,
         QStringLiteral ("il piu' recente e' K1ABC, e' \"%1\"")
             .arg (n > 0 ? trovati[0] : "-"));
  esito (out, n > 1 && std::strcmp (trovati[1], "DL9XYZ") == 0,
         QStringLiteral ("il secondo e' DL9XYZ, e' \"%1\"")
             .arg (n > 1 ? trovati[1] : "-"));

  n = vicini (14, 1600.0f, 5.0f, 10, 4, trovati);
  esito (out, n == 1 && std::strcmp (trovati[0], "JA1XYZ") == 0,
         QStringLiteral ("a 1600 Hz si trova solo JA1XYZ (%1)").arg (n));

  n = vicini (14, 1500.0f, 5.0f, 2, 4, trovati);   // memoria corta: solo cicli 12,13
  esito (out, n == 1 && std::strcmp (trovati[0], "K1ABC") == 0,
         QStringLiteral ("con memoria 2 resta solo K1ABC (%1)").arg (n));

  n = vicini (14, 1500.0f, 5.0f, 10, 1, trovati);
  esito (out, n == 1, QStringLiteral ("il tetto su quante ne torna e' rispettato (%1)").arg (n));

  // Il ciclo corrente non e' storia: una stazione sentita ADESSO non deve
  // diventare ipotesi per se stessa, altrimenti la misura del guadagno sarebbe
  // falsata (si "indovinerebbe" cio' che si e' appena letto).
  azzera ();
  registra (20, 1500.0f, "K1ABC");
  n = vicini (20, 1500.0f, 5.0f, 10, 4, trovati);
  esito (out, n == 0, QStringLiteral ("il ciclo corrente non conta (%1)").arg (n));

  out << "\n4) l'estrazione del mittente dal messaggio\n";
  struct Caso { char const* msg; char const* atteso; };
  // L'ultimo caso e' quello che conta: se si leggesse il secondo campo alla
  // cieca, "DX" finirebbe nell'elenco e sprecherebbe un'ipotesi a ogni
  // candidato vicino.
  for (Caso c : {Caso {"IU8LMC K1ABC R-10", "K1ABC"},
                 Caso {"CQ DL9XYZ JO62", "DL9XYZ"},
                 Caso {"CQ DX JA1XYZ PM95", "JA1XYZ"},
                 Caso {"K1ABC IU8LMC RR73", "IU8LMC"}})
    {
      azzera ();
      registra_da_messaggio (5, 1500.0f, c.msg);
      char t[2][kLunghezzaCall] {};
      int const n = vicini (6, 1500.0f, 5.0f, 10, 2, t);
      esito (out, n == 1 && std::strcmp (t[0], c.atteso) == 0,
             QStringLiteral ("\"%1\" -> %2 (trovato \"%3\")")
                 .arg (c.msg, c.atteso, n > 0 ? t[0] : "-"));
    }

  // Un messaggio che non contiene nominativi non deve sporcare l'elenco.
  azzera ();
  registra_da_messaggio (5, 1500.0f, "CQ");
  esito (out, quante () == 0, QStringLiteral ("\"CQ\" da solo non registra niente"));

  // 4b) FT8 e FT2 non devono leggersi l'archivio a vicenda. Un messaggio FT8
  // non e' mai presente nell'audio di uno slot FT2: come ipotesi non puo'
  // andare a buon fine, e in cambio espone alla CRC-14. Tutto il costo del
  // rischio e zero guadagno possibile, quindi la ricerca filtra per modo.
  out << "\n4b) l'archivio dei messaggi e' separato per modo\n";
  {
    azzera ();
    std::array<signed char, 77> bits_ft8 {};
    std::array<signed char, 77> bits_ft2 {};
    for (int i = 0; i < 77; ++i)
      {
        bits_ft8[static_cast<size_t> (i)] = static_cast<signed char> (i & 1);
        bits_ft2[static_cast<size_t> (i)] = static_cast<signed char> ((i + 1) & 1);
      }
    registra_messaggio (1500.0f, bits_ft8.data (), kModoFt8);
    registra_messaggio (2000.0f, bits_ft2.data (), kModoFt2);

    std::array<signed char, 77> letto {};
    esito (out, trova_messaggio (1500.0f, 5.0f, 0, 60000, letto.data (), kModoFt8) == 1
                && letto == bits_ft8,
           QStringLiteral ("FT8 ritrova il proprio messaggio"));
    esito (out, trova_messaggio (2000.0f, 5.0f, 0, 60000, letto.data (), kModoFt2) == 1
                && letto == bits_ft2,
           QStringLiteral ("FT2 ritrova il proprio messaggio"));
    esito (out, trova_messaggio (2000.0f, 5.0f, 0, 60000, letto.data (), kModoFt8) == 0,
           QStringLiteral ("FT8 NON vede il messaggio di FT2"));
    esito (out, trova_messaggio (1500.0f, 5.0f, 0, 60000, letto.data (), kModoFt2) == 0,
           QStringLiteral ("FT2 NON vede il messaggio di FT8"));

    std::array<float, 8> freq {};
    esito (out, frequenze_messaggi (0, 60000, freq.data (), 8, kModoFt2) == 1
                && freq[0] > 1999.0f && freq[0] < 2001.0f,
           QStringLiteral ("le frequenze attese di FT2 sono solo le sue"));
    esito (out, frequenze_messaggi (0, 60000, freq.data (), 8, kModoFt8) == 1
                && freq[0] > 1499.0f && freq[0] < 1501.0f,
           QStringLiteral ("le frequenze attese di FT8 sono solo le sue"));
    esito (out, quanti_messaggi () == 2,
           QStringLiteral ("l'archivio resta uno solo (2 voci)"));
    azzera ();
  }

  out << "\n5) il contatore di ciclo avanza\n";
  int const c0 = ciclo_corrente ();
  int const c1 = avanza_ciclo ();
  esito (out, c1 == c0 + 1 && ciclo_corrente () == c1,
         QStringLiteral ("%1 -> %2").arg (c0).arg (c1));

  out << '\n' << (falliti == 0 ? "TUTTO A POSTO" : QStringLiteral ("%1 FALLITI").arg (falliti))
      << '\n';
  return falliti == 0 ? 0 : 1;
}
