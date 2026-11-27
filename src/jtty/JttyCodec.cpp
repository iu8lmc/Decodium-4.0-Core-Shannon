// Port di lib/jtty/jtty_source_codec.f90 e jtty_mod.f90 (WSJT-X 3.2.0-rc1).
// Le regole sono quelle del Fortran, nello stesso ordine: dove il Fortran
// legge colonne fisse di una stringa, qui si fa lo stesso sulla stringa
// riempita di spazi, perche' e' li' che stanno i casi limite.

#include "JttyCodec.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace decodium
{
namespace jtty
{

namespace
{

char const kAlphabet[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ +-./?!\"#$%,&*()_'=[]{}<>|:;";

char const* const kControlText[kControlPhraseCount] = {
  "AGN?", "CALL?", "AGN CALL", "NR?", "AGN NR", "EXCH?", "STATE?", "SECTION?",
  "ZONE?", "GRID?", "RPRT?", "QSL TU", "TU", "QRZ?", "QSO B4", "WAIT", "NIL?", "OK?"
};

char const* const kArrlSections[] = {
  "AB", "AK", "AL", "AR", "AZ", "BC", "CO", "CT", "DE", "EB",
  "EMA", "ENY", "EPA", "EWA", "GA", "GH", "IA", "ID", "IL", "IN",
  "KS", "KY", "LA", "LAX", "NS", "MB", "MDC", "ME", "MI", "MN",
  "MO", "MS", "MT", "NC", "ND", "NE", "NFL", "NH", "NL", "NLI",
  "NM", "NNJ", "NNY", "TER", "NTX", "NV", "OH", "OK", "ONE", "ONN",
  "ONS", "OR", "ORG", "PAC", "PR", "QC", "RI", "SB", "SC", "SCV",
  "SD", "SDG", "SF", "SFL", "SJV", "SK", "SNJ", "STX", "SV", "TN",
  "UT", "VA", "VI", "VT", "WCF", "WI", "WMA", "WNY", "WPA", "WTX",
  "WV", "WWA", "WY", "DX", "PE", "NB"
};
constexpr int kArrlSectionCount = sizeof (kArrlSections) / sizeof (kArrlSections[0]);

char const kBase36[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Suddivisione dello spazio c28 (packjt77_grammar).
constexpr int kNTokens = 2063592;
constexpr int kMax22 = 4194304;

using I8 = long long;

bool is_digit (char c) { return c >= '0' && c <= '9'; }
bool is_upper (char c) { return c >= 'A' && c <= 'Z'; }

// Fortran ibits(value, pos, len).
I8 ibits (I8 value, int pos, int len)
{
  return (value >> pos) & ((I8 {1} << len) - 1);
}

// Fortran index(s, c): posizione 1-based, 0 se assente.
int findex (std::string const& s, char c)
{
  auto const p = s.find (c);
  return p == std::string::npos ? 0 : static_cast<int> (p) + 1;
}

// s(i:j) 1-based, inclusivo; vuota se j < i.
std::string sub (std::string const& s, int i, int j)
{
  if (j < i) return {};
  std::string r;
  r.reserve (j - i + 1);
  for (int k = i; k <= j; ++k)
    {
      std::size_t const idx = static_cast<std::size_t> (k - 1);
      r.push_back (idx < s.size () ? s[idx] : ' ');
    }
  return r;
}

char at (std::string const& s, int i)
{
  std::size_t const idx = static_cast<std::size_t> (i - 1);
  return idx < s.size () ? s[idx] : ' ';
}

// lib/chkcall.f90
bool chkcall (std::string const& w13)
{
  std::string const w = fixed (w13, 13);
  std::string bc = sub (w, 1, 6);
  int const n1 = len_trim (w);
  if (n1 > 11) return false;
  if (findex (w, '.') >= 1 || findex (w, '+') >= 1 || findex (w, '-') >= 1
      || findex (w, '?') >= 1)
    return false;
  if (n1 > 6 && findex (w, '/') <= 0) return false;

  int const i0 = findex (w, '/');
  if (std::max (i0 - 1, n1 - i0) > 6) return false;
  if (i0 >= 2 && i0 <= n1 - 1)
    {
      if (i0 - 1 <= n1 - i0) bc = fixed (sub (w, i0 + 1, n1) + "   ", 6);
      if (i0 - 1 > n1 - i0) bc = fixed (sub (w, 1, i0 - 1) + "   ", 6);
    }

  int const nbc = len_trim (bc);
  if (nbc > 6) return false;
  if (!is_upper (at (bc, 1)) && !is_upper (at (bc, 2))) return false;
  if (at (bc, 1) == 'Q' && sub (bc, 1, 5) != "QU1RK") return false;

  int i1 = 0;
  if (is_digit (at (bc, 2))) i1 = 2;
  if (is_digit (at (bc, 3))) i1 = 3;
  if (i1 == 0) return false;
  if (i1 == nbc) return false;
  int n = 0;
  for (int i = i1 + 1; i <= nbc; ++i)
    {
      if (!is_upper (at (bc, i))) return false;
      ++n;
    }
  return n >= 1 && n <= 3;
}

// packjt77.f90 callok
bool callok (std::string const& w)
{
  auto islet = [] (char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
  int const n = len_trim (w);
  if (n < 3) return false;
  if (at (w, 1) == 'Q') return false;
  int i = n;
  for (; i >= 1; --i)
    if (is_digit (at (w, i))) break;
  int const i0 = i;
  if (i0 != 2 && i0 != 3) return false;
  std::string const pfx = fixed (sub (w, 1, i0 - 1), 2);
  int nlp = 0, ndp = 0;
  int const np = len_trim (pfx);
  for (int k = 1; k <= np; ++k)
    {
      if (is_digit (at (pfx, k))) ++ndp;
      if (islet (at (pfx, k))) ++nlp;
    }
  if (nlp + ndp != np) return false;
  if (nlp == 0) return false;
  int const ns = n - i0;
  if (ns < 1 || ns > 3) return false;
  int nls = 0;
  for (int k = i0 + 1; k <= n; ++k)
    if (islet (at (w, k))) ++nls;
  return nls >= ns;
}

// packjt77_grammar.f90 pack77_c28_standard_shape
bool c28_standard_shape (std::string const& token)
{
  int const n = len_trim (token);
  if (n < 3 || n > 6) return false;
  for (int i = 1; i <= n; ++i)
    {
      char const c = at (token, i);
      if (!(is_upper (c) || is_digit (c))) return false;
    }
  int iarea = 0;
  for (int i = n; i >= 2; --i)
    if (is_digit (at (token, i)))
      {
        iarea = i;
        break;
      }
  if (iarea != 2 && iarea != 3) return false;
  int npdig = 0, nplet = 0;
  for (int i = 1; i <= iarea - 1; ++i)
    {
      char const c = at (token, i);
      if (is_digit (c)) ++npdig;
      else if (is_upper (c)) ++nplet;
      else return false;
    }
  if (nplet == 0 || npdig >= iarea - 1) return false;
  int nslet = 0;
  for (int i = iarea + 1; i <= n; ++i)
    {
      if (is_upper (at (token, i))) ++nslet;
      else return false;
    }
  return nslet <= 3;
}

// pack28 ridotto a cio' che JTTY puo' portare: un nominativo standard. Tutto
// il resto il Fortran lo cifra come hash a 22 bit, che al ritorno non da'
// mai indietro lo stesso testo: qui si risponde subito -1.
int pack28_standard (std::string const& c13in)
{
  std::string const c13 = fixed (c13in, 13);
  if (!c28_standard_shape (c13)) return -1;
  int const n = len_trim (c13);
  int i = n;
  for (; i >= 2; --i)
    if (is_digit (at (c13, i))) break;
  int const iarea = i;
  std::string callsign;
  if (iarea == 2) callsign = " " + sub (c13, 1, 5);
  else callsign = sub (c13, 1, 6);
  static std::string const a1 = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  static std::string const a2 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  static std::string const a3 = "0123456789";
  static std::string const a4 = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int const i1 = findex (a1, at (callsign, 1)) - 1;
  int const i2 = findex (a2, at (callsign, 2)) - 1;
  int const i3 = findex (a3, at (callsign, 3)) - 1;
  int const i4 = findex (a4, at (callsign, 4)) - 1;
  int const i5 = findex (a4, at (callsign, 5)) - 1;
  int const i6 = findex (a4, at (callsign, 6)) - 1;
  if (i1 < 0 || i2 < 0 || i3 < 0 || i4 < 0 || i5 < 0 || i6 < 0) return -1;
  int n28 = 36 * 10 * 27 * 27 * 27 * i1 + 10 * 27 * 27 * 27 * i2 + 27 * 27 * 27 * i3
      + 27 * 27 * i4 + 27 * i5 + i6;
  n28 += kNTokens + kMax22;
  return n28 & ((1 << 28) - 1);
}

// unpack28 (packjt77.f90). La tabella degli hash non c'e': un hash torna
// come "<...>", che nessun controllo di JTTY accetta, esattamente come il
// "<CALL>" che restituirebbe il Fortran.
bool unpack28 (int n28, std::string& c13)
{
  bool success = true;
  if (n28 < kNTokens)
    {
      if (n28 == 0) c13 = "DE";
      else if (n28 == 1) c13 = "QRZ";
      else if (n28 == 2) c13 = "CQ";
      else if (n28 <= 1002)
        {
          char buf[16];
          std::snprintf (buf, sizeof buf, "CQ_%03d", n28 - 3);
          c13 = buf;
        }
      else if (n28 <= 532443)
        {
          static std::string const c4 = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";
          int n = n28 - 1003;
          int const j1 = n / (27 * 27 * 27);
          n -= 27 * 27 * 27 * j1;
          int const j2 = n / (27 * 27);
          n -= 27 * 27 * j2;
          int const j3 = n / 27;
          int const j4 = n - 27 * j3;
          std::string s {c4[j1], c4[j2], c4[j3], c4[j4]};
          std::size_t const first = s.find_first_not_of (' ');
          s = first == std::string::npos ? std::string {} : s.substr (first);
          c13 = "CQ_" + s;
        }
      else
        {
          c13.clear ();
          success = false;
        }
    }
  else
    {
      int n = n28 - kNTokens;
      if (n < kMax22)
        {
          c13 = "<...>";
        }
      else
        {
          static std::string const c1 = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
          static std::string const c2 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
          static std::string const c3 = "0123456789";
          static std::string const c4 = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";
          n -= kMax22;
          int const i1 = n / (36 * 10 * 27 * 27 * 27);
          n -= 36 * 10 * 27 * 27 * 27 * i1;
          int const i2 = n / (10 * 27 * 27 * 27);
          n -= 10 * 27 * 27 * 27 * i2;
          int const i3 = n / (27 * 27 * 27);
          n -= 27 * 27 * 27 * i3;
          int const i4 = n / (27 * 27);
          n -= 27 * 27 * i4;
          int const i5 = n / 27;
          int const i6 = n - 27 * i5;
          // Il Fortran leggerebbe oltre la fine della tabella: qui e' un
          // nominativo che non esiste, e basta.
          if (i1 > 36) return false;
          std::string s {c1[i1], c2[i2], c3[i3], c4[i4], c4[i5], c4[i6]};
          std::size_t const first = s.find_first_not_of (' ');
          s = first == std::string::npos ? std::string {} : s.substr (first);
          c13 = s;
          if (!callok (c13))
            {
              c13 = "QU1RK";
              success = false;
            }
        }
    }
  std::string const padded = fixed (c13, 13);
  int const i0 = findex (padded, ' ');
  if (i0 != 0 && i0 < len_trim (padded))
    {
      c13 = "QU1RK";
      success = false;
    }
  return success;
}

bool valid_number (int kind, int value)
{
  bool ok = kind >= 0 && kind <= 7 && value >= 0 && value <= 131071;
  if (!ok) return false;
  if (kind == NUM_CQ_ZONE) ok = value >= 1 && value <= 40;
  if (kind == NUM_ITU_ZONE) ok = value >= 1 && value <= 90;
  if (kind == NUM_LICENSE_YEAR) ok = value <= 9999;
  return ok;
}

bool valid_role (int role)
{
  return role == ROLE_FIELD_ONLY || role == ROLE_FULL;
}

bool pack_base36 (std::string const& token, I8& value)
{
  value = 0;
  if (token.size () != 2 && token.size () != 3) return false;
  for (char const c : token)
    {
      char const* p = std::strchr (kBase36, c);
      if (!p || c == '\0') return false;
      value = 36 * value + (p - kBase36);
    }
  if (token.size () == 3 && value < 36 * 36) return false;
  return true;
}

std::string unpack_base36 (I8 value, int n)
{
  std::string token (kAtomTextLength, ' ');
  I8 work = value;
  for (int i = n; i >= 1; --i)
    {
      int const digit = static_cast<int> (work % 36);
      work /= 36;
      token[static_cast<std::size_t> (i - 1)] = kBase36[digit];
    }
  return token;
}

bool grid4_to_index (std::string const& grid, int& index_value)
{
  index_value = 0;
  if (len_trim (grid) != 4) return false;
  int const a = at (grid, 1) - 'A';
  int const b = at (grid, 2) - 'A';
  int const c = at (grid, 3) - '0';
  int const d = at (grid, 4) - '0';
  bool const ok = a >= 0 && a < 18 && b >= 0 && b < 18 && c >= 0 && c < 10 && d >= 0 && d < 10;
  if (ok) index_value = ((a * 18 + b) * 10 + c) * 10 + d;
  return ok;
}

std::string index_to_grid4 (int index_value)
{
  int work = index_value;
  int const d = work % 10;
  work /= 10;
  int const c = work % 10;
  work /= 10;
  int const b = work % 18;
  int const a = work / 18;
  std::string grid (kAtomTextLength, ' ');
  grid[0] = static_cast<char> ('A' + a);
  grid[1] = static_cast<char> ('A' + b);
  grid[2] = static_cast<char> ('0' + c);
  grid[3] = static_cast<char> ('0' + d);
  return grid;
}

void finish_struct (I8 body, int family, I8& n32)
{
  n32 = 4 * (8 * body + family) + 2;
}

std::string render_number (int kind, int value)
{
  char buf[32];
  switch (kind)
    {
    case NUM_SERIAL:
      if (value < 1000) std::snprintf (buf, sizeof buf, "%03d", value);
      else std::snprintf (buf, sizeof buf, "%d", value);
      break;
    case NUM_CQ_ZONE:
    case NUM_ITU_ZONE:
    case NUM_CHECK:
      if (value < 100) std::snprintf (buf, sizeof buf, "%02d", value);
      else std::snprintf (buf, sizeof buf, "%d", value);
      break;
    case NUM_LICENSE_YEAR:
      std::snprintf (buf, sizeof buf, "%04d", value);
      break;
    default:
      std::snprintf (buf, sizeof buf, "%d", value);
      break;
    }
  return buf;
}

std::string render_role (int role, std::string const& field)
{
  return role == ROLE_FULL ? "599 " + field : field;
}

// decimal_value di pack_jtty: da 1 a 6 cifre, al piu' 131071.
bool decimal_value (std::string const& text, int& value)
{
  value = 0;
  if (text.size () < 1 || text.size () > 6) return false;
  for (char const c : text)
    if (!is_digit (c)) return false;
  for (char const c : text) value = 10 * value + (c - '0');
  return value <= 131071;
}

}

char const* alphabet () { return kAlphabet; }

int source_index (char c)
{
  if (c == '\0') return -1;
  char const* p = std::strchr (kAlphabet, c);
  return p ? static_cast<int> (p - kAlphabet) : -1;
}

char source_char (int index)
{
  return index >= 0 && index < 64 ? kAlphabet[index] : ' ';
}

char const* control_text (int phrase)
{
  return phrase >= 0 && phrase < kControlPhraseCount ? kControlText[phrase] : "";
}

int arrl_section_count () { return kArrlSectionCount; }

char const* arrl_section (int index1)
{
  return index1 >= 1 && index1 <= kArrlSectionCount ? kArrlSections[index1 - 1] : "";
}

// pack77_arrl_section_index: confronta le prime tre colonne, quindi un
// argomento di due caratteri non trova mai niente (come nel Fortran).
int arrl_section_index (std::string const& section)
{
  if (section.size () < 3) return -1;
  std::string const key = rtrim (section.substr (0, 3));
  for (int i = 0; i < kArrlSectionCount; ++i)
    if (key == kArrlSections[i]) return i + 1;
  return -1;
}

std::string rtrim (std::string const& s)
{
  std::size_t const last = s.find_last_not_of (' ');
  return last == std::string::npos ? std::string {} : s.substr (0, last + 1);
}

int len_trim (std::string const& s)
{
  return static_cast<int> (rtrim (s).size ());
}

std::string fixed (std::string const& s, std::size_t length)
{
  std::string r = s.substr (0, std::min (s.size (), length));
  r.resize (length, ' ');
  return r;
}

void frame_to_payload (Frame frame, int payload[kPayloadBits])
{
  for (int i = 0; i < kPayloadBits; ++i)
    payload[i] = static_cast<int> ((frame >> (kPayloadBits - 1 - i)) & 1u);
}

Frame payload_to_frame (int const payload[kPayloadBits])
{
  Frame f = 0;
  for (int i = 0; i < kPayloadBits; ++i)
    f = (f << 1) | static_cast<Frame> (payload[i] & 1);
  return f;
}

// jtty_mod.f90 jtty_standard_call / jtty_source_codec.f90 standard_call
bool standard_call (std::string const& call)
{
  std::string const work = fixed (call, 13);
  if (!chkcall (work)) return false;
  if (findex (work, '/') > 0) return false;
  if (at (work, 1) == 'Q') return false;
  for (char const c : rtrim (work))
    if (!is_upper (c) && !is_digit (c)) return false;
  int const n28 = pack28_standard (work);
  if (n28 < 0) return false;
  std::string unpacked;
  if (!unpack28 (n28, unpacked)) return false;
  return rtrim (unpacked) == rtrim (work);
}

Atom call_atom (int action, std::string const& call)
{
  Atom a;
  a.kind = ATOM_CALL;
  a.subtype = action;
  a.text = fixed (call, kAtomTextLength);
  return a;
}

Atom exch_num_atom (int role, int kind, int value)
{
  Atom a;
  a.kind = ATOM_EXCH_NUM;
  a.role = role;
  a.subtype = kind;
  a.value = value;
  return a;
}

Atom exch_loc_atom (int role, int kind, std::string const& token)
{
  Atom a;
  a.kind = ATOM_EXCH_LOC;
  a.role = role;
  a.subtype = kind;
  a.text = fixed (token, kAtomTextLength);
  return a;
}

Atom zone_loc_atom (int zone, std::string const& token)
{
  Atom a;
  a.kind = ATOM_EXCH_PAIR;
  a.subtype = PAIR_ZONE_LOC3;
  a.value = zone;
  a.text = fixed (token, kAtomTextLength);
  return a;
}

Atom class_section_atom (int count, std::string const& cls, int section_index)
{
  Atom a;
  a.kind = ATOM_EXCH_PAIR;
  a.subtype = PAIR_CLASS_SECTION;
  a.value = count;
  a.value2 = section_index;
  a.text = fixed (cls, kAtomTextLength);
  return a;
}

Atom exch_num_time_atom (int role, int serial, int minute_of_day)
{
  Atom a;
  a.kind = ATOM_EXCH_NUM_TIME;
  a.role = role;
  a.value = serial;
  a.value2 = minute_of_day;
  return a;
}

Atom control_atom (int phrase)
{
  Atom a;
  a.kind = ATOM_CONTROL;
  a.subtype = phrase;
  return a;
}

Atom grid4_atom (int role, std::string const& grid)
{
  Atom a;
  a.kind = ATOM_GRID4;
  a.role = role;
  a.text = fixed (grid, kAtomTextLength);
  return a;
}

Atom text5_atom (std::string const& text)
{
  Atom a;
  a.kind = ATOM_TEXT5;
  a.text = fixed (text.substr (0, std::min<std::size_t> (5, text.size ())), kAtomTextLength);
  return a;
}

bool pack_atom (Atom const& atom, bool eom, Frame& frame)
{
  frame = 0;
  I8 n32 = 0;
  switch (atom.kind)
    {
    case ATOM_CALL:
      {
        if (atom.subtype < 0 || atom.subtype > 5) return false;
        std::string const call_text = fixed (atom.text, 13);
        if (!standard_call (call_text)) return false;
        int const n28 = pack28_standard (call_text);
        std::string roundtrip;
        if (n28 < 0 || !unpack28 (n28, roundtrip) || rtrim (roundtrip) != rtrim (call_text))
          return false;
        int i2, n2;
        if (atom.subtype <= 3)
          {
            i2 = 0;
            n2 = atom.subtype;
          }
        else
          {
            i2 = 1;
            n2 = atom.subtype - 4;
          }
        n32 = (static_cast<I8> (n28) << 4) + 4 * n2 + i2;
        break;
      }
    case ATOM_EXCH_NUM:
      {
        if (!valid_role (atom.role) || !valid_number (atom.subtype, atom.value)) return false;
        I8 const body = (static_cast<I8> (atom.role) << 26) + (static_cast<I8> (atom.subtype) << 22)
            + (static_cast<I8> (atom.value) << 5);
        finish_struct (body, 0, n32);
        break;
      }
    case ATOM_EXCH_LOC:
      {
        if (!valid_role (atom.role) || atom.subtype < 0 || atom.subtype > 4) return false;
        int const n = len_trim (atom.text);
        if (n != 2 && n != 3) return false;
        I8 token_value;
        if (!pack_base36 (sub (atom.text, 1, n), token_value)) return false;
        I8 const body = (static_cast<I8> (atom.role) << 26) + (static_cast<I8> (atom.subtype) << 22)
            + (static_cast<I8> (n - 2) << 21) + (token_value << 5);
        finish_struct (body, 1, n32);
        break;
      }
    case ATOM_EXCH_PAIR:
      {
        I8 pair_data;
        switch (atom.subtype)
          {
          case PAIR_ZONE_LOC3:
            {
              if (atom.value < 1 || atom.value > 40) return false;
              int const n = len_trim (atom.text);
              if (n != 2 && n != 3) return false;
              I8 token_value;
              if (!pack_base36 (sub (atom.text, 1, n), token_value)) return false;
              pair_data = (static_cast<I8> (atom.value) << 17) + (static_cast<I8> (n - 2) << 16) + token_value;
              break;
            }
          case PAIR_CLASS_SECTION:
            {
              if (atom.value < 1 || atom.value > 32) return false;
              if (atom.value2 < 1 || atom.value2 > kArrlSectionCount || len_trim (atom.text) != 1)
                return false;
              int const class_index = at (atom.text, 1) - 'A';
              if (class_index < 0 || class_index > 5) return false;
              pair_data = (static_cast<I8> (atom.value) << 17) + (static_cast<I8> (class_index) << 14)
                  + (static_cast<I8> (atom.value2) << 7);
              break;
            }
          default:
            return false;
          }
        I8 const body = (static_cast<I8> (atom.subtype) << 24) + (pair_data << 1);
        finish_struct (body, 2, n32);
        break;
      }
    case ATOM_EXCH_NUM_TIME:
      {
        if (!valid_role (atom.role) || atom.value < 0 || atom.value > 16383) return false;
        if (atom.value2 < 0 || atom.value2 > 1439) return false;
        I8 const body = (static_cast<I8> (atom.role) << 26) + (static_cast<I8> (atom.value) << 12)
            + (static_cast<I8> (atom.value2) << 1);
        finish_struct (body, 3, n32);
        break;
      }
    case ATOM_CONTROL:
      {
        if (atom.subtype < 0 || atom.subtype > 17) return false;
        I8 const body = (I8 {0} << 23) + (static_cast<I8> (atom.subtype) << 16);
        finish_struct (body, 4, n32);
        break;
      }
    case ATOM_GRID4:
      {
        if (!valid_role (atom.role)) return false;
        int grid_index;
        if (!grid4_to_index (atom.text, grid_index)) return false;
        I8 const data = (static_cast<I8> (atom.role) << 22) + (static_cast<I8> (grid_index) << 7);
        I8 const body = (I8 {1} << 23) + data;
        finish_struct (body, 4, n32);
        break;
      }
    case ATOM_TEXT5:
      {
        I8 top30 = 0;
        for (int i = 1; i <= 5; ++i)
          {
            int const idx = source_index (at (atom.text, i));
            if (idx < 0) return false;
            top30 = 64 * top30 + idx;
          }
        n32 = 4 * top30 + 3;
        break;
      }
    default:
      return false;
    }
  // write(frame(1:32),'(b32.32)') n32; frame(33)='0'; frame(34)=eom
  frame = (static_cast<Frame> (n32 & 0xFFFFFFFFll) << 2) | (eom ? 1u : 0u);
  return true;
}

bool unpack_atom (Frame frame, Atom& atom, bool& eom)
{
  atom = Atom {};
  eom = false;
  if ((frame >> 1) & 1u) return false;           // bit riservato
  I8 const n32 = static_cast<I8> ((frame >> 2) & 0xFFFFFFFFull);
  if (n32 == 0) return false;
  I8 const top30 = n32 >> 2;
  int const i2 = static_cast<int> (n32 & 3);
  switch (i2)
    {
    case 0:
    case 1:
      {
        int const n28 = static_cast<int> (n32 >> 4);
        int const n2 = static_cast<int> ((n32 >> 2) & 3);
        if (i2 == 1 && n2 > 1) return false;
        std::string call_text;
        bool const success = unpack28 (n28, call_text);
        if (!success || !standard_call (call_text)) return false;
        atom = call_atom (i2 == 0 ? n2 : n2 + 4, call_text);
        break;
      }
    case 2:
      {
        int const family = static_cast<int> (top30 & 7);
        I8 const body = top30 >> 3;
        switch (family)
          {
          case 0:
            {
              atom.kind = ATOM_EXCH_NUM;
              atom.role = static_cast<int> (ibits (body, 26, 1));
              atom.subtype = static_cast<int> (ibits (body, 22, 4));
              atom.value = static_cast<int> (ibits (body, 5, 17));
              int const zero = static_cast<int> (ibits (body, 0, 5));
              if (zero != 0 || !valid_number (atom.subtype, atom.value)) return false;
              break;
            }
          case 1:
            {
              atom.kind = ATOM_EXCH_LOC;
              atom.role = static_cast<int> (ibits (body, 26, 1));
              atom.subtype = static_cast<int> (ibits (body, 22, 4));
              int const n = static_cast<int> (ibits (body, 21, 1)) + 2;
              I8 const token_value = ibits (body, 5, 16);
              int const zero = static_cast<int> (ibits (body, 0, 5));
              I8 const limit = n == 2 ? 36 * 36 : 36 * 36 * 36;
              if (zero != 0 || atom.subtype > 4 || token_value >= limit) return false;
              if (n == 3 && token_value < 36 * 36) return false;
              atom.text = unpack_base36 (token_value, n);
              break;
            }
          case 2:
            {
              atom.kind = ATOM_EXCH_PAIR;
              atom.subtype = static_cast<int> (ibits (body, 24, 3));
              I8 const pair_data = ibits (body, 1, 23);
              if (ibits (body, 0, 1) != 0) return false;
              switch (atom.subtype)
                {
                case PAIR_ZONE_LOC3:
                  {
                    atom.value = static_cast<int> (ibits (pair_data, 17, 6));
                    int const n = static_cast<int> (ibits (pair_data, 16, 1)) + 2;
                    I8 const token_value = ibits (pair_data, 0, 16);
                    I8 const limit = n == 2 ? 36 * 36 : 36 * 36 * 36;
                    if (atom.value < 1 || atom.value > 40 || token_value >= limit) return false;
                    if (n == 3 && token_value < 36 * 36) return false;
                    atom.text = unpack_base36 (token_value, n);
                    break;
                  }
                case PAIR_CLASS_SECTION:
                  {
                    atom.value = static_cast<int> (ibits (pair_data, 17, 6));
                    int const class_index = static_cast<int> (ibits (pair_data, 14, 3));
                    atom.value2 = static_cast<int> (ibits (pair_data, 7, 7));
                    int const zero = static_cast<int> (ibits (pair_data, 0, 7));
                    if (atom.value < 1 || atom.value > 32 || class_index > 5) return false;
                    if (atom.value2 < 1 || atom.value2 > kArrlSectionCount || zero != 0) return false;
                    atom.text = fixed (std::string (1, static_cast<char> ('A' + class_index)),
                                       kAtomTextLength);
                    break;
                  }
                default:
                  return false;
                }
              break;
            }
          case 3:
            {
              atom.kind = ATOM_EXCH_NUM_TIME;
              atom.role = static_cast<int> (ibits (body, 26, 1));
              atom.value = static_cast<int> (ibits (body, 12, 14));
              atom.value2 = static_cast<int> (ibits (body, 1, 11));
              int const zero = static_cast<int> (ibits (body, 0, 1));
              if (zero != 0 || atom.value2 > 1439) return false;
              break;
            }
          case 4:
            {
              int const i = static_cast<int> (ibits (body, 23, 4));
              I8 const data = ibits (body, 0, 23);
              switch (i)
                {
                case 0:
                  {
                    atom.kind = ATOM_CONTROL;
                    atom.subtype = static_cast<int> (ibits (data, 16, 7));
                    int const zero = static_cast<int> (ibits (data, 0, 16));
                    if (zero != 0 || atom.subtype > 17) return false;
                    break;
                  }
                case 1:
                  {
                    atom.kind = ATOM_GRID4;
                    atom.role = static_cast<int> (ibits (data, 22, 1));
                    int const grid_index = static_cast<int> (ibits (data, 7, 15));
                    int const zero = static_cast<int> (ibits (data, 0, 7));
                    if (zero != 0 || grid_index >= 32400) return false;
                    atom.text = index_to_grid4 (grid_index);
                    break;
                  }
                default:
                  return false;
                }
              break;
            }
          default:
            return false;
          }
        break;
      }
    case 3:
      {
        atom.kind = ATOM_TEXT5;
        std::string t (kAtomTextLength, ' ');
        for (int i = 1; i <= 5; ++i)
          t[static_cast<std::size_t> (i - 1)] = source_char (static_cast<int> ((top30 >> (6 * (5 - i))) & 63));
        atom.text = t;
        break;
      }
    }
  eom = (frame & 1u) != 0;
  return true;
}

bool render_atom (Atom const& atom, std::string& text)
{
  text.clear ();
  switch (atom.kind)
    {
    case ATOM_CALL:
      {
        std::string const call = rtrim (atom.text);
        switch (atom.subtype)
          {
          case CALL_CQ: text = "CQ " + call + " CQ"; break;
          case CALL_CALL: text = call; break;
          case CALL_TU_CQ: text = "TU " + call + " CQ"; break;
          case CALL_CALL_TU: text = call + " TU"; break;
          case CALL_CALL_AGN: text = call + " AGN?"; break;
          case CALL_TU_NOW: text = "TU NOW " + call; break;
          default: return false;
          }
        break;
      }
    case ATOM_EXCH_NUM:
      if (!valid_role (atom.role) || !valid_number (atom.subtype, atom.value)) return false;
      text = render_role (atom.role, render_number (atom.subtype, atom.value));
      break;
    case ATOM_EXCH_LOC:
      if (atom.subtype < 0 || atom.subtype > 4) return false;
      text = render_role (atom.role, rtrim (atom.text));
      break;
    case ATOM_EXCH_PAIR:
      switch (atom.subtype)
        {
        case PAIR_ZONE_LOC3:
          {
            char buf[16];
            std::snprintf (buf, sizeof buf, "%02d", atom.value);
            text = std::string ("599 ") + buf + " " + rtrim (atom.text);
            break;
          }
        case PAIR_CLASS_SECTION:
          {
            if (atom.value2 < 1 || atom.value2 > kArrlSectionCount) return false;
            char buf[32];
            std::snprintf (buf, sizeof buf, "%d%c", atom.value, at (atom.text, 1));
            text = std::string (buf) + " " + kArrlSections[atom.value2 - 1];
            break;
          }
        default:
          return false;
        }
      break;
    case ATOM_EXCH_NUM_TIME:
      {
        if (!valid_role (atom.role) || atom.value < 0 || atom.value > 16383 || atom.value2 < 0
            || atom.value2 > 1439)
          return false;
        char buf[16];
        std::snprintf (buf, sizeof buf, "%02d%02d", atom.value2 / 60, atom.value2 % 60);
        text = render_role (atom.role, render_number (NUM_SERIAL, atom.value) + " " + buf);
        break;
      }
    case ATOM_CONTROL:
      if (atom.subtype < 0 || atom.subtype > 17) return false;
      text = kControlText[atom.subtype];
      break;
    case ATOM_GRID4:
      text = render_role (atom.role, sub (atom.text, 1, 4));
      break;
    case ATOM_TEXT5:
      text = sub (atom.text, 1, 5);
      break;
    default:
      return false;
    }
  return true;
}

std::string normalize_message (std::string const& raw)
{
  std::string const src = fixed (raw, kMessageLength);
  std::string normalized;
  bool last_space = true;
  for (char c : src)
    {
      if (c == '\0') c = ' ';
      if (c == '~') c = ' ';
      if (c >= 'a' && c <= 'z') c = static_cast<char> (c - 32);
      if (source_index (c) < 0) c = '#';
      if (c == ' ')
        {
          if (last_space) continue;
          normalized.push_back (' ');
          last_space = true;
        }
      else
        {
          normalized.push_back (c);
          last_space = false;
        }
    }
  return fixed (normalized, kMessageLength);
}

bool pack_atoms (std::vector<Atom> const& atoms, std::vector<Frame>& frames)
{
  frames.clear ();
  int const natoms = static_cast<int> (atoms.size ());
  if (natoms < 1 || natoms > kMaxFrames) return false;
  for (int i = 0; i < natoms; ++i)
    {
      Frame f;
      if (!pack_atom (atoms[static_cast<std::size_t> (i)], i == natoms - 1, f))
        {
          frames.clear ();
          return false;
        }
      frames.push_back (f);
    }
  return true;
}

int pack_message (std::string const& message, int exchange_profile,
                  std::vector<Frame>& frames, std::string* canonical)
{
  constexpr int INF = 999;
  frames.clear ();
  std::string msg = normalize_message (message);
  if (canonical) *canonical = rtrim (msg);
  int const profile = exchange_profile;
  if (profile < EXCHANGE_UNKNOWN || profile > EXCHANGE_RTTY) return -1;

  if (profile == EXCHANGE_RTTY)
    {
      // normalize_serials: "599 05" -> "599 005", "599 0123" -> "599 123".
      std::string result;
      int first = 1;
      int const length = len_trim (msg);
      while (first <= length)
        {
          std::string const rest = sub (msg, first, length);
          int last = findex (rest, ' ');
          if (last == 0) last = length;
          else last = first + last - 2;
          std::string rendered = sub (msg, first, last);
          int next = last + 2;
          if (rendered == "599" && next <= length)
            {
              std::string const rest2 = sub (msg, next, length);
              int last_field = findex (rest2, ' ');
              if (last_field == 0) last_field = length;
              else last_field = next + last_field - 2;
              int value;
              if (decimal_value (sub (msg, next, last_field), value))
                {
                  if (!render_atom (exch_num_atom (ROLE_FULL, NUM_SERIAL, value), rendered))
                    return -1;
                  next = last_field + 2;
                }
            }
          if (first == 1) result = rtrim (rendered);
          else result = rtrim (result) + " " + rtrim (rendered);
          if (len_trim (result) > kMessageLength) return -1;
          first = next;
        }
      msg = fixed (result, kMessageLength);
    }
  if (canonical) *canonical = rtrim (msg);

  int const n = len_trim (msg);
  if (n <= 0) return 0;

  std::vector<int> dp (static_cast<std::size_t> (n + 2), INF);
  std::vector<int> successor (static_cast<std::size_t> (n + 1), 0);
  std::vector<Atom> choice (static_cast<std::size_t> (n + 1));
  dp[static_cast<std::size_t> (n + 1)] = 0;
  int ipos = 0;

  auto consider = [&] (Atom const& atom, int next) {
    int const cost = 1 + dp[static_cast<std::size_t> (next)];
    if (cost > kMaxFrames || cost > dp[static_cast<std::size_t> (ipos)]) return;
    Atom const& best = choice[static_cast<std::size_t> (ipos)];
    int const rank = atom.kind == ATOM_TEXT5 ? 1 : 0;
    int const best_rank = best.kind == ATOM_TEXT5 ? 1 : 0;
    int const key = 100 * atom.kind + 2 * atom.subtype + atom.role;
    int const best_key = 100 * best.kind + 2 * best.subtype + best.role;
    if (cost == dp[static_cast<std::size_t> (ipos)])
      {
        if (rank > best_rank) return;
        if (rank == best_rank)
          {
            if (next < successor[static_cast<std::size_t> (ipos)]) return;
            if (next == successor[static_cast<std::size_t> (ipos)] && key >= best_key) return;
          }
      }
    dp[static_cast<std::size_t> (ipos)] = cost;
    successor[static_cast<std::size_t> (ipos)] = next;
    choice[static_cast<std::size_t> (ipos)] = atom;
  };

  auto offer = [&] (Atom const& atom) {
    Frame frame;
    if (!pack_atom (atom, false, frame)) return;
    Atom decoded;
    bool eom;
    if (!unpack_atom (frame, decoded, eom)) return;
    std::string rendered;
    if (!render_atom (decoded, rendered)) return;
    int const length = len_trim (rendered);
    if (length == 0) return;
    int const last = ipos + length - 1;
    if (last > n) return;
    if (sub (msg, ipos, last) != rendered.substr (0, static_cast<std::size_t> (length))) return;
    int next = n + 1;
    if (last < n)
      {
        if (at (msg, last + 1) != ' ') return;
        next = last + 2;
      }
    consider (atom, next);
  };

  auto try_compact = [&] () {
    std::string words[3];
    int first = ipos;
    int nwords = 0;
    for (int j = 1; j <= 3; ++j)
      {
        if (first > n) break;
        int last = findex (sub (msg, first, n), ' ');
        if (last == 0) last = n;
        else last = first + last - 2;
        nwords = j;
        words[j - 1] = sub (msg, first, last);
        first = last + 2;
      }

    for (int j = 1; j <= nwords; ++j)
      {
        if (len_trim (words[j - 1]) > kAtomTextLength) continue;
        for (int action = CALL_CQ; action <= CALL_TU_NOW; ++action)
          offer (call_atom (action, rtrim (words[j - 1])));
      }
    for (int j = 0; j < kControlPhraseCount; ++j) offer (control_atom (j));

    for (int role = ROLE_FIELD_ONLY; role <= ROLE_FULL; ++role)
      {
        int field = 1;
        if (role == ROLE_FULL)
          {
            if (rtrim (words[0]) != "599" || nwords < 2) continue;
            field = 2;
          }
        std::string const w = rtrim (words[field - 1]);
        int const length = static_cast<int> (w.size ());
        int value;
        bool const numeric = decimal_value (w, value);
        if (numeric) offer (exch_num_atom (role, NUM_GENERIC, value));
        if (numeric && role == ROLE_FULL && profile == EXCHANGE_RTTY)
          offer (exch_num_atom (role, NUM_SERIAL, value));
        if (length == 4) offer (grid4_atom (role, w.substr (0, 4)));
        if (role != ROLE_FULL || length < 2 || length > 3) continue;
        if (w.find_first_of ("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos) continue;
        offer (exch_loc_atom (role, LOC_QTH, w));
        if (profile == EXCHANGE_RTTY) offer (exch_loc_atom (role, LOC_STATE_PROVINCE, w));
      }

    std::string const w1 = rtrim (words[0]);
    int const length = static_cast<int> (w1.size ());
    if (nwords < 2 || length < 2 || length > 3) return;
    int value;
    if (!decimal_value (w1.substr (0, static_cast<std::size_t> (length - 1)), value)) return;
    int const section_index = arrl_section_index (rtrim (words[1]));
    offer (class_section_atom (value, w1.substr (static_cast<std::size_t> (length - 1), 1),
                               section_index));
  };

  for (ipos = n; ipos >= 1; --ipos)
    {
      int const inext = std::min (n + 1, ipos + 5);
      consider (text5_atom (sub (msg, ipos, inext - 1)), inext);
      if (ipos > 1 && at (msg, ipos - 1) != ' ') continue;
      try_compact ();
    }

  if (dp[1] > kMaxFrames) return -1;
  std::vector<Atom> atoms;
  ipos = 1;
  while (ipos <= n)
    {
      atoms.push_back (choice[static_cast<std::size_t> (ipos)]);
      ipos = successor[static_cast<std::size_t> (ipos)];
      if (static_cast<int> (atoms.size ()) > kMaxFrames) return -1;
    }
  if (!pack_atoms (atoms, frames))
    {
      frames.clear ();
      return -1;
    }
  return static_cast<int> (frames.size ());
}

int pack_native_atoms (NativeAtomDescriptor const* c_atoms, int natoms, std::vector<Frame>& frames)
{
  frames.clear ();
  if (natoms < 1 || natoms > kMaxFrames) return ENCODE_INVALID_DESCRIPTOR;
  std::vector<Atom> atoms;
  for (int i = 0; i < natoms; ++i)
    {
      NativeAtomDescriptor const& c = c_atoms[i];
      if (c.reserved != 0) return ENCODE_INVALID_DESCRIPTOR;
      // unpack_descriptor_text: al piu' 8 caratteri, terminati da NUL.
      std::string descriptor_text;
      bool text_valid = false;
      for (int j = 0; j < 9; ++j)
        {
          if (c.text[j] == '\0')
            {
              text_valid = true;
              break;
            }
          if (j >= 8) break;
          descriptor_text.push_back (c.text[j]);
        }
      if (!text_valid) return ENCODE_INVALID_DESCRIPTOR;
      descriptor_text = fixed (descriptor_text, kAtomTextLength);
      switch (c.kind)
        {
        case ATOM_CALL:
          if (c.role != 0 || c.value != 0) return ENCODE_INVALID_DESCRIPTOR;
          atoms.push_back (call_atom (c.subtype, descriptor_text));
          break;
        case ATOM_EXCH_NUM:
          if (len_trim (descriptor_text) != 0) return ENCODE_INVALID_DESCRIPTOR;
          atoms.push_back (exch_num_atom (c.role, c.subtype, c.value));
          break;
        case ATOM_EXCH_LOC:
          if (c.value != 0) return ENCODE_INVALID_DESCRIPTOR;
          atoms.push_back (exch_loc_atom (c.role, c.subtype, descriptor_text));
          break;
        case ATOM_EXCH_PAIR:
          {
            if (c.subtype != 1) return ENCODE_INVALID_DESCRIPTOR;
            if (c.role < 0 || c.role > 5) return ENCODE_INVALID_DESCRIPTOR;
            int const section_index = arrl_section_index (descriptor_text);
            if (section_index < 1) return ENCODE_UNKNOWN_SECTION;
            atoms.push_back (class_section_atom (c.value, std::string (1, static_cast<char> ('A' + c.role)),
                                                 section_index));
            break;
          }
        case ATOM_CONTROL:
          if (c.role != 0 || c.value != 0 || len_trim (descriptor_text) != 0)
            return ENCODE_INVALID_DESCRIPTOR;
          atoms.push_back (control_atom (c.subtype));
          break;
        case ATOM_GRID4:
          if (c.subtype != 0 || c.value != 0) return ENCODE_INVALID_DESCRIPTOR;
          atoms.push_back (grid4_atom (c.role, descriptor_text));
          break;
        default:
          return ENCODE_INVALID_DESCRIPTOR;
        }
    }
  if (!pack_atoms (atoms, frames)) return ENCODE_INVALID_DESCRIPTOR;
  return ENCODE_OK;
}

UnpackResult unpack_frames (Frame const* frames, int nframes)
{
  UnpackResult r;
  r.message = std::string (kMessageLength, ' ');
  int k = 1;
  bool last_frame_sep = false;
  bool last_frame_flag = false;
  bool all_valid = nframes >= 1 && nframes <= kMaxFrames;
  int const count = std::max (0, std::min (nframes, kMaxFrames));
  auto put = [&] (char c) {
    if (k <= kMessageLength) r.message[static_cast<std::size_t> (k - 1)] = c;
    ++k;
  };
  for (int iframe = 0; iframe < count; ++iframe)
    {
      last_frame_sep = false;
      last_frame_flag = false;
      Atom atom;
      bool eom;
      if (!unpack_atom (frames[iframe], atom, eom))
        {
          all_valid = false;
          continue;
        }
      std::string rendered;
      if (!render_atom (atom, rendered))
        {
          all_valid = false;
          continue;
        }
      last_frame_flag = eom;
      if (atom.kind == ATOM_TEXT5)
        {
          std::uint64_t const n30 = (frames[iframe] >> 4) & 0x3FFFFFFFull;
          for (int j = 1; j <= 5; ++j)
            {
              int const idx = static_cast<int> ((n30 >> (6 * (5 - j))) & 63);
              char c = source_char (idx);
              if (c == ' ') c = '~';
              put (c);
            }
        }
      else
        {
          for (char const c : rtrim (rendered)) put (c);
          ++k;                                   // separatore implicito
          last_frame_sep = true;
        }
    }
  r.trailing_sep = last_frame_sep;
  r.is_last_frame = last_frame_flag && all_valid;
  r.source_valid = all_valid;
  return r;
}

std::string display_message_text (std::string const& decoded)
{
  std::string msg = fixed (decoded, kMessageLength);
  int const n1 = len_trim (msg);
  for (int i = 1; i <= n1 - 4; ++i)
    if (msg.compare (static_cast<std::size_t> (i - 1), 5, "~~~~~") == 0)
      msg.replace (static_cast<std::size_t> (i - 1), 5, " ... ");
  int const n2 = len_trim (msg);
  for (int i = 0; i < n2; ++i)
    if (msg[static_cast<std::size_t> (i)] == '~') msg[static_cast<std::size_t> (i)] = ' ';
  if (msg[0] == ' ') msg = fixed (rtrim (msg.substr (1)), kMessageLength);
  return msg;
}

}
}
