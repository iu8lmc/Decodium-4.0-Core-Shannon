// test_nonstandard_call_directed.cpp
//
// Il giro completo per un nominativo NON STANDARD che risponde a una chiamata:
// si codifica il messaggio, lo si trasmette in una trama FT2 vera con rumore,
// lo si fa decodificare dal decoder Fortran di produzione e si verifica che il
// testo recuperato venga riconosciuto come diretto a noi.
//
// PERCHE' ESISTE. "II8IHBC" ha sette caratteri e non entra nella codifica
// standard a 28 bit: il protocollo lo manda col tipo 4, cioe' il nominativo del
// corrispondente come hash fra parentesi angolari e il proprio per esteso,
// senza coda -- "<IU8LMC> II8IHBC" e' un messaggio COMPLETO e vuol dire
// "II8IHBC chiama IU8LMC". Due difetti distinti lo rendevano inutile in aria
// (segnalati da IU8LMC l'11 e il 12/9/2026, su FT8, FT4 e FT2):
//
//   1) il filtro semantico lo scartava come messaggio monco, e la riga non
//      compariva affatto nella lista;
//   2) il sequencer non lo riconosceva come chiamata diretta, quindi la riga
//      si vedeva ma il contatto non si chiudeva mai.
//
// Entrambi nascevano dallo stesso abbaglio: cercare le parentesi angolari nei
// token GIA' normalizzati, dove normalizeCallToken() le ha rimosse. Su quei
// token un hash e' indistinguibile da un nominativo qualsiasi.
//
// QUESTO TEST NON REPLICA LA LOGICA. Il simulatore test_ft2_qso_sim.cpp imita
// la macchina a stati del bridge ("mirrors DecodiumBridge::autoSequenceStep"):
// utile per altro, inutile qui, perche' una copia che sbaglia come l'originale
// non prova niente. Qui si chiamano le funzioni VERE di Sequencer/
// MessageTokenRules, le stesse che usa DecodiumBridge.
//
// Uso: test_nonstandard_call_directed   (esce 0 se tutto passa)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <array>
#include <vector>

#include <QCoreApplication>
#include <QMutexLocker>
#include <QString>
#include <QStringList>
#include <QVector>

#include "Detector/FortranRuntimeGuard.hpp"

#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "Sequencer/MessageTokenRules.hpp"

extern "C"
{
  // Firma reale: le ultime tre size_t sono le lunghezze nascoste che gfortran
  // passa per gli argomenti stringa. Ometterle costa un segmentation fault.
  void ft2_async_decode_ (short iwave[], int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                          int* ndepth, int* ncontest, char mycall[], char hiscall[],
                          int snrs[], float dts[], float freqs[], int naps[], float quals[],
                          signed char bits77[], char decodeds[], int* nout,
                          size_t, size_t, size_t);
  void ftx_ft2_stage7_clravg_c ();
}

