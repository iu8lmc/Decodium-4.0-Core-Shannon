// JTTY source codec: the 34-bit frame grammar of WSJT-X 3.2 JTTY.
//
// Port in C++ nativo di lib/jtty/jtty_source_codec.f90 e jtty_mod.f90
// (WSJT-X 3.2.0-rc1). Decodium non collega il runtime Fortran: la grammatica
// e' riscritta qui riga per riga, e tools/jtty_bench la confronta con il
// riferimento Fortran compilato a parte.
//
// Un frame porta 32 bit di grammatica, un bit riservato sempre a zero e il
// bit di fine messaggio. Qui un frame e' un intero: il bit 33 del Fortran
// (payload(1), il piu' significativo di n32) sta nel bit 33 dell'intero, il
// bit di fine messaggio nel bit 0.

#ifndef DECODIUM_JTTY_CODEC_HPP
#define DECODIUM_JTTY_CODEC_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace decodium
{
namespace jtty
{

constexpr int kMaxFrames = 16;
constexpr int kPayloadBits = 34;
constexpr int kMessageLength = 80;

// Il campo di testo del Fortran e' character*13: gli atomi lo tengono della
// stessa lunghezza, riempito di spazi, perche' alcune regole leggono le
// colonne fisse (text(1:5), text(1:4), text(1:1)).
constexpr int kAtomTextLength = 13;

enum AtomKind
{
  ATOM_CALL = 0,
  ATOM_EXCH_NUM = 1,
  ATOM_EXCH_LOC = 2,
  ATOM_EXCH_PAIR = 3,
  ATOM_EXCH_NUM_TIME = 4,
  ATOM_CONTROL = 5,
  ATOM_GRID4 = 6,
  ATOM_TEXT5 = 7
};

enum CallAction
{
  CALL_CQ = 0,
  CALL_CALL = 1,
  CALL_TU_CQ = 2,
  CALL_CALL_TU = 3,
  CALL_CALL_AGN = 4,
  CALL_TU_NOW = 5
};

enum Role
{
  ROLE_FIELD_ONLY = 0,
  ROLE_FULL = 1
};

enum NumberKind
{
  NUM_SERIAL = 0,
  NUM_CQ_ZONE = 1,
  NUM_ITU_ZONE = 2,
  NUM_AGE = 3,
  NUM_POWER = 4,
  NUM_CHECK = 5,
  NUM_LICENSE_YEAR = 6,
  NUM_GENERIC = 7
};

enum LocationKind
{
  LOC_STATE_PROVINCE = 0,
  LOC_SECTION = 1,
  LOC_COUNTRY_PREFIX = 2,
  LOC_QTH = 3,
  LOC_ADMIN_CODE = 4
};

enum PairSchema
{
  PAIR_ZONE_LOC3 = 0,
  PAIR_CLASS_SECTION = 1
};

enum ExchangeProfile
{
  EXCHANGE_UNKNOWN = 0,
  EXCHANGE_FIELD_DAY = 1,
  EXCHANGE_RTTY = 2
};

enum EncodeStatus
{
  ENCODE_OK = 0,
  ENCODE_INVALID_DESCRIPTOR = 1,
  ENCODE_UNKNOWN_SECTION = 2
};

constexpr int kControlPhraseCount = 18;

struct Atom
{
  int kind {-1};
  int subtype {0};
  int role {ROLE_FIELD_ONLY};
  int value {0};
  int value2 {0};
  std::string text = std::string (kAtomTextLength, ' ');
};

// Il descrittore di un atomo come lo compila l'interfaccia (i tasti F1-F8):
// stesso tracciato di jtty_source_atom_c e di Jtty::NativeAtomDescriptor.
struct NativeAtomDescriptor
{
  std::int8_t kind {};
  std::int8_t subtype {};
  std::int8_t role {};
  std::int8_t reserved {};
  std::int32_t value {};
  char text[9] {};
};

// Frame di 34 bit: bit 33 = payload(1), ..., bit 0 = payload(34) (fine messaggio).
using Frame = std::uint64_t;

char const* alphabet ();
int source_index (char c);
char source_char (int index);
char const* control_text (int phrase);
int arrl_section_count ();
char const* arrl_section (int index1);          // 1-based, come PACK77_ARRL_SECTIONS
int arrl_section_index (std::string const& section);  // -1 se assente

// Frame <-> bit del payload (payload[0] = payload(1) del Fortran).
void frame_to_payload (Frame frame, int payload[kPayloadBits]);
Frame payload_to_frame (int const payload[kPayloadBits]);

bool standard_call (std::string const& call);

Atom call_atom (int action, std::string const& call);
Atom exch_num_atom (int role, int kind, int value);
Atom exch_loc_atom (int role, int kind, std::string const& token);
Atom zone_loc_atom (int zone, std::string const& token);
Atom class_section_atom (int count, std::string const& cls, int section_index);
Atom exch_num_time_atom (int role, int serial, int minute_of_day);
Atom control_atom (int phrase);
Atom grid4_atom (int role, std::string const& grid);
Atom text5_atom (std::string const& text);

bool pack_atom (Atom const& atom, bool eom, Frame& frame);
bool unpack_atom (Frame frame, Atom& atom, bool& eom);
bool render_atom (Atom const& atom, std::string& text);

// Testo dell'operatore -> alfabeto JTTY (maiuscole, spazi compressi, '#' per
// i caratteri non rappresentabili). Il risultato e' lungo al piu' 80 colonne.
std::string normalize_message (std::string const& raw);

// pack_jtty: il messaggio minimo in frame. Ritorna il numero di frame, 0 per
// un messaggio vuoto, -1 se non entra in 16 frame o il profilo non e' valido.
// canonical riceve il testo normalizzato che verra' davvero trasmesso.
int pack_message (std::string const& message, int exchange_profile,
                  std::vector<Frame>& frames, std::string* canonical = nullptr);

bool pack_atoms (std::vector<Atom> const& atoms, std::vector<Frame>& frames);

// genjtty_atoms_c: descrittori dell'interfaccia -> frame. Ritorna EncodeStatus.
int pack_native_atoms (NativeAtomDescriptor const* atoms, int natoms,
                       std::vector<Frame>& frames);

struct UnpackResult
{
  std::string message;        // 80 colonne, come character*80
  bool trailing_sep {false};
  bool is_last_frame {false};
  bool source_valid {false};
};

UnpackResult unpack_frames (Frame const* frames, int nframes);

// Il testo per l'operatore: '~~~~~' (frame persi) diventa ' ... ', '~' uno spazio.
std::string display_message_text (std::string const& decoded);

// Utilita' sulle stringhe a lunghezza fissa del Fortran.
std::string rtrim (std::string const& s);
int len_trim (std::string const& s);
std::string fixed (std::string const& s, std::size_t length);

}
}

#endif