namespace {

constexpr int kSampleRate {12000};
constexpr int kFt2Samples {45000};
constexpr int kNsps {288};
constexpr float kCarrierHz {1500.0f};
constexpr int kMaxLines {100};   // il decoder Fortran scrive fino a 100 righe: con 32 si esce dagli array
constexpr int kDecodedChars {37};

QString const kMyCall {QStringLiteral ("IU8LMC")};
QString const kSpecialCall {QStringLiteral ("II8IHBC")};

int falliti = 0;

void verifica (bool condizione, QString const& cosa)
{
  std::printf ("  %-62s %s\n", cosa.toUtf8 ().constData (), condizione ? "ok" : "FALLITO");
  if (!condizione) ++falliti;
}

// Trama FT2 col messaggio dato, piu' rumore gaussiano calibrato.
std::vector<qint16> rendi_trama (QString const& messaggio, double snrDb, unsigned seme)
{
  auto const enc = decodium::txmsg::encodeFt2 (messaggio);
  if (!enc.ok || enc.tones.isEmpty ())
    throw std::runtime_error ("l'encoder rifiuta il messaggio: " + messaggio.toStdString ());

  QVector<float> const onda = decodium::txwave::generateFt2Wave (
      enc.tones.constData (), enc.tones.size (), kNsps,
      static_cast<float> (kSampleRate), kCarrierHz);

  double picco = 0.0;
  for (float s : onda) picco = std::max (picco, std::fabs (static_cast<double> (s)));
  if (picco <= 0.0) picco = 1.0;
  double const guadagno = 0.6 / picco;
  double const rms = guadagno * 0.5 * picco;
  double const sigma = rms / std::pow (10.0, snrDb / 20.0);

  std::vector<float> frame (static_cast<size_t> (kFt2Samples), 0.0f);
  int const offset = 3000;
  for (int i = 0; i < onda.size () && offset + i < kFt2Samples; ++i)
    frame[static_cast<size_t> (offset + i)] += static_cast<float> (guadagno) * onda[i];

  std::mt19937 rng {seme};
  std::normal_distribution<float> rumore {0.0f, static_cast<float> (sigma)};
  std::vector<qint16> pcm (static_cast<size_t> (kFt2Samples));
  for (int i = 0; i < kFt2Samples; ++i)
    {
      float const v = frame[static_cast<size_t> (i)] + rumore (rng);
      pcm[static_cast<size_t> (i)] =
          static_cast<qint16> (std::clamp (v * 32767.0f, -32768.0f, 32767.0f));
    }
  return pcm;
}

QString taglia (char const* campo, int n)
{
  QString s = QString::fromLatin1 (campo, n);
  return s.trimmed ();
}

// Decodifica la trama col decoder di produzione e restituisce i messaggi.
QStringList decodifica (std::vector<qint16>& pcm)
{
  int nqsoprogress = 0, nfqso = static_cast<int> (kCarrierHz), nfa = 200, nfb = 4000;
  int ndepth = 3, ncontest = 0, nout = 0;
  std::array<char, 12> my {}, his {};
  std::fill (my.begin (), my.end (), ' ');
  std::fill (his.begin (), his.end (), ' ');
  QByteArray const mine = kMyCall.toLatin1 ();
  std::copy_n (mine.constData (), std::min<int> (mine.size (), 12), my.begin ());

  std::array<int, kMaxLines> snrs {}, naps {};
  std::array<float, kMaxLines> dts {}, freqs {}, quals {};
  std::array<signed char, kMaxLines * 77> bits77 {};
  std::vector<char> decodeds (static_cast<size_t> (kMaxLines) * kDecodedChars, ' ');

  {
    QMutexLocker locker {&decodium::fortran::runtime_mutex ()};
    ftx_ft2_stage7_clravg_c ();
    ft2_async_decode_ (pcm.data (), &nqsoprogress, &nfqso, &nfa, &nfb,
                       &ndepth, &ncontest, my.data (), his.data (),
                       snrs.data (), dts.data (), freqs.data (),
                       naps.data (), quals.data (), bits77.data (),
                       decodeds.data (), &nout,
                       static_cast<size_t> (my.size ()),
                       static_cast<size_t> (his.size ()),
                       static_cast<size_t> (kDecodedChars));
  }

  QStringList righe;
  for (int i = 0; i < nout && i < kMaxLines; ++i)
    {
      QString const t = taglia (decodeds.data () + i * kDecodedChars, kDecodedChars);
      if (!t.isEmpty ()) righe << t;
    }
  return righe;
}

}  // namespace

int main (int argc, char* argv[])
{
  // Senza buffer: se il decoder crasha, l'ultima riga stampata e' davvero
  // l'ultima eseguita. Serve, e due volte e' servito davvero.
  std::setvbuf (stdout, nullptr, _IONBF, 0);
  QCoreApplication app {argc, argv};
  using namespace decodium::seq;

  std::printf ("Nominativo non standard che chiama: giro completo\n");
  std::printf ("  mio nominativo: %s   chiamante: %s\n\n",
               kMyCall.toUtf8 ().constData (), kSpecialCall.toUtf8 ().constData ());

  // --- 1. L'encoder accetta la forma di tipo 4 e la trasmette
  QString const messaggio = QStringLiteral ("<%1> %2").arg (kMyCall, kSpecialCall);
  QStringList recuperate;
  try
    {
      auto pcm = rendi_trama (messaggio, 6.0, 20260912u);
      recuperate = decodifica (pcm);
    }
  catch (std::exception const& e)
    {
      std::printf ("  trasmissione/decodifica non riuscita: %s\n", e.what ());
      return 1;
    }

  std::printf ("decodifica della trama (%d righe):\n", static_cast<int> (recuperate.size ()));
  for (QString const& r : recuperate) std::printf ("    '%s'\n", r.toUtf8 ().constData ());
  std::printf ("\n");

  // Il decoder deve restituire il messaggio col nominativo non standard per
  // esteso. Il destinatario puo' tornare come hash risolto "<IU8LMC>" oppure
  // come segnaposto "<...>" se l'hash non e' in cache: entrambi sono legittimi,
  // e il secondo caso e' quello che il ramo euristico deve coprire.
  QString ricevuto;
  for (QString const& r : recuperate)
    if (r.contains (kSpecialCall)) { ricevuto = r; break; }
  verifica (!ricevuto.isEmpty (),
            QStringLiteral ("il decoder recupera il messaggio del nominativo speciale"));

  // --- 2. Le regole vere riconoscono la forma canonica
  verifica (isNonStandardDirectedForm (messaggio),
            QStringLiteral ("'%1' e' riconosciuto come forma di tipo 4").arg (messaggio));

  QString destinatario, mittente;
  verifica (splitNonStandardDirected (messaggio, &destinatario, &mittente),
            QStringLiteral ("i due nominativi si estraggono"));
  verifica (destinatario == kMyCall,
            QStringLiteral ("il destinatario e' il mio nominativo (%1)").arg (destinatario));
  verifica (mittente == kSpecialCall,
            QStringLiteral ("il mittente e' la stazione speciale (%1)").arg (mittente));

  // --- 3. Lo stesso, sul testo COSI' COME ESCE DAL DECODER
  if (!ricevuto.isEmpty () && ricevuto.contains (QLatin1Char ('<')))
    {
      QString d2, m2;
      bool const estratto = splitNonStandardDirected (ricevuto, &d2, &m2);
      bool const segnaposto = ricevuto.contains (QStringLiteral ("<...>"));
      if (segnaposto)
        {
          verifica (!estratto,
                    QStringLiteral ("hash non risolto: resta al ramo euristico, come deve"));
        }
      else
        {
          verifica (estratto && m2 == kSpecialCall,
                    QStringLiteral ("dal testo decodificato si estrae il mittente"));
        }
    }

  // --- 4. Le forme che NON devono far muovere il sequencer
  verifica (!isNonStandardDirectedForm (QStringLiteral ("CQ II8IHBC")),
            QStringLiteral ("un CQ non e' una chiamata diretta"));
  verifica (!isNonStandardDirectedForm (QStringLiteral ("IU8LMC II8IHBC -12")),
            QStringLiteral ("con rapporto non e' la forma canonica"));
  QString d3, m3;
  verifica (splitNonStandardDirected (QStringLiteral ("<IK7YC> II8IHBC"), &d3, &m3)
                && d3 != kMyCall,
            QStringLiteral ("diretto a un altro: si estrae, ma non e' per me"));

  std::printf ("\n%s (%d controlli falliti)\n", falliti == 0 ? "TUTTO A POSTO" : "CI SONO ERRORI",
               falliti);
  return falliti == 0 ? 0 : 1;
}
