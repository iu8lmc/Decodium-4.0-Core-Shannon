#include <cstddef>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlogical-op-parentheses"
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#endif

namespace {

struct StringSlice
{
  char const* data;
  std::size_t size;
};

inline char ch(StringSlice s, int pos)
{
  return (pos >= 1 && static_cast<std::size_t>(pos) <= s.size) ? s.data[static_cast<std::size_t>(pos - 1)] : ' ';
}

inline std::size_t len_trim(StringSlice s)
{
  while (s.size != 0 && s.data[s.size - 1] == ' ')
    {
      --s.size;
    }
  return s.size;
}

inline StringSlice slice(StringSlice s, int start, int end)
{
  if (start < 1 || end < start) return {nullptr, 0};
  std::size_t const begin = static_cast<std::size_t>(start - 1);
  std::size_t const len = static_cast<std::size_t>(end - start + 1);
  if (begin >= s.size) return {nullptr, 0};
  std::size_t const available = s.size - begin;
  return {s.data + begin, len < available ? len : available};
}

inline bool operator== (StringSlice lhs, char const* rhs)
{
  if (!rhs)
    {
      return lhs.data == nullptr && lhs.size == 0;
    }

  std::size_t rhs_size = 0;
  while (rhs[rhs_size] != '\0')
    {
      ++rhs_size;
    }

  if (lhs.size != rhs_size)
    {
      return false;
    }

  for (std::size_t i = 0; i < lhs.size; ++i)
    {
      if (lhs.data[i] != rhs[i])
        {
          return false;
        }
    }

  return true;
}

inline bool operator!= (StringSlice lhs, char const* rhs)
{
  return !(lhs == rhs);
}

void ft8var_chkgrid_impl(StringSlice callsign, StringSlice grid, bool& lchkcall, bool& lgvalid, bool& lwrongcall)
{
  lchkcall = false;
  lgvalid = false;
  lwrongcall = false;
  if (ch(grid, 1) > '@' && ch(grid, 1) < 'J') {
    if (ch(grid, 1) == 'A' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'E') || ch(grid, 2) == 'M' || ch(grid, 2) == 'N' || ch(grid, 2) == 'Q' || ch(grid, 2) == 'R')) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'B' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'G') || ch(grid, 2) == 'M' || ch(grid, 2) == 'N' || ch(grid, 2) == 'Q' || ch(grid, 2) == 'R')) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'C' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'G') || (ch(grid, 2) == 'G' && ch(grid, 3) > '7' && ch(grid, 4) < '4') || (ch(grid, 2) == 'I' && ch(grid, 3) > '0' && ch(grid, 4) > '1') || (ch(grid, 2) > 'I' && ch(grid, 2) < 'M') || ch(grid, 2) == 'Q' || ch(grid, 2) == 'R')) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'D' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'G') || (ch(grid, 2) > 'G' && ch(grid, 2) < 'K') || (ch(grid, 2) == 'G' && slice(grid, 3, 4) != "52" && slice(grid, 3, 4) != "73") || ch(grid, 2) == 'Q' || ch(grid, 2) == 'R')) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'E' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'C') || (ch(grid, 2) == 'C' && slice(grid, 3, 4) != "41") || (ch(grid, 2) == 'F' && slice(grid, 3, 4) != "96") || (ch(grid, 2) > 'C' && ch(grid, 2) < 'F') || (ch(grid, 2) == 'G' && slice(grid, 3, 4) != "93") || ch(grid, 2) == 'H' || ch(grid, 2) == 'Q' || (ch(grid, 2) == 'R' && slice(grid, 3, 4) != "60"))) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'F' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'D') || (ch(grid, 2) == 'Q' && ch(grid, 3) < '3') || ch(grid, 2) == 'R')) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'G' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'C') || (ch(grid, 2) > 'K' && ch(grid, 2) < 'N') || (ch(grid, 2) == 'K' && ch(grid, 3) > '0') || (ch(grid, 2) == 'N' && ch(grid, 3) > '3') || (ch(grid, 2) == 'Q' && ch(grid, 4) > '4') || ch(grid, 2) == 'R')) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'H' && (ch(grid, 2) == 'A' || (ch(grid, 2) == 'B' && slice(grid, 3, 4) != "22") || ch(grid, 2) == 'C' || (ch(grid, 2) > 'D' && ch(grid, 2) < 'H') || ch(grid, 2) == 'J' || ch(grid, 2) == 'L' || ch(grid, 2) == 'N' || ch(grid, 2) == 'O' || (ch(grid, 2) == 'K' && ch(grid, 3) < '7') || (ch(grid, 2) == 'M' && ch(grid, 4) < '6') || (ch(grid, 2) == 'Q' && ch(grid, 4) > '0') || ch(grid, 2) == 'R')) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'I' && (ch(grid, 2) == 'A' || (ch(grid, 2) > 'B' && ch(grid, 2) < 'F') || (ch(grid, 2) == 'B' && slice(grid, 3, 4) != "59") || (ch(grid, 2) == 'F' && slice(grid, 3, 4) != "32") || ch(grid, 2) == 'G' || (ch(grid, 2) == 'H' && ch(grid, 3) != '7') || (ch(grid, 2) == 'I' && slice(grid, 3, 4) != "22") || (ch(grid, 2) == 'O' && ch(grid, 3) < '3') || (ch(grid, 2) == 'P' && ch(grid, 4) > '7') || (ch(grid, 2) == 'Q' && ch(grid, 4) > '1') || ch(grid, 2) == 'R')) {
      lchkcall = true;
    }
  } else if (ch(grid, 1) > 'I' && ch(grid, 1) < 'S') {
    if (ch(grid, 1) == 'J' && (ch(grid, 2) == 'C' || (ch(grid, 2) == 'A' && slice(grid, 3, 4) != "00") || (ch(grid, 2) == 'B' && slice(grid, 3, 4) != "59") || (ch(grid, 2) == 'D' && slice(grid, 3, 4) != "15") || ch(grid, 2) == 'E' || (ch(grid, 2) == 'F' && ch(grid, 3) < '8') || (ch(grid, 2) == 'G' && ch(grid, 3) < '6') || (ch(grid, 2) == 'H' && ch(grid, 3) < '5') || (ch(grid, 2) == 'I' && ch(grid, 3) < '2') || (ch(grid, 2) == 'R' && ch(grid, 4) > '0'))) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'K' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'C') || (ch(grid, 2) == 'C' && slice(grid, 3, 4) != "90") || ch(grid, 2) == 'D' || (ch(grid, 2) == 'E' && slice(grid, 3, 4) != "83" && slice(grid, 3, 4) != "93") || (ch(grid, 2) == 'F' && ch(grid, 4) < '5') || (ch(grid, 2) == 'R' && ch(grid, 4) > '0'))) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'L' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'E') || (ch(grid, 2) == 'E' && slice(grid, 3, 4) != "53" && slice(grid, 3, 4) != "54" && slice(grid, 3, 4) != "63") || ch(grid, 2) == 'F' || (ch(grid, 2) == 'G' && ch(grid, 4) < '4') || (ch(grid, 2) == 'J' && ch(grid, 3) > '5') || (ch(grid, 2) == 'Q' && ch(grid, 4) < '5' && ch(grid, 4) < '9') || (ch(grid, 2) == 'R' && ch(grid, 4) > '1'))) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'M' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'D') || (ch(grid, 2) == 'D' && slice(grid, 3, 4) != "66" && slice(grid, 3, 4) != "67" && slice(grid, 3, 4) != "49") || (ch(grid, 2) == 'E' && slice(grid, 3, 4) != "40" && slice(grid, 3, 4) != "41" && slice(grid, 3, 4) != "50") || (ch(grid, 2) == 'F' && slice(grid, 3, 4) != "81" && slice(grid, 3, 4) != "82") || ch(grid, 2) == 'G' || (ch(grid, 2) == 'H' && slice(grid, 3, 4) != "10") || (ch(grid, 2) == 'I' && ch(grid, 3) < '5') || (ch(grid, 2) == 'J' && ch(grid, 3) < '6') || (ch(grid, 2) == 'K' && ch(grid, 3) < '5') || (ch(grid, 2) == 'R' && ch(grid, 4) > '1'))) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'N' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'H') || (ch(grid, 2) == 'H' && slice(grid, 3, 4) != "87" && slice(grid, 3, 4) != "88") || (ch(grid, 2) == 'I' && ch(grid, 3) < '9' && slice(grid, 3, 4) != "89") || (ch(grid, 2) == 'J' && ch(grid, 3) > '0' && ch(grid, 3) < '6') || (ch(grid, 2) == 'R' && ch(grid, 4) > '1'))) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'O' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'F') || (ch(grid, 2) == 'F' && ch(grid, 3) < '7') || (ch(grid, 2) == 'G' && ch(grid, 3) < '6') || (ch(grid, 2) == 'H' && slice(grid, 3, 4) != "90" && slice(grid, 3, 4) != "92" && slice(grid, 3, 4) != "29" && slice(grid, 3, 4) != "99") || ch(grid, 2) == 'R')) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'P' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'F') || (ch(grid, 2) == 'F' && ch(grid, 4) < '2') || (ch(grid, 2) == 'K' && ch(grid, 3) > '3' && slice(grid, 3, 4) != "90") || (ch(grid, 2) == 'Q' && ch(grid, 4) > '6') || ch(grid, 2) == 'R')) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'Q' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'D') || (ch(grid, 2) == 'D' && slice(grid, 3, 4) != "94" && slice(grid, 3, 4) != "95") || (ch(grid, 2) == 'E' && ch(grid, 4) < '6') || (ch(grid, 2) == 'F' && ch(grid, 3) > '6' && slice(grid, 3, 4) != "98") || (ch(grid, 2) == 'K' && ch(grid, 3) > '2' && slice(grid, 3, 4) != "36") || (ch(grid, 2) == 'L' && ch(grid, 3) > '2' && slice(grid, 3, 4) != "64" && slice(grid, 3, 4) != "74") || (ch(grid, 2) == 'M' && ch(grid, 3) > '0' && slice(grid, 3, 4) != "19") || (ch(grid, 2) == 'N' && ch(grid, 3) > '7') || (ch(grid, 2) == 'Q' && ch(grid, 4) > '7') || ch(grid, 2) == 'R')) {
      lchkcall = true;
    } else if (ch(grid, 1) == 'R' && ((ch(grid, 2) > '@' && ch(grid, 2) < 'D') || (ch(grid, 2) == 'D' && slice(grid, 3, 4) != "47" && slice(grid, 3, 4) != "29" && slice(grid, 3, 4) != "39") || (ch(grid, 2) == 'K' && ch(grid, 4) > '4' && slice(grid, 3, 4) != "39") || (ch(grid, 2) > 'K' && ch(grid, 2) < 'O') || (ch(grid, 2) == 'O' && ch(grid, 4) == '0') || (ch(grid, 2) == 'Q' && ch(grid, 4) > '1') || ch(grid, 2) == 'R')) {
      lchkcall = true;
    }
  } else if (ch(grid, 1) > '/' && ch(grid, 1) < ':') {
    lchkcall = true;
  }
  if (!lchkcall) {
    if (ch(callsign, 1) > 'K' && ch(callsign, 1) < '[') {
      goto label_2;
    } else if (ch(callsign, 1) > '/' && ch(callsign, 1) < ':') {
      goto label_4;
    }
    if (ch(callsign, 1) == 'A') {
      if (ch(callsign, 2) > '@' && ch(callsign, 2) < 'M') {
        if ((slice(grid, 1, 2) == "FM" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "EL" && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "DL" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "EM" || slice(grid, 1, 2) == "DM" || slice(grid, 1, 2) == "EN" || (slice(grid, 1, 2) == "CM" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FN" && ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "DN" && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "CN" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "CO" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FK" && ch(grid, 3) > '5' && ch(grid, 3) < '8' && ch(grid, 4) > '6' && ch(grid, 4) < '9') || slice(grid, 1, 2) == "BP" || (slice(grid, 1, 2) == "BO" && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "AP" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || slice(grid, 1, 4) == "FK28" || (slice(grid, 1, 2) == "AO" && ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "BL" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "BK" && (slice(grid, 3, 4) == "19" || slice(grid, 3, 4) == "28" || slice(grid, 3, 4) == "29")) || (slice(grid, 1, 2) == "AL" && (slice(grid, 3, 4) == "91" || slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "18" || slice(grid, 3, 4) == "27" || slice(grid, 3, 4) == "36" || slice(grid, 3, 4) == "45" || slice(grid, 3, 4) == "64" || slice(grid, 3, 4) == "73" || slice(grid, 3, 4) == "93")) || (slice(grid, 1, 3) == "QK2" && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "AO" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '0' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "RO" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '0' && ch(grid, 4) < '3') || slice(grid, 3, 4) == "63")) || slice(grid, 1, 4) == "AP30" || slice(grid, 1, 4) == "QK36" || slice(grid, 1, 4) == "QL20" || slice(grid, 1, 4) == "RK39" || slice(grid, 1, 4) == "AK56" || slice(grid, 1, 4) == "AH48" || slice(grid, 1, 4) == "JA00" || slice(grid, 1, 4) == "RB32" || slice(grid, 1, 4) == "CP00") {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "FM" && (slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "23" || slice(grid, 3, 4) == "24")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "EL" && ((ch(grid, 3) > '1' && ch(grid, 3) < '8' && ch(grid, 4) > '3' && ch(grid, 4) < '9' && slice(grid, 3, 4) != "28" && slice(grid, 3, 4) != "58") || slice(grid, 3, 4) == "04" || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "14")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "DL" && (slice(grid, 3, 4) == "78" || slice(grid, 3, 4) == "88")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "DM" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "EN" && ((ch(grid, 3) == '9' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || ((ch(grid, 3) == '8' || ch(grid, 3) == '7') && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) == '9' && ch(grid, 3) != '2'))) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "CM" && (slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "85")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "FN" && ((ch(grid, 4) == '7' && ch(grid, 3) > '/' && ch(grid, 3) < '5') || (ch(grid, 4) == '6' && ch(grid, 3) > '/' && ch(grid, 3) < '4') || (ch(grid, 3) == '6' && ch(grid, 4) > '0' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "04" || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "15" || slice(grid, 3, 4) == "52")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "CO" && ((ch(grid, 3) == '0' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || (ch(grid, 3) == '1' && ch(grid, 4) > '3' && ch(grid, 4) < '7') || (ch(grid, 3) == '4' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "39")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "BO" && ((ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < '9') || (ch(grid, 3) == '4' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "14" || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "35")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "AP" && ((ch(grid, 3) == '4' && ((ch(grid, 4) > '/' && ch(grid, 4) < '3') || (ch(grid, 4) > '3' && ch(grid, 4) < ':'))) || (ch(grid, 3) == '5' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "63" || slice(grid, 3, 4) == "67" || slice(grid, 3, 4) == "69" || slice(grid, 3, 4) == "79")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "AO" && ((ch(grid, 3) == '4' && ((ch(grid, 4) > '2' && ch(grid, 4) < '7') || ch(grid, 4) == '8' || ch(grid, 4) == '9')) || (ch(grid, 3) == '5' && ch(grid, 4) > '3' && ch(grid, 4) < ':' && ch(grid, 4) != '6') || ((ch(grid, 3) == '6' || ch(grid, 3) == '7') && ch(grid, 4) > '4' && ch(grid, 4) < '9') || (ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "97" || slice(grid, 3, 4) == "86" || slice(grid, 3, 4) == "87" || slice(grid, 3, 4) == "31")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "BL" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "22")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "RO" && (slice(grid, 3, 4) == "61" || slice(grid, 3, 4) == "71")) {
            lgvalid = false;
          }
        }
      } else if (slice(callsign, 1, 2) == "AX") {
        if ((slice(grid, 1, 2) == "QG" && ch(grid, 3) > '/' && ch(grid, 3) < '7') || (slice(grid, 1, 2) == "QF" && ch(grid, 3) > '/' && ch(grid, 3) < '7') || (slice(grid, 1, 2) == "PF" && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "OF" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "QE" && ((ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "19")) || (slice(grid, 1, 2) == "QH" && ch(grid, 3) > '/' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "OG" && ch(grid, 3) > '5' && ch(grid, 3) < ':') || slice(grid, 1, 2) == "PG" || slice(grid, 1, 2) == "PH") {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "QF" && ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '7') {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "PF" && ((ch(grid, 3) > '/' && ch(grid, 3) < '8' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || (ch(grid, 4) == '5' && ch(grid, 3) > '1' && ch(grid, 3) < '7') || (ch(grid, 4) == '6' && ch(grid, 3) > '2' && ch(grid, 3) < '7') || slice(grid, 3, 4) == "57")) {
            lgvalid = false;
          } else if (slice(grid, 1, 4) == "OF74") {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "QH" && ((ch(grid, 3) == '3' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (ch(grid, 3) == '4' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "09")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "OG" && (slice(grid, 3, 4) == "60" || slice(grid, 3, 4) == "69")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "PH" && ((ch(grid, 3) == '0' && ch(grid, 4) > '1' && ch(grid, 4) < ':' && ch(grid, 4) != '5' && ch(grid, 4) != '6') || (ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (ch(grid, 3) == '9' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "59" || slice(grid, 3, 4) == "79" || slice(grid, 3, 4) == "89")) {
            lgvalid = false;
          }
        }
      } else if (ch(callsign, 2) > 'O' && ch(callsign, 2) < 'T') {
        if ((slice(grid, 1, 2) == "MM" && ((ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '7') || slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "85")) || (slice(grid, 1, 2) == "ML" && ((ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "33"))) {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "MM" && ch(grid, 3) == '4' && ch(grid, 4) > '3' && ch(grid, 4) < '7') {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "ML" && ((ch(grid, 3) == '6' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "07")) {
            lgvalid = false;
          }
        }
      } else if (ch(callsign, 1) == 'A' && ch(callsign, 2) > 'L' && ch(callsign, 2) < 'P') {
        if ((slice(grid, 1, 2) == "IN" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '4' && slice(grid, 3, 4) != "50") || (slice(grid, 1, 2) == "IM" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "13" || slice(grid, 3, 4) == "75" || slice(grid, 3, 4) == "85")) || (slice(grid, 1, 2) == "JN" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "20")) || (slice(grid, 1, 2) == "JM" && (slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "09" || slice(grid, 3, 4) == "19" || slice(grid, 3, 4) == "29")) || (slice(grid, 1, 2) == "IL" && ((ch(grid, 4) == '7' && ch(grid, 3) > '/' && ch(grid, 3) < '3') || (ch(grid, 4) == '8' && ch(grid, 3) > '/' && ch(grid, 3) < '4') || slice(grid, 3, 4) == "39"))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "A2") {
        if ((slice(grid, 1, 2) == "KG" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KH" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "KH" && (slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "30")) || (slice(grid, 1, 2) == "KG" && (slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "47"))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "A3") {
        if ((slice(grid, 1, 2) == "AG" && (slice(grid, 3, 4) == "28" || slice(grid, 3, 4) == "29" || slice(grid, 3, 4) == "06" || slice(grid, 3, 4) == "07")) || (slice(grid, 1, 2) == "AH" && (slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "34"))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "A4") {
        if ((slice(grid, 1, 2) == "LL" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "LK" && ch(grid, 3) > '5' && ch(grid, 3) < '9' && ch(grid, 4) > '5' && ch(grid, 4) < ':')) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "A5") {
        if (slice(grid, 1, 2) == "NL" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '5' && ch(grid, 4) < '9') {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "A6") {
        if (slice(grid, 1, 2) == "LL" && ((ch(grid, 3) > '5' && ch(grid, 3) < '9' && ch(grid, 4) > '1' && ch(grid, 4) < '6') || slice(grid, 3, 4) == "53" || slice(grid, 3, 4) == "54")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "A7") {
        if ((slice(grid, 1, 3) == "LL5" && ch(grid, 4) > '3' && ch(grid, 4) < '7') || slice(grid, 1, 4) == "LL65") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "A9") {
        if (slice(grid, 1, 3) == "LL5" && ch(grid, 4) > '4' && ch(grid, 4) < '7') {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "AY" || slice(callsign, 1, 2) == "AZ") {
        if ((slice(grid, 1, 2) == "GF" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FF" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "FG" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "GG" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (slice(grid, 1, 2) == "FE" && ch(grid, 3) > '2' && ch(grid, 3) < '9') || (slice(grid, 1, 2) == "FD" && ch(grid, 3) > '2' && ch(grid, 3) < '8' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "GC07" || slice(grid, 1, 4) == "GC16") {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "GF" && (slice(grid, 3, 4) == "16" || slice(grid, 3, 4) == "17")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "FG" && (slice(grid, 3, 4) == "56" || slice(grid, 3, 4) == "57")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "GG" && (slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "35")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "FE" && ((ch(grid, 3) == '8' && ch(grid, 4) > '/' && ch(grid, 4) < '7') || (ch(grid, 3) == '3' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "70" || slice(grid, 3, 4) == "73")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "FD" && ((ch(grid, 3) > '2' && ch(grid, 3) < '5' && ch(grid, 4) > '4' && ch(grid, 4) < '8') || (ch(grid, 3) == '7' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (ch(grid, 3) == '6' && ch(grid, 4) > '6' && ch(grid, 4) < ':'))) {
            lgvalid = false;
          }
        }
      } else if (ch(callsign, 1) == 'A' && ch(callsign, 2) > 'S' && ch(callsign, 2) < 'X') {
        if ((slice(grid, 1, 2) == "MJ" && (slice(grid, 3, 4) == "99" || slice(grid, 3, 4) == "89" || slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "68")) || (slice(grid, 1, 2) == "MK" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "52")) || (slice(grid, 1, 2) == "NK" && ((ch(grid, 3) == '0' && ch(grid, 4) > '1' && ch(grid, 4) < '6') || (ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "39")) || (slice(grid, 1, 2) == "ML" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "NL" && ch(grid, 3) > '/' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "MM" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 1, 4) == "NM00") {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "MK" && ch(grid, 3) == '6' && ch(grid, 4) > '1' && ch(grid, 4) < '5') {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "NK" && (slice(grid, 3, 4) == "26" || slice(grid, 3, 4) == "27")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "ML" && ((ch(grid, 3) > '4' && (ch(grid, 4) == '0' || ch(grid, 4) == '5' || ch(grid, 4) == '8' || ch(grid, 4) == '9')) || slice(grid, 3, 4) == "58" || slice(grid, 3, 4) == "59")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "NL" && ((ch(grid, 4) == '9' && ch(grid, 3) > '/' && ch(grid, 4) < '5') || (ch(grid, 4) == '8' && ch(grid, 3) > '0' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "40")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "MM" && (slice(grid, 3, 4) == "61" || slice(grid, 3, 4) == "62")) {
            lgvalid = false;
          }
        }
      } else if (slice(callsign, 1, 2) == "A8") {
        if (slice(grid, 1, 2) == "IJ" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < '9' && slice(grid, 3, 4) != "67" && slice(grid, 3, 4) != "67") {
          lgvalid = true;
        }
      }
    } else if (ch(callsign, 1) == 'B') {
      if (ch(callsign, 2) > '/' && ch(callsign, 2) < ':' && ch(callsign, 3) > '/' && ch(callsign, 3) < ':') {
        return;
      }
      if (slice(grid, 1, 2) == "OL" || (slice(grid, 1, 2) == "PL" && ch(grid, 3) > '/' && ch(grid, 3) < '2') || slice(grid, 1, 2) == "OM" || (slice(grid, 1, 2) == "PM" && ch(grid, 3) > '/' && ch(grid, 3) < '3') || (slice(grid, 1, 2) == "PN" && ch(grid, 3) > '/' && ch(grid, 3) < '8') || (slice(grid, 1, 2) == "PO" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (slice(grid, 1, 2) == "OK" && (slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "58" || slice(grid, 3, 4) == "59")) || slice(grid, 1, 2) == "ON" || slice(grid, 1, 4) == "OO90" || slice(grid, 1, 4) == "OO91" || slice(grid, 1, 2) == "NN" || slice(grid, 1, 2) == "NM" || (slice(grid, 1, 2) == "NL" && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "MN" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "MM" && ch(grid, 3) > '5' && ch(grid, 3) < ':')) {
        lgvalid = true;
      }
      if (lgvalid) {
        if ((ch(callsign, 2) > 'L' && ch(callsign, 2) < 'R') || (ch(callsign, 2) > 'T' && ch(callsign, 2) < 'Y')) {
          if (slice(grid, 1, 4) != "PL04" && slice(grid, 1, 4) != "PL03" && slice(grid, 1, 4) != "PL05" && slice(grid, 1, 4) != "PL02" && slice(grid, 1, 4) != "PL01" && slice(grid, 1, 4) != "OL93" && slice(grid, 1, 4) != "PL15" && slice(grid, 1, 4) != "OL80") {
            lgvalid = false;
          }
        }
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "NL" && ((ch(grid, 3) > '/' && ch(grid, 3) < '9' && ch(grid, 4) > '0' && ch(grid, 4) < '7' && slice(grid, 3, 4) != "83" && slice(grid, 3, 4) != "84" && slice(grid, 3, 4) != "85") || slice(grid, 3, 4) == "77" || slice(grid, 3, 4) == "87")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "ON" && ((ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (ch(grid, 3) > '4' && ch(grid, 3) < '7' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "79")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PN" && ((ch(grid, 3) == '7' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (ch(grid, 3) == '5' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "40" || slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "59" || slice(grid, 3, 4) == "69" || slice(grid, 3, 4) == "79")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "MM" && ((ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (ch(grid, 3) == '7' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (ch(grid, 3) == '8' && ch(grid, 4) > '/' && ch(grid, 4) < '5'))) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OL" && ((ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) == '0') || slice(grid, 3, 4) == "11" || slice(grid, 3, 4) == "21")) {
          lgvalid = false;
        }
      }
      if (!lgvalid) {
        if (slice(callsign, 3, 4) == "9S" && ((ch(callsign, 2) > 'K' && ch(callsign, 2) < 'R') || (ch(callsign, 2) > 'T' && ch(callsign, 2) < 'Y'))) {
          if ((slice(grid, 1, 2) == "OK" && ch(grid, 3) > '5' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "OJ" && ch(grid, 3) > '3' && ch(grid, 3) < '9' && ch(grid, 4) > '6' && ch(grid, 4) < ':')) {
            lgvalid = true;
          }
        }
      }
    } else if (ch(callsign, 1) == 'C') {
      if (ch(callsign, 2) > '@' && ch(callsign, 2) < 'F') {
        if ((slice(grid, 1, 2) == "FF" && ch(grid, 3) > '2' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "FG" && ((ch(grid, 3) > '3' && ch(grid, 3) < '6') || (ch(grid, 3) == '6' && ch(grid, 4) > '4' && ch(grid, 4) < '8'))) || (slice(grid, 1, 2) == "FE" && ch(grid, 3) > '1' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "FH" && ((ch(grid, 3) > '3' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "52")) || (slice(grid, 1, 2) == "FD" && ((ch(grid, 3) > '1' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "53" || slice(grid, 3, 4) == "85")) || slice(grid, 1, 4) == "EF96" || slice(grid, 1, 4) == "FF06" || slice(grid, 1, 4) == "EG93" || slice(grid, 1, 4) == "FG03" || slice(grid, 1, 4) == "DG52" || slice(grid, 1, 4) == "DG73") {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "FF" && ch(grid, 3) == '3' && ch(grid, 4) > '5' && ch(grid, 4) < ':') {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "FE" && (slice(grid, 3, 4) == "40" || slice(grid, 3, 4) == "41" || slice(grid, 3, 4) == "29")) {
            lgvalid = false;
          }
        }
      } else if (slice(callsign, 1, 2) == "CL" || slice(callsign, 1, 2) == "CM" || slice(callsign, 1, 2) == "CO") {
        if ((slice(grid, 1, 2) == "EL" && ch(grid, 3) > '6' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "FL" && ch(grid, 3) > '/' && ch(grid, 3) < '3') || slice(grid, 1, 4) == "FK19") {
          lgvalid = true;
        }
      } else if ((ch(callsign, 2) > 'E' && ch(callsign, 2) < 'L') || slice(callsign, 1, 2) == "CY" || slice(callsign, 1, 2) == "CZ") {
        if ((slice(grid, 1, 2) == "CN" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FN" && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "EN" && ch(grid, 4) > '0' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "CO" || (slice(grid, 1, 2) == "GN" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "FO" || slice(grid, 1, 2) == "EO" || slice(grid, 1, 2) == "DO" || (slice(grid, 1, 2) == "GO" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "DN" && ch(grid, 4) > '8' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "CP" || slice(grid, 1, 2) == "DP" || slice(grid, 1, 2) == "EP" || (slice(grid, 1, 2) == "FP" && ((ch(grid, 3) > '/' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "96")) || (slice(grid, 1, 2) == "EQ" && (slice(grid, 3, 4) == "15" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "71" || slice(grid, 3, 4) == "73" || slice(grid, 3, 4) == "79" || slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "86" || slice(grid, 3, 4) == "96")) || (slice(grid, 1, 2) == "FR" && (slice(grid, 3, 4) == "71" || slice(grid, 3, 4) == "82")) || (slice(grid, 1, 2) == "FQ" && (slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "12")) || slice(grid, 1, 4) == "GN03" || slice(grid, 1, 4) == "FN93" || slice(grid, 1, 4) == "ER60" || slice(grid, 1, 4) == "DQ10" || slice(grid, 1, 4) == "CQ71") {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "EO" && ((ch(grid, 3) > '3' && ch(grid, 3) < '9' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || (ch(grid, 4) == '7' && ch(grid, 3) > '4' && ch(grid, 3) < '9') || slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "86")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "CO" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "18" || slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "40")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "EP" && ((ch(grid, 4) == '0' && ch(grid, 3) > '2' && ch(grid, 3) < ':') || (ch(grid, 4) == '1' && ch(grid, 3) > '3' && ch(grid, 3) < '9') || (ch(grid, 4) == '2' && ch(grid, 3) > '4' && ch(grid, 3) < '8'))) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "CN" && (slice(grid, 3, 4) == "68" || slice(grid, 3, 4) == "98")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "FP" && ((ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '4' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "04")) {
            lgvalid = false;
          }
        }
      } else if (ch(callsign, 2) > 'P' && ch(callsign, 2) < 'U') {
        if ((slice(grid, 1, 2) == "IN" && ch(grid, 3) > '4' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "IM" && ch(grid, 3) > '4' && ((ch(grid, 3) < '7' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "13" || slice(grid, 3, 4) == "10" || slice(grid, 3, 4) == "20")) || (slice(grid, 1, 2) == "HM" && ((ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "76"))) {
          lgvalid = true;
        }
      } else if (ch(callsign, 2) > 'U' && ch(callsign, 2) < 'Y') {
        if (slice(grid, 1, 2) == "GF" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '4' && ch(grid, 4) < ':') {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "CP") {
        if ((slice(grid, 1, 2) == "FH" && ch(grid, 3) > '4' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "FG" && ch(grid, 3) > '4' && ch(grid, 3) < '9' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "GH" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (slice(grid, 1, 2) == "FI" && (slice(grid, 3, 4) == "60" || slice(grid, 3, 4) == "70")) || (slice(grid, 1, 2) == "GG" && (slice(grid, 3, 4) == "09" || slice(grid, 3, 4) == "19"))) {
          lgvalid = true;
        }
      } else if ((slice(callsign, 1, 2) == "C2" && slice(grid, 1, 4) == "RI39") || (slice(callsign, 1, 2) == "C3" && slice(grid, 1, 4) == "JN02")) {
        lgvalid = true;
      } else if (slice(callsign, 1, 2) == "CN") {
        if ((slice(grid, 1, 2) == "IM" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (slice(grid, 1, 2) == "IL" && ((ch(grid, 3) > '0' && ch(grid, 3) < '6' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "69" || slice(grid, 3, 4) == "79"))) {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 3) == "IM9" && (ch(grid, 4) == '0' || ch(grid, 4) == '1' || ch(grid, 4) == '5')) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "IL" && ((ch(grid, 3) > '3' && ch(grid, 3) < '6' && ch(grid, 4) > '0' && ch(grid, 4) < '6') || (ch(grid, 4) == '1' || ch(grid, 4) == '3' || ch(grid, 4) == ':') || (ch(grid, 4) == '2' || ch(grid, 4) == '6' || ch(grid, 4) == ':') || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "39")) {
            lgvalid = false;
          }
        }
      } else if (slice(callsign, 1, 2) == "C4") {
        if ((slice(grid, 1, 3) == "KM6" && (ch(grid, 4) == '4' || ch(grid, 4) == '5')) || (slice(grid, 1, 3) == "KM7" && (ch(grid, 4) == '4' || ch(grid, 4) == '5'))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "C5" && slice(grid, 1, 2) == "IK" && ch(grid, 3) > '0' && ch(grid, 3) < '4' && ch(grid, 4) == '3') {
        lgvalid = true;
      } else if (slice(callsign, 1, 2) == "C6" && slice(grid, 1, 2) == "FL" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '1' && ch(grid, 4) < '7') {
        lgvalid = true;
      } else if (slice(callsign, 1, 2) == "C8" || slice(callsign, 1, 2) == "C9") {
        if ((slice(grid, 1, 2) == "KG" && ch(grid, 3) > '4' && ch(grid, 3) < '8' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KH" && ch(grid, 3) > '4' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "LH" && ch(grid, 3) == '1' && ch(grid, 4) > '2' && ch(grid, 4) < ':')) {
          lgvalid = true;
        }
      } else if (ch(callsign, 2) == '0' || ch(callsign, 2) == '1' || ch(callsign, 2) == '7') {
        lwrongcall = true;
        return;
      }
    } else if (ch(callsign, 1) == 'D') {
      if (ch(callsign, 2) > '@' && ch(callsign, 2) < 'S') {
        if ((slice(grid, 1, 2) == "JO" && ((ch(grid, 3) > '1' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "45")) || (slice(grid, 1, 2) == "JN" && ch(grid, 3) > '2' && ch(grid, 3) < '7' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "IB59") {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "JO" && ((ch(grid, 3) < '2' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "35")) {
            lgvalid = false;
          }
        }
      } else if (slice(callsign, 1, 2) == "DS" || slice(callsign, 1, 2) == "DT" || (ch(callsign, 2) > '6' && ch(callsign, 2) < ':')) {
        if ((slice(grid, 1, 3) == "PM3" && ch(grid, 4) > '2' && ch(grid, 4) < '9') || (slice(grid, 1, 3) == "PM4" && ch(grid, 4) > '3' && ch(grid, 4) < '9') || slice(grid, 1, 4) == "PM24" || slice(grid, 1, 4) == "PM57" || slice(grid, 1, 4) == "GC07") {
          lgvalid = true;
        }
      } else if (ch(callsign, 2) > 'T' && ch(callsign, 2) < '[') {
        if ((slice(grid, 1, 2) == "PK" && ch(grid, 3) > '/' && ch(grid, 3) < '4') || (slice(grid, 1, 2) == "PJ" && ((ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "04")) || (slice(grid, 1, 2) == "OJ" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "OK" && ch(grid, 3) == '9' && ch(grid, 4) > '/' && ch(grid, 4) < '7') || (slice(grid, 1, 2) == "PL" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "10")) || slice(grid, 1, 4) == "OK70" || slice(grid, 1, 4) == "OK71") {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "PK" && ((ch(grid, 3) == '3' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (ch(grid, 3) == '2' && ch(grid, 4) > '4' && ch(grid, 4) < ':'))) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "PJ" && (slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "15")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "OJ" && (slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "85")) {
            lgvalid = false;
          }
        }
      } else if (slice(callsign, 1, 2) == "D0" || slice(callsign, 1, 2) == "D1") {
        if (slice(grid, 1, 3) == "KN8" && ch(grid, 4) > '6' && ch(grid, 4) < ':' || slice(grid, 1, 3) == "KN9" && ch(grid, 4) > '6' && ch(grid, 4) < ':' || slice(grid, 1, 4) == "LN08" || slice(grid, 1, 4) == "LN09") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "D2" || slice(callsign, 1, 2) == "D3") {
        if ((slice(grid, 1, 2) == "JI" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "JH" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KH" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "28")) || (slice(grid, 1, 3) == "KI0" && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 3) == "JH5" && ch(grid, 4) > '1' && ch(grid, 4) < '5')) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "D4" && ((slice(grid, 1, 3) == "HK7" && ch(grid, 4) > '3' && ch(grid, 4) < '8') || (slice(grid, 1, 3) == "HK8" && ch(grid, 4) > '3' && ch(grid, 4) < '7'))) {
        lgvalid = true;
      } else if (slice(callsign, 1, 2) == "D5") {
        if (slice(grid, 1, 2) == "IJ" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < '9' && slice(grid, 3, 4) != "67" && slice(grid, 3, 4) != "67") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "D6" && slice(grid, 1, 2) == "LH" && (slice(grid, 3, 4) == "17" || slice(grid, 3, 4) == "18" || slice(grid, 3, 4) == "27")) {
        lgvalid = true;
      }
    } else if (ch(callsign, 1) == 'E') {
      if (ch(callsign, 2) > '@' && ch(callsign, 2) < 'I') {
        if ((slice(grid, 1, 2) == "IN" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '4' && slice(grid, 3, 4) != "50") || (slice(grid, 1, 2) == "IM" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "13" || slice(grid, 3, 4) == "75" || slice(grid, 3, 4) == "85")) || (slice(grid, 1, 2) == "JN" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "20")) || (slice(grid, 1, 2) == "JM" && (slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "09" || slice(grid, 3, 4) == "19" || slice(grid, 3, 4) == "29")) || (slice(grid, 1, 2) == "IL" && ((ch(grid, 4) == '7' && ch(grid, 3) > '/' && ch(grid, 3) < '3') || (ch(grid, 4) == '8' && ch(grid, 3) > '/' && ch(grid, 3) < '4') || slice(grid, 3, 4) == "39"))) {
          lgvalid = true;
        }
      } else if (ch(callsign, 2) > 'L' && ch(callsign, 2) < 'P') {
        if ((slice(grid, 1, 2) == "KO" && ((ch(grid, 3) > '0' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "52" || slice(grid, 3, 4) == "62")) || (slice(grid, 1, 2) == "KN" && ch(grid, 3) > '0' && ch(grid, 3) < ':' && ch(grid, 4) > '4' && ch(grid, 4) < ':')) {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "KO" && (slice(grid, 3, 4) == "81" || slice(grid, 3, 4) == "91")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "KN" && ((ch(grid, 3) > '0' && ch(grid, 3) < '4' && ch(grid, 4) > '4' && ch(grid, 4) < '7') || slice(grid, 3, 4) == "95" || slice(grid, 3, 4) == "96")) {
            lgvalid = false;
          }
        }
      } else if (slice(callsign, 1, 2) == "EI" || slice(callsign, 1, 2) == "EJ") {
        if (slice(grid, 1, 2) == "IO" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '0' && ch(grid, 4) < '6') {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "E2") {
        if ((slice(grid, 1, 2) == "OK" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '0' && ch(grid, 4) < '9') || slice(grid, 3, 4) == "09")) || (slice(grid, 1, 2) == "NK" && ((ch(grid, 3) > '8' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "87" || slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "89")) || (slice(grid, 1, 2) == "NJ" && ((ch(grid, 3) == '9' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "89")) || (slice(grid, 1, 2) == "OJ" && ((ch(grid, 3) == '0' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "16")) || slice(grid, 1, 4) == "NL90" || slice(grid, 1, 4) == "OL00") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "E3") {
        if ((slice(grid, 1, 2) == "KK" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "LK" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '1' && ch(grid, 4) < '7')) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "E4" && (slice(grid, 1, 4) == "KM71" || slice(grid, 1, 4) == "KM72")) {
        lgvalid = true;
      } else if (slice(callsign, 1, 2) == "E5") {
        if ((slice(grid, 1, 2) == "BH" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "10")) || (slice(grid, 1, 2) == "BG" && (slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "09" || slice(grid, 3, 4) == "18" || slice(grid, 3, 4) == "19")) || (slice(grid, 1, 2) == "AH" && (slice(grid, 3, 4) == "78" || slice(grid, 3, 4) == "79" || slice(grid, 3, 4) == "81" || slice(grid, 3, 4) == "86" || slice(grid, 3, 4) == "99")) || (slice(grid, 1, 2) == "BI" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "10" || slice(grid, 3, 4) == "11"))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "E6") {
        if (slice(grid, 1, 4) == "AH50" || slice(grid, 1, 4) == "AH51") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "E7") {
        if (slice(grid, 1, 2) == "JN" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < '6' && slice(grid, 3, 4) != "72" && slice(grid, 3, 4) != "73") {
          lgvalid = true;
        }
      } else if (ch(callsign, 2) > 'T' && ch(callsign, 2) < 'X') {
        if (slice(grid, 1, 2) == "KO" && ch(grid, 3) > '0' && ch(grid, 3) < '7' && ch(grid, 4) > '0' && ch(grid, 4) < '7') {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "ES") {
        if ((slice(grid, 1, 2) == "KO" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "KO49") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "ER") {
        if ((slice(grid, 1, 2) == "KN" && ch(grid, 3) > '2' && ch(grid, 3) < '5' && ch(grid, 4) > '4' && ch(grid, 4) < '9') || slice(grid, 1, 4) == "KN56") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "EK") {
        if ((slice(grid, 1, 2) == "LN" && (slice(grid, 3, 4) == "10" || slice(grid, 3, 4) == "11" || slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "21")) || (slice(grid, 1, 2) == "LM" && (slice(grid, 3, 4) == "29" || slice(grid, 3, 4) == "39" || slice(grid, 3, 4) == "38"))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "EX") {
        if ((slice(grid, 1, 2) == "MN" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (slice(grid, 1, 2) == "MM" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) == '9')) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "EY") {
        if ((slice(grid, 1, 2) == "MM" && ch(grid, 3) > '2' && ch(grid, 3) < '8' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "MN" && (slice(grid, 3, 4) == "40" || slice(grid, 3, 4) == "50"))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "EZ") {
        if ((slice(grid, 1, 2) == "LM" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "LN" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "MM" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "MN" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '2')) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "EL") {
        if (slice(grid, 1, 2) == "IJ" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < '9' && slice(grid, 3, 4) != "67" && slice(grid, 3, 4) != "67") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "EP" || slice(callsign, 1, 2) == "EQ") {
        if ((slice(grid, 1, 2) == "LM" && ch(grid, 3) > '1' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "LL" && ((ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "75" || slice(grid, 3, 4) == "85" || slice(grid, 3, 4) == "95")) || (slice(grid, 1, 2) == "ML" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '4' && ch(grid, 4) < ':' && slice(grid, 3, 4) != "15" && slice(grid, 3, 4) != "19") || (slice(grid, 1, 2) == "MM" && ch(grid, 3) == '0' && ch(grid, 4) > '/' && ch(grid, 4) < '7')) {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "LM" && ((ch(grid, 3) == '2' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':' && slice(grid, 3, 4) != "78" && slice(grid, 3, 4) != "88"))) {
            lgvalid = false;
          }
        }
      } else if (slice(callsign, 1, 2) == "ET") {
        if ((slice(grid, 1, 2) == "LJ" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "LK" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "KJ" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KK" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5')) {
          lgvalid = true;
        }
      } else if (ch(callsign, 2) == '0' || ch(callsign, 2) == '1' || ch(callsign, 2) == '8' || ch(callsign, 2) == '9') {
        lwrongcall = true;
        return;
      }
    } else if (ch(callsign, 1) == 'F') {
      if ((ch(callsign, 2) > '/' && ch(callsign, 2) < ':') || (ch(callsign, 2) > '@' && ch(callsign, 2) < 'G') || ch(callsign, 2) == 'I' || ch(callsign, 2) == 'L' || ch(callsign, 2) == 'N' || ch(callsign, 2) == 'Q' || ch(callsign, 2) == 'U' || ch(callsign, 2) == 'V' || ch(callsign, 2) == 'X' || ch(callsign, 2) == 'Z') {
        if ((slice(grid, 1, 2) == "IN" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 3) == "IN7" && (ch(grid, 4) == '7' || ch(grid, 4) == '8')) || (slice(grid, 1, 2) == "JN" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 3) == "JN4" && (ch(grid, 4) == '8' || ch(grid, 4) == '9')) || slice(grid, 1, 4) == "JN02" || slice(grid, 1, 4) == "JN12" || slice(grid, 1, 4) == "IN92" || (slice(grid, 1, 2) == "JO" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "10" || slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "11"))) {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "IN" && (slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "85")) {
            lgvalid = false;
          }
        }
      } else if (slice(callsign, 1, 2) == "FM" && slice(grid, 1, 4) == "FK94") {
        lgvalid = true;
      } else if (slice(callsign, 1, 2) == "FG") {
        if (slice(grid, 1, 2) == "FK" && (slice(grid, 3, 4) == "96" || slice(grid, 3, 4) == "95")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "FP") {
        if (slice(grid, 1, 2) == "GN" && (slice(grid, 3, 4) == "16" || slice(grid, 3, 4) == "17")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "FS" && slice(grid, 1, 4) == "FK88") {
        lgvalid = true;
      } else if (slice(callsign, 1, 2) == "FJ" && slice(grid, 1, 4) == "FK87") {
        lgvalid = true;
      } else if (slice(callsign, 1, 2) == "FY") {
        if (slice(grid, 1, 2) == "GJ" && ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '1' && ch(grid, 4) < '6') {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "FR") {
        if (slice(grid, 1, 2) == "LG" && (slice(grid, 3, 4) == "79" || slice(grid, 3, 4) == "78")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "FK") {
        if ((slice(grid, 1, 2) == "RG" && ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "RH" && ch(grid, 3) > '0' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "RG" && (slice(grid, 3, 4) == "19" || slice(grid, 3, 4) == "57" || slice(grid, 3, 4) == "67"))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "FK") {
        if ((slice(grid, 1, 2) == "QH" && (slice(grid, 3, 4) == "90" || slice(grid, 3, 4) == "91")) || slice(grid, 1, 4) == "QG98") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "FH") {
        if (slice(grid, 1, 2) == "LH" && (slice(grid, 3, 4) == "27" || slice(grid, 3, 4) == "26")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "FT") {
        if ((slice(grid, 1, 2) == "LH" && (slice(grid, 3, 4) == "38" || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "74")) || (slice(grid, 1, 2) == "LE" && (slice(grid, 3, 4) == "53" || slice(grid, 3, 4) == "54" || slice(grid, 3, 4) == "63")) || (slice(grid, 1, 2) == "ME" && (slice(grid, 3, 4) == "40" || slice(grid, 3, 4) == "41" || slice(grid, 3, 4) == "50")) || (slice(grid, 1, 2) == "MF" && (slice(grid, 3, 4) == "81" || slice(grid, 3, 4) == "82")) || slice(grid, 1, 4) == "LG07" || slice(grid, 1, 4) == "KG98") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "FO") {
        if ((slice(grid, 1, 2) == "BH" && ch(grid, 3) > '1' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (slice(grid, 1, 2) == "BG" && ch(grid, 3) > '1' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 3) == "BI9" && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "CI" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01")) || slice(grid, 1, 4) == "CH09" || slice(grid, 1, 4) == "DK50") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "FW") {
        if (slice(grid, 1, 2) == "AH" && (slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "16")) {
          lgvalid = true;
        }
      }
    } else if (ch(callsign, 1) == 'G') {
      if ((slice(grid, 1, 2) == "IO" && ch(grid, 3) > '4' && ch(grid, 3) < ':') || (slice(grid, 1, 3) == "JO0" && ch(grid, 4) > '/' && ch(grid, 4) < '4') || slice(grid, 1, 4) == "IP90" || slice(grid, 1, 4) == "IN69" || slice(grid, 1, 4) == "IN89" || slice(grid, 1, 4) == "IO37") {
        lgvalid = true;
      }
      if (lgvalid && slice(grid, 1, 2) == "IO") {
        if ((slice(grid, 1, 3) == "IO5" && ch(grid, 4) != '4' && ch(grid, 4) != '7') || (slice(grid, 1, 3) == "IO6" && ch(grid, 4) > '/' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "96" || slice(grid, 3, 4) == "98") {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 1) == 'H') {
      if (slice(callsign, 1, 2) == "HA" || slice(callsign, 1, 2) == "HG") {
        if ((slice(grid, 1, 2) == "JN" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '4' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "KN" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '5' && ch(grid, 4) < '9' && slice(grid, 3, 4) != "16")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "HB" || slice(callsign, 1, 2) == "HE") {
        if (((slice(grid, 1, 3) == "JN3" || slice(grid, 1, 3) == "JN4") && ch(grid, 4) > '4' && ch(grid, 4) < '8') || slice(grid, 1, 4) == "JN56") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "HS") {
        if ((slice(grid, 1, 2) == "OK" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '0' && ch(grid, 4) < '9') || slice(grid, 3, 4) == "09")) || (slice(grid, 1, 2) == "NK" && ((ch(grid, 3) > '8' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "87" || slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "89")) || (slice(grid, 1, 2) == "NJ" && ((ch(grid, 3) == '9' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "89")) || (slice(grid, 1, 2) == "OJ" && ((ch(grid, 3) == '0' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "16")) || slice(grid, 1, 4) == "NL90" || slice(grid, 1, 4) == "OL00") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "H2") {
        if ((slice(grid, 1, 3) == "KM6" && (ch(grid, 4) == '4' || ch(grid, 4) == '5')) || (slice(grid, 1, 3) == "KM7" && (ch(grid, 4) == '4' || ch(grid, 4) == '5'))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "HF") {
        if ((slice(grid, 1, 2) == "JO" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "KO" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "20")) || slice(grid, 1, 4) == "KN09" || slice(grid, 1, 4) == "KN19" || slice(grid, 1, 4) == "KO20" || slice(grid, 1, 4) == "JN99" || slice(grid, 1, 4) == "JN89") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "H6" || slice(callsign, 1, 2) == "H7" || slice(callsign, 1, 2) == "HT") {
        if (slice(grid, 1, 2) == "EK" && ch(grid, 3) > '5' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '5') {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "HC" || slice(callsign, 1, 2) == "HD") {
        if ((slice(grid, 1, 2) == "FI" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "06")) || (slice(grid, 1, 2) == "FJ" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) == '0') || slice(grid, 3, 4) == "01")) || (slice(grid, 1, 2) == "EI" && ((ch(grid, 3) == '9' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "59" || slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "58")) || (slice(grid, 1, 2) == "EJ" && (slice(grid, 3, 4) == "90" || slice(grid, 3, 4) == "40"))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "HH") {
        if ((slice(grid, 1, 2) == "FK" && (slice(grid, 3, 4) == "38" || slice(grid, 3, 4) == "39" || slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "49")) || slice(grid, 1, 4) == "FL30") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "HI") {
        if (slice(grid, 1, 2) == "FK" && (slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "58" || slice(grid, 3, 4) == "59" || slice(grid, 3, 4) == "47")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "HL") {
        if ((slice(grid, 1, 3) == "PM3" && ch(grid, 4) > '2' && ch(grid, 4) < '9') || (slice(grid, 1, 3) == "PM4" && ch(grid, 4) > '3' && ch(grid, 4) < '9') || slice(grid, 1, 4) == "PM24" || slice(grid, 1, 4) == "PM57" || slice(grid, 1, 4) == "GC07") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "HK" || slice(callsign, 1, 2) == "HJ") {
        if ((slice(grid, 1, 2) == "FJ" && ((ch(grid, 3) > '0' && ch(grid, 3) < '7') || slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "02" || slice(grid, 3, 4) == "03")) || (slice(grid, 1, 2) == "FK" && ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '3' && slice(grid, 3, 4) != "22") || (slice(grid, 1, 2) == "FI" && ((ch(grid, 3) > '1' && ch(grid, 3) < '6' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "45" || slice(grid, 3, 4) == "55" || slice(grid, 3, 4) == "46" || slice(grid, 3, 4) == "56") && slice(grid, 3, 4) != "27") || (slice(grid, 1, 2) == "EJ" && (slice(grid, 3, 4) == "93" || slice(grid, 3, 4) == "94")) || slice(grid, 1, 4) == "EK92") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "HZ") {
        if ((slice(grid, 1, 2) == "LL" && ch(grid, 3) > '/' && ch(grid, 3) < '8') || (slice(grid, 1, 2) == "KL" && ch(grid, 3) > '6' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "LK" && ch(grid, 3) > '/' && ch(grid, 3) < '8' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "LM" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "10")) || (slice(grid, 1, 2) == "KM" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 1, 4) == "KM92") {
          lgvalid = true;
        }
      } else if (ch(callsign, 2) == 'P' || ch(callsign, 2) == '3' || ch(callsign, 2) == '8' || ch(callsign, 2) == '9' || ch(callsign, 2) == 'O') {
        if ((slice(grid, 1, 2) == "FJ" && (slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "09" || slice(grid, 3, 4) == "17" || slice(grid, 3, 4) == "18")) || (slice(grid, 1, 2) == "EJ" && (slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "89" || slice(grid, 3, 4) == "97" || slice(grid, 3, 4) == "98" || slice(grid, 3, 4) == "99"))) {
          lgvalid = true;
        }
      } else if (ch(callsign, 2) == 'R' || ch(callsign, 2) == 'Q') {
        if (slice(grid, 1, 2) == "EK" && ch(grid, 3) > '4' && ch(grid, 3) < '9' && ch(grid, 4) > '2' && ch(grid, 4) < '7' && slice(grid, 3, 4) != "56" && slice(grid, 3, 4) != "83") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "HN") {
        if ((slice(grid, 1, 2) == "LM" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "KM" && (slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "93")) || (slice(grid, 1, 2) == "LL" && ch(grid, 3) > '0' && ch(grid, 3) < '5' && ch(grid, 4) == '9')) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "HU") {
        if (slice(grid, 1, 2) == "EK" && (slice(grid, 3, 4) == "53" || slice(grid, 3, 4) == "54" || slice(grid, 3, 4) == "63" || slice(grid, 3, 4) == "43")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "H5") {
        if ((slice(grid, 1, 2) == "KG" && ((ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (ch(grid, 3) == '6' && ch(grid, 4) > '0' && ch(grid, 4) < '4'))) || (slice(grid, 1, 2) == "KF" && ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JF" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '4' && ch(grid, 4) < ':' && slice(grid, 3, 4) != "85") || (slice(grid, 1, 2) == "JG" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 1, 4) == "KE83" || slice(grid, 1, 4) == "KE93") {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "KF" && ((ch(grid, 4) == '5' && ch(grid, 3) > '2' && ch(grid, 3) < '6') || (ch(grid, 4) == '6' && ch(grid, 3) > '3' && ch(grid, 3) < '6') || slice(grid, 3, 4) == "57")) {
            lgvalid = false;
          }
        }
      } else if (ch(callsign, 2) > 'V' && ch(callsign, 2) < 'Z') {
        if ((slice(grid, 1, 2) == "IN" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 3) == "IN7" && (ch(grid, 4) == '7' || ch(grid, 4) == '8')) || (slice(grid, 1, 2) == "JN" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 3) == "JN4" && (ch(grid, 4) == '8' || ch(grid, 4) == '9')) || slice(grid, 1, 4) == "JN02" || slice(grid, 1, 4) == "JN12" || slice(grid, 1, 4) == "IN92" || (slice(grid, 1, 2) == "JO" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "10" || slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "11"))) {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "IN" && (slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "85")) {
            lgvalid = false;
          }
        }
      } else if (slice(callsign, 1, 2) == "H1") {
        lwrongcall = true;
        return;
      }
    } else if (ch(callsign, 1) == 'I') {
      if ((slice(grid, 1, 2) == "JN" && ((ch(grid, 3) > '2' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "40" || slice(grid, 3, 4) == "41")) || (slice(grid, 1, 2) == "JM" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "65" || slice(grid, 3, 4) == "56"))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "JM" && (slice(grid, 3, 4) == "69" || slice(grid, 3, 4) == "86" || slice(grid, 3, 4) == "96" || slice(grid, 3, 4) == "97" || slice(grid, 3, 4) == "98")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "JN" && ((ch(grid, 3) == '7' && ch(grid, 4) > '2' && ch(grid, 4) < '8') || (ch(grid, 3) == '8' && ch(grid, 4) > '1' && ch(grid, 4) < '8') || (ch(grid, 3) == '9' && ch(grid, 4) > '0' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "50")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 1) == 'J') {
      if (slice(callsign, 1, 2) == "JA" || (ch(callsign, 2) > 'D' && ch(callsign, 2) < 'T')) {
        if ((slice(grid, 1, 2) == "PM" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || (slice(grid, 1, 3) == "QM0" && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "QN" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (slice(grid, 1, 3) == "PN9" && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "PL" && ch(grid, 3) > '0' && ch(grid, 3) < '6' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "QM19" || slice(grid, 1, 4) == "KC90") {
          lgvalid = true;
        }
        if (lgvalid) {
          if (slice(grid, 1, 2) == "PM" && ((ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (ch(grid, 3) > '5' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '3' && slice(grid, 3, 4) != "62") || slice(grid, 3, 4) == "45" || slice(grid, 3, 4) == "46" || slice(grid, 3, 4) == "55" || slice(grid, 3, 4) == "56" || slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "89")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "PL" && ((ch(grid, 3) > '0' && ch(grid, 3) < '3' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "38" || slice(grid, 3, 4) == "39" || slice(grid, 3, 4) == "57" || slice(grid, 3, 4) == "59")) {
            lgvalid = false;
          } else if (slice(grid, 1, 2) == "QN" && ((ch(grid, 3) == '2' && (ch(grid, 4) == '0' || ch(grid, 4) == '1' || ch(grid, 4) == '5')) || slice(grid, 3, 4) == "10")) {
            lgvalid = false;
          }
        }
      } else if (slice(callsign, 1, 2) == "J4") {
        if ((slice(grid, 1, 2) == "KM" && ((ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "46" || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "14") && slice(grid, 3, 4) != "05") || slice(grid, 1, 4) == "JM99" || (slice(grid, 1, 2) == "KN" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && (ch(grid, 4) == '0' || ch(grid, 4) == '1'))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "J2") {
        if (slice(grid, 1, 2) == "LK" && (slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "11" || slice(grid, 3, 4) == "12")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "J3") {
        if (slice(grid, 1, 2) == "FK" && (slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "91")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "J5") {
        if (slice(grid, 1, 2) == "IK" && (slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "11" || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "20")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "J6") {
        if (slice(grid, 1, 2) == "FK" && (slice(grid, 3, 4) == "93" || slice(grid, 3, 4) == "94")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "J7" && slice(grid, 1, 4) == "FK95") {
        lgvalid = true;
      } else if (slice(callsign, 1, 2) == "J8") {
        if (slice(grid, 1, 2) == "FK" && (slice(grid, 3, 4) == "93" || slice(grid, 3, 4) == "92")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 3) == "JD1") {
        if (slice(grid, 1, 2) == "QL" && (slice(grid, 3, 4) == "04" || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "16" || slice(grid, 3, 4) == "17")) {
          lgvalid = true;
        }
      } else if (ch(callsign, 2) == 'T' || ch(callsign, 2) == 'U' || ch(callsign, 2) == 'V') {
        if ((slice(grid, 1, 2) == "ON" && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "NN" && ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "NO" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "OO" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "OO" && (slice(grid, 3, 4) == "60" || slice(grid, 3, 4) == "70"))) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "JW") {
        if ((slice(grid, 1, 2) == "JQ" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KQ" && ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JR" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) == '0') || (slice(grid, 1, 2) == "KR" && ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) == '0') || slice(grid, 1, 4) == "JQ94") {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "JX") {
        if (slice(grid, 1, 2) == "IQ" && (slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "61")) {
          lgvalid = true;
        }
      } else if (slice(callsign, 1, 2) == "JY") {
        if ((slice(grid, 1, 2) == "KM" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (slice(grid, 1, 2) == "KL" && (slice(grid, 3, 4) == "79" || slice(grid, 3, 4) == "89"))) {
          lgvalid = true;
        }
      } else if (ch(callsign, 2) == 'B' || ch(callsign, 2) == 'C' || ch(callsign, 2) == '1' || ch(callsign, 2) == '9' || ch(callsign, 2) == 'Z' || (slice(callsign, 1, 2) == "JD" && ch(callsign, 3) > '1' && ch(callsign, 3) < ':')) {
        lwrongcall = true;
        return;
      }
    } else if (ch(callsign, 1) == 'K') {
      if (ch(callsign, 2) > '/' && ch(callsign, 2) < ':' && ch(callsign, 3) > '/' && ch(callsign, 3) < ':') {
        return;
      }
      if ((slice(grid, 1, 2) == "FM" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "EL" && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "DL" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "EM" || slice(grid, 1, 2) == "DM" || slice(grid, 1, 2) == "EN" || (slice(grid, 1, 2) == "CM" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FN" && ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "DN" && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "CN" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "CO" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FK" && ch(grid, 3) > '5' && ch(grid, 3) < '8' && ch(grid, 4) > '6' && ch(grid, 4) < '9') || slice(grid, 1, 2) == "BP" || (slice(grid, 1, 2) == "BO" && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "AP" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || slice(grid, 1, 4) == "FK28" || (slice(grid, 1, 2) == "AO" && ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "BL" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "BK" && (slice(grid, 3, 4) == "19" || slice(grid, 3, 4) == "28" || slice(grid, 3, 4) == "29")) || (slice(grid, 1, 2) == "AL" && (slice(grid, 3, 4) == "91" || slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "18" || slice(grid, 3, 4) == "27" || slice(grid, 3, 4) == "36" || slice(grid, 3, 4) == "45" || slice(grid, 3, 4) == "64" || slice(grid, 3, 4) == "73" || slice(grid, 3, 4) == "93")) || (slice(grid, 1, 3) == "QK2" && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "AO" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '0' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "RO" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '0' && ch(grid, 4) < '3') || slice(grid, 3, 4) == "63")) || slice(grid, 1, 4) == "AP30" || slice(grid, 1, 4) == "QK36" || slice(grid, 1, 4) == "QL20" || slice(grid, 1, 4) == "RK39" || slice(grid, 1, 4) == "AK56" || slice(grid, 1, 4) == "AH48" || slice(grid, 1, 4) == "JA00" || slice(grid, 1, 4) == "RB32" || slice(grid, 1, 4) == "CP00") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "FM" && (slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "23" || slice(grid, 3, 4) == "24")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "EL" && ((ch(grid, 3) > '1' && ch(grid, 3) < '8' && ch(grid, 4) > '3' && ch(grid, 4) < '9' && slice(grid, 3, 4) != "28" && slice(grid, 3, 4) != "58") || slice(grid, 3, 4) == "04" || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "14")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "DL" && (slice(grid, 3, 4) == "78" || slice(grid, 3, 4) == "88")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "DM" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "EN" && ((ch(grid, 3) == '9' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || ((ch(grid, 3) == '8' || ch(grid, 3) == '7') && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) == '9' && ch(grid, 3) != '2'))) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "CM" && (slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "85")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FN" && ((ch(grid, 4) == '7' && ch(grid, 3) > '/' && ch(grid, 3) < '5') || (ch(grid, 4) == '6' && ch(grid, 3) > '/' && ch(grid, 3) < '4') || (ch(grid, 3) == '6' && ch(grid, 4) > '0' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "04" || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "15" || slice(grid, 3, 4) == "52")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "CO" && ((ch(grid, 3) == '0' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || (ch(grid, 3) == '1' && ch(grid, 4) > '3' && ch(grid, 4) < '7') || (ch(grid, 3) == '4' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "39")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "BO" && ((ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < '9') || (ch(grid, 3) == '4' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "14" || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "35")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "AP" && ((ch(grid, 3) == '4' && ((ch(grid, 4) > '/' && ch(grid, 4) < '3') || (ch(grid, 4) > '3' && ch(grid, 4) < ':'))) || (ch(grid, 3) == '5' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "63" || slice(grid, 3, 4) == "67" || slice(grid, 3, 4) == "69" || slice(grid, 3, 4) == "79")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "AO" && ((ch(grid, 3) == '4' && ((ch(grid, 4) > '2' && ch(grid, 4) < '7') || ch(grid, 4) == '8' || ch(grid, 4) == '9')) || (ch(grid, 3) == '5' && ch(grid, 4) > '3' && ch(grid, 4) < ':' && ch(grid, 4) != '6') || ((ch(grid, 3) == '6' || ch(grid, 3) == '7') && ch(grid, 4) > '4' && ch(grid, 4) < '9') || (ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "97" || slice(grid, 3, 4) == "86" || slice(grid, 3, 4) == "87" || slice(grid, 3, 4) == "31")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "BL" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "22")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "RO" && (slice(grid, 3, 4) == "61" || slice(grid, 3, 4) == "71")) {
          lgvalid = false;
        }
      }
    }
label_2:
  if (ch(callsign, 1) == 'L') {
    if (ch(callsign, 2) > '@' && ch(callsign, 2) < 'O') {
      if ((slice(grid, 1, 2) == "JO" && ((ch(grid, 3) > '1' && ch(grid, 3) < '7' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "37")) || (slice(grid, 1, 2) == "JP" && ch(grid, 3) > '1' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "KP" && ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KQ" && ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 1, 4) == "JQ90") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "JP" && ((ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < '7') || slice(grid, 3, 4) == "23" || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "58" || slice(grid, 3, 4) == "59" || slice(grid, 3, 4) == "69" || slice(grid, 3, 4) == "97")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "KP" && ch(grid, 4) == '8' && ch(grid, 3) > '2' && ch(grid, 3) < '6') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "KQ" && (slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "51")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 2) > 'N' && ch(callsign, 2) < 'X') {
      if ((slice(grid, 1, 2) == "GF" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FF" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "FG" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "GG" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (slice(grid, 1, 2) == "FE" && ch(grid, 3) > '2' && ch(grid, 3) < '9') || (slice(grid, 1, 2) == "FD" && ch(grid, 3) > '2' && ch(grid, 3) < '8' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "GC07" || slice(grid, 1, 4) == "GC16") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "GF" && (slice(grid, 3, 4) == "16" || slice(grid, 3, 4) == "17")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FG" && (slice(grid, 3, 4) == "56" || slice(grid, 3, 4) == "57")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "GG" && (slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "35")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FE" && ((ch(grid, 3) == '8' && ch(grid, 4) > '/' && ch(grid, 4) < '7') || (ch(grid, 3) == '3' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "70" || slice(grid, 3, 4) == "73")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FD" && ((ch(grid, 3) > '2' && ch(grid, 3) < '5' && ch(grid, 4) > '4' && ch(grid, 4) < '8') || (ch(grid, 3) == '7' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (ch(grid, 3) == '6' && ch(grid, 4) > '6' && ch(grid, 4) < ':'))) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "LX") {
      if ((slice(grid, 1, 2) == "JN" && (slice(grid, 3, 4) == "29" || slice(grid, 3, 4) == "39")) || (slice(grid, 1, 2) == "JO" && (slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "30"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "LY") {
      if (slice(grid, 1, 2) == "KO" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '1' && ch(grid, 4) < '7') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "LZ") {
      if (slice(grid, 1, 2) == "KN" && ch(grid, 3) > '0' && ch(grid, 3) < '5' && ch(grid, 4) > '0' && ch(grid, 4) < '5') {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > '0' && ch(callsign, 2) < ':') {
      if ((slice(grid, 1, 2) == "GF" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FF" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "FG" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "GG" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (slice(grid, 1, 2) == "FE" && ch(grid, 3) > '2' && ch(grid, 3) < '9') || (slice(grid, 1, 2) == "FD" && ch(grid, 3) > '2' && ch(grid, 3) < '8' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "GC07" || slice(grid, 1, 4) == "GC16") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "GF" && (slice(grid, 3, 4) == "16" || slice(grid, 3, 4) == "17")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FG" && (slice(grid, 3, 4) == "56" || slice(grid, 3, 4) == "57")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "GG" && (slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "35")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FE" && ((ch(grid, 3) == '8' && ch(grid, 4) > '/' && ch(grid, 4) < '7') || (ch(grid, 3) == '3' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "70" || slice(grid, 3, 4) == "73")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FD" && ((ch(grid, 3) > '2' && ch(grid, 3) < '5' && ch(grid, 4) > '4' && ch(grid, 4) < '8') || (ch(grid, 3) == '7' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (ch(grid, 3) == '6' && ch(grid, 4) > '6' && ch(grid, 4) < ':'))) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "L0") {
      lwrongcall = true;
      return;
    }
  } else if (ch(callsign, 1) == 'M') {
    if ((slice(grid, 1, 2) == "IO" && ch(grid, 3) > '4' && ch(grid, 3) < ':') || (slice(grid, 1, 3) == "JO0" && ch(grid, 4) > '/' && ch(grid, 4) < '4') || slice(grid, 1, 4) == "IP90" || slice(grid, 1, 4) == "IN69" || slice(grid, 1, 4) == "IN89" || slice(grid, 1, 4) == "IO37") {
      lgvalid = true;
    }
    if (lgvalid && slice(grid, 1, 2) == "IO") {
      if ((slice(grid, 1, 3) == "IO5" && ch(grid, 4) != '4' && ch(grid, 4) != '7') || (slice(grid, 1, 3) == "IO6" && ch(grid, 4) > '/' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "96" || slice(grid, 3, 4) == "98") {
        lgvalid = false;
      }
    }
  } else if (ch(callsign, 1) == 'N') {
    if (ch(callsign, 2) > '/' && ch(callsign, 2) < ':' && ch(callsign, 3) > '/' && ch(callsign, 3) < ':') {
      return;
    }
    if ((slice(grid, 1, 2) == "FM" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "EL" && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "DL" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "EM" || slice(grid, 1, 2) == "DM" || slice(grid, 1, 2) == "EN" || (slice(grid, 1, 2) == "CM" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FN" && ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "DN" && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "CN" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "CO" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FK" && ch(grid, 3) > '5' && ch(grid, 3) < '8' && ch(grid, 4) > '6' && ch(grid, 4) < '9') || slice(grid, 1, 2) == "BP" || (slice(grid, 1, 2) == "BO" && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "AP" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || slice(grid, 1, 4) == "FK28" || (slice(grid, 1, 2) == "AO" && ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "BL" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "BK" && (slice(grid, 3, 4) == "19" || slice(grid, 3, 4) == "28" || slice(grid, 3, 4) == "29")) || (slice(grid, 1, 2) == "AL" && (slice(grid, 3, 4) == "91" || slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "18" || slice(grid, 3, 4) == "27" || slice(grid, 3, 4) == "36" || slice(grid, 3, 4) == "45" || slice(grid, 3, 4) == "64" || slice(grid, 3, 4) == "73" || slice(grid, 3, 4) == "93")) || (slice(grid, 1, 3) == "QK2" && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "AO" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '0' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "RO" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '0' && ch(grid, 4) < '3') || slice(grid, 3, 4) == "63")) || slice(grid, 1, 4) == "AP30" || slice(grid, 1, 4) == "QK36" || slice(grid, 1, 4) == "QL20" || slice(grid, 1, 4) == "RK39" || slice(grid, 1, 4) == "AK56" || slice(grid, 1, 4) == "AH48" || slice(grid, 1, 4) == "JA00" || slice(grid, 1, 4) == "RB32" || slice(grid, 1, 4) == "CP00") {
      lgvalid = true;
    }
    if (lgvalid) {
      if (slice(grid, 1, 2) == "FM" && (slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "23" || slice(grid, 3, 4) == "24")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "EL" && ((ch(grid, 3) > '1' && ch(grid, 3) < '8' && ch(grid, 4) > '3' && ch(grid, 4) < '9' && slice(grid, 3, 4) != "28" && slice(grid, 3, 4) != "58") || slice(grid, 3, 4) == "04" || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "14")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "DL" && (slice(grid, 3, 4) == "78" || slice(grid, 3, 4) == "88")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "DM" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "EN" && ((ch(grid, 3) == '9' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || ((ch(grid, 3) == '8' || ch(grid, 3) == '7') && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) == '9' && ch(grid, 3) != '2'))) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "CM" && (slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "85")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "FN" && ((ch(grid, 4) == '7' && ch(grid, 3) > '/' && ch(grid, 3) < '5') || (ch(grid, 4) == '6' && ch(grid, 3) > '/' && ch(grid, 3) < '4') || (ch(grid, 3) == '6' && ch(grid, 4) > '0' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "04" || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "15" || slice(grid, 3, 4) == "52")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "CO" && ((ch(grid, 3) == '0' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || (ch(grid, 3) == '1' && ch(grid, 4) > '3' && ch(grid, 4) < '7') || (ch(grid, 3) == '4' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "39")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "BO" && ((ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < '9') || (ch(grid, 3) == '4' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "14" || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "35")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "AP" && ((ch(grid, 3) == '4' && ((ch(grid, 4) > '/' && ch(grid, 4) < '3') || (ch(grid, 4) > '3' && ch(grid, 4) < ':'))) || (ch(grid, 3) == '5' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "63" || slice(grid, 3, 4) == "67" || slice(grid, 3, 4) == "69" || slice(grid, 3, 4) == "79")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "AO" && ((ch(grid, 3) == '4' && ((ch(grid, 4) > '2' && ch(grid, 4) < '7') || ch(grid, 4) == '8' || ch(grid, 4) == '9')) || (ch(grid, 3) == '5' && ch(grid, 4) > '3' && ch(grid, 4) < ':' && ch(grid, 4) != '6') || ((ch(grid, 3) == '6' || ch(grid, 3) == '7') && ch(grid, 4) > '4' && ch(grid, 4) < '9') || (ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "97" || slice(grid, 3, 4) == "86" || slice(grid, 3, 4) == "87" || slice(grid, 3, 4) == "31")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "BL" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "22")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "RO" && (slice(grid, 3, 4) == "61" || slice(grid, 3, 4) == "71")) {
        lgvalid = false;
      }
    }
  } else if (ch(callsign, 1) == 'O') {
    if (ch(callsign, 2) > 'E' && ch(callsign, 2) < 'K') {
      if ((slice(grid, 1, 2) == "KP" && ch(grid, 3) > '/' && ch(grid, 3) < '6') || slice(grid, 1, 4) == "JP90" || slice(grid, 1, 4) == "KO09" || slice(grid, 1, 4) == "KO19" || slice(grid, 1, 4) == "KQ30") {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'M' && ch(callsign, 2) < 'U') {
      if ((slice(grid, 1, 2) == "JO" && (slice(grid, 3, 4) == "10" || slice(grid, 3, 4) == "11" || slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "30")) || slice(grid, 1, 4) == "JN29") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "OE") {
      if (slice(grid, 1, 2) == "JN" && ch(grid, 3) > '3' && ch(grid, 3) < '9' && ch(grid, 4) > '5' && ch(grid, 4) < '9') {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) == 'K' || ch(callsign, 2) == 'L') {
      if ((slice(grid, 1, 2) == "JO" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) == '0') || (slice(grid, 1, 2) == "JN" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "JO71") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "OM") {
      if ((slice(grid, 1, 2) == "JN" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KN" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '7' && ch(grid, 4) < ':')) {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) == 'Z' || ch(callsign, 2) == 'U' || ch(callsign, 2) == 'V') {
      if (slice(grid, 1, 2) == "JO" && ((ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "75" || slice(grid, 3, 4) == "76")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "OD") {
      if (slice(grid, 1, 2) == "KM" && (slice(grid, 3, 4) == "73" || slice(grid, 3, 4) == "74" || slice(grid, 3, 4) == "84")) {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > '@' && ch(callsign, 2) < 'D') {
      if ((slice(grid, 1, 2) == "FH" && ch(grid, 3) > '0' && ch(grid, 3) < '6' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FI" && ch(grid, 3) > '/' && ch(grid, 3) < '5') || (slice(grid, 1, 3) == "EI9" && ch(grid, 4) > '2' && ch(grid, 4) < '7')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "OX" || slice(callsign, 1, 2) == "XP") {
      if ((slice(grid, 1, 2) == "GP" && ch(grid, 3) > '1' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "HP" && ch(grid, 3) > '/' && ch(grid, 3) < '9' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "HQ90" || (slice(grid, 1, 2) == "GQ" && (slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "41" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "14" || slice(grid, 3, 4) == "12")) || (slice(grid, 1, 2) == "FQ" && (slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "67" || slice(grid, 3, 4) == "57" || slice(grid, 3, 4) == "56" || slice(grid, 3, 4) == "38"))) {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > '/' && ch(callsign, 2) < ':') {
      lwrongcall = true;
      return;
    }
  } else if (ch(callsign, 1) == 'P') {
    if (ch(callsign, 2) > '@' && ch(callsign, 2) < 'J') {
      if (((slice(grid, 1, 3) == "JO2" || slice(grid, 1, 3) == "JO3") && ch(grid, 4) > '/' && ch(grid, 4) < '4') || slice(grid, 1, 4) == "JO11") {
        lgvalid = true;
      }
    } else if (ch(callsign, 1) == 'P' && ch(callsign, 2) > 'O' && ch(callsign, 2) < 'Z') {
      if ((slice(grid, 1, 2) == "GG" && ((ch(grid, 3) > '0' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "09")) || (slice(grid, 1, 2) == "HI" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '6' && slice(grid, 3, 4) != "25") || slice(grid, 3, 4) == "06" || slice(grid, 3, 4) == "07" || slice(grid, 3, 4) == "36")) || (slice(grid, 1, 2) == "HH" && ch(grid, 3) > '/' && ch(grid, 3) < '2') || (slice(grid, 1, 2) == "GH" && slice(grid, 3, 4) != "01") || slice(grid, 1, 2) == "GI" || (slice(grid, 1, 2) == "GF" && ((ch(grid, 3) > '0' && ch(grid, 3) < '5' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "36")) || (slice(grid, 1, 2) == "GJ" && ((ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "04" || slice(grid, 3, 4) == "44")) || (slice(grid, 1, 2) == "FJ" && ((ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "95")) || (slice(grid, 1, 2) == "FI" && ch(grid, 3) > '2' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "FH" && ((ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "93" || slice(grid, 3, 4) == "94" || slice(grid, 3, 4) == "95")) || slice(grid, 1, 4) == "HG59" || slice(grid, 1, 4) == "HJ50") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "GG" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '6' && slice(grid, 3, 4) != "64" && slice(grid, 3, 4) != "65") || (ch(grid, 3) == '1' && ch(grid, 4) > '1' && ch(grid, 4) < '7'))) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "HH" && (ch(grid, 3) == '1' && ch(grid, 4) > '/' && ch(grid, 4) < '7')) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "GI" && ((ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "79")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "GF" && (slice(grid, 3, 4) == "17" || slice(grid, 3, 4) == "27")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "GJ" && (slice(grid, 3, 4) == "13" || slice(grid, 3, 4) == "23" || slice(grid, 3, 4) == "52" || slice(grid, 3, 4) == "53")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FJ" && ch(grid, 3) > '4' && ch(grid, 3) < '7' && ch(grid, 4) > '1' && ch(grid, 4) < '5') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FI" && ch(grid, 3) > '2' && ch(grid, 3) < '5' && ch(grid, 4) > '5' && ch(grid, 4) < ':') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FH" && ((ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '5' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "68")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "P2") {
      if ((slice(grid, 1, 2) == "QI" && ch(grid, 3) > '/' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "QH" && ch(grid, 3) > '2' && ch(grid, 3) < '8' && ch(grid, 4) > '7' && ch(grid, 4) < ':')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "P3") {
      if ((slice(grid, 1, 3) == "KM6" && (ch(grid, 4) == '4' || ch(grid, 4) == '5')) || (slice(grid, 1, 3) == "KM7" && (ch(grid, 4) == '4' || ch(grid, 4) == '5'))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "P4" && (slice(grid, 1, 4) == "FK42" || slice(grid, 1, 4) == "FK52")) {
      lgvalid = true;
    } else if (ch(callsign, 2) > '4' && ch(callsign, 2) < ':') {
      if ((slice(grid, 1, 2) == "PM" && ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "PN" && ch(grid, 3) > '1' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '3')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "PJ" && ((ch(callsign, 3) == '2' || ch(callsign, 3) == '4') && slice(grid, 1, 4) == "FK52") || ((ch(callsign, 3) == '5' || ch(callsign, 3) == '6') && slice(grid, 1, 4) == "FK87") || ((ch(callsign, 3) == '7' || ch(callsign, 3) == '8' || ch(callsign, 3) == '0') && slice(grid, 1, 4) == "FK88")) {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "PZ") {
      if (slice(grid, 1, 2) == "GJ" && ch(grid, 3) > '0' && ch(grid, 3) < '3' && ch(grid, 4) > '1' && ch(grid, 4) < '6') {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'J' && ch(callsign, 2) < 'P') {
      if ((slice(grid, 1, 2) == "NJ" && ch(grid, 3) > '5' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "NI" && ch(grid, 3) > '8' && ch(grid, 3) < ':' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "OI" && !(ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '4') && !(ch(grid, 3) > '1' && ch(grid, 3) < '8' && ch(grid, 4) == '0')) || (slice(grid, 1, 2) == "PI" && !(ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) == '0')) || (slice(grid, 1, 2) == "OJ" && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "PJ" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "QI" && ch(grid, 3) > '/' && ch(grid, 3) < '1' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "PH" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) == '9')) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "PJ" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "44")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "NJ" && ((ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (ch(grid, 3) == '7' && ((ch(grid, 4) > '5' && ch(grid, 4) < ':') || ch(grid, 4) == '0' || ch(grid, 4) == '1' || ch(grid, 4) == '3')) || (ch(grid, 3) == '8' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "95")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OJ" && ((ch(grid, 3) > '4' && ch(grid, 3) < '7' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || (ch(grid, 3) == '1' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "04")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OI" && ((ch(grid, 4) == '1' && ch(grid, 3) > '1' && ch(grid, 3) < '5') || (ch(grid, 4) == '5' && ch(grid, 3) > '2' && ch(grid, 3) < '6') || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "39")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PI" && (slice(grid, 3, 4) == "61" || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "71" || slice(grid, 3, 4) == "83" || slice(grid, 3, 4) == "25" || slice(grid, 3, 4) == "35" || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "29")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "P1" || slice(callsign, 1, 2) == "P0") {
      lwrongcall = true;
      return;
    }
  } else if (ch(callsign, 1) == 'R') {
    if ((slice(grid, 1, 2) == "KO" && ((ch(grid, 3) > '2' && ch(grid, 3) < ':') || (ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '3' && ch(grid, 4) < '6'))) || slice(grid, 1, 2) == "LO" || slice(grid, 1, 2) == "MO" || slice(grid, 1, 2) == "NO" || (slice(grid, 1, 2) == "LN" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KN" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KP" && ((ch(grid, 3) > '3' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "30")) || slice(grid, 1, 2) == "NP" || slice(grid, 1, 2) == "OO" || slice(grid, 1, 2) == "PO" || slice(grid, 1, 2) == "LP" || slice(grid, 1, 2) == "MP" || (slice(grid, 1, 2) == "PN" && ((ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "39" || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "52" || slice(grid, 3, 4) == "62")) || slice(grid, 1, 4) == "MQ60" || (slice(grid, 1, 2) == "QN" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "28" || slice(grid, 3, 4) == "29")) || (slice(grid, 1, 2) == "QO" && (ch(grid, 3) == '0' || ch(grid, 3) == '1' || ch(grid, 3) == '8' || ch(grid, 3) == '9' || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "59")) || slice(grid, 1, 4) == "JO94" || slice(grid, 1, 2) == "OP" || slice(grid, 1, 2) == "PP" || slice(grid, 1, 4) == "JB59" || slice(grid, 1, 4) == "NQ03" || (slice(grid, 1, 2) == "RP" && (slice(grid, 3, 4) == "59" || slice(grid, 3, 4) == "84"))) {
      lgvalid = true;
    }
    if (lgvalid) {
      if (slice(grid, 1, 2) == "KO" && ((ch(grid, 3) > '2' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "35" || slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "60" || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "61")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "LO" && (slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "60")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "LN" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) == '1') || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "48")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "MO" && ((ch(grid, 3) > '0' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (ch(grid, 3) > '1' && ch(grid, 3) < '6' && ch(grid, 4) == '3'))) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "KN" && (ch(grid, 3) > '5' && ch(grid, 3) < '9' && ((ch(grid, 4) > '6' && ch(grid, 4) < ':') || ch(grid, 4) == '3'))) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "KP" && (slice(grid, 3, 4) == "42" || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "99")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "OO" && slice(grid, 3, 4) == "00") {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "PO" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) == '0') || slice(grid, 3, 4) == "11" || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "21")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "LP" && ch(grid, 4) == '9' && ch(grid, 3) > '/' && ch(grid, 3) < '9' && ch(grid, 3) != '4' && ch(grid, 3) != '5') {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "PN" && (slice(grid, 3, 4) == "56" || slice(grid, 3, 4) == "83" || slice(grid, 3, 4) == "93" || slice(grid, 3, 4) == "94" || slice(grid, 3, 4) == "95")) {
        lgvalid = false;
      }
    }
  } else if (ch(callsign, 1) == 'S') {
    if (ch(callsign, 1) == 'S' && ch(callsign, 2) > '@' && ch(callsign, 2) < 'N') {
      if ((slice(grid, 1, 2) == "JO" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JP" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "KP" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "25"))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "JO" && (slice(grid, 3, 4) == "85" || slice(grid, 3, 4) == "95")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "JP" && ((ch(grid, 3) == '6' && ch(grid, 4) > '4' && ch(grid, 4) < '9') || (ch(grid, 3) == '7' && ch(grid, 4) > '6' && ch(grid, 4) < '9') || slice(grid, 3, 4) == "89" || slice(grid, 3, 4) == "99")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "KP" && (slice(grid, 3, 4) == "13" || slice(grid, 3, 4) == "14")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 1) == 'S' && ch(callsign, 2) > 'M' && ch(callsign, 2) < 'S') {
      if ((slice(grid, 1, 2) == "JO" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "KO" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "20")) || slice(grid, 1, 4) == "KN09" || slice(grid, 1, 4) == "KN19" || slice(grid, 1, 4) == "KO20" || slice(grid, 1, 4) == "JN99" || slice(grid, 1, 4) == "JN89") {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'U' && ch(callsign, 2) < '[') {
      if ((slice(grid, 1, 2) == "KM" && ((ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "46" || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "14") && slice(grid, 3, 4) != "05") || slice(grid, 1, 4) == "JM99" || (slice(grid, 1, 2) == "KN" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && (ch(grid, 4) == '0' || ch(grid, 4) == '1'))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "S5") {
      if (slice(grid, 1, 2) == "JN" && ch(grid, 3) > '5' && ch(grid, 3) < '9' && ch(grid, 4) > '4' && ch(grid, 4) < '7') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9V") {
      if (slice(grid, 1, 2) == "OJ" && (slice(grid, 3, 4) == "11" || slice(grid, 3, 4) == "21")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "S0") {
      if (slice(grid, 1, 2) == "IL" && ch(grid, 3) > '0' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '8') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "S2" || slice(callsign, 1, 2) == "S3") {
      if (slice(grid, 1, 2) == "NL" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '0' && ch(grid, 4) < '7') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "S4" || slice(callsign, 1, 2) == "S8") {
      if ((slice(grid, 1, 2) == "KG" && ((ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (ch(grid, 3) == '6' && ch(grid, 4) > '0' && ch(grid, 4) < '4'))) || (slice(grid, 1, 2) == "KF" && ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JF" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '4' && ch(grid, 4) < ':' && slice(grid, 3, 4) != "85") || (slice(grid, 1, 2) == "JG" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 1, 4) == "KE83" || slice(grid, 1, 4) == "KE93") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "KF" && ((ch(grid, 4) == '5' && ch(grid, 3) > '2' && ch(grid, 3) < '6') || (ch(grid, 4) == '6' && ch(grid, 3) > '3' && ch(grid, 3) < '6') || slice(grid, 3, 4) == "57")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "S7") {
      if ((slice(grid, 1, 2) == "LI" && (slice(grid, 3, 4) == "75" || slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "74" || slice(grid, 3, 4) == "64" || slice(grid, 3, 4) == "63" || slice(grid, 3, 4) == "82" || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "30")) || (slice(grid, 1, 2) == "LH" && (slice(grid, 3, 4) == "59" || slice(grid, 3, 4) == "39"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "S9" && (slice(grid, 1, 4) == "JJ30" || slice(grid, 1, 4) == "JJ31")) {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "ST") {
      if ((slice(grid, 1, 2) == "KK" && ch(grid, 3) > '0' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "KL" && ch(grid, 3) > '1' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 1, 4) == "KJ79") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "S1") {
      lwrongcall = true;
      return;
    }
  } else if (ch(callsign, 1) == 'T') {
    if (ch(callsign, 2) > '@' && ch(callsign, 2) < 'D') {
      if ((slice(grid, 1, 2) == "KM" && ((ch(grid, 3) > '2' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "85")) || (slice(grid, 1, 2) == "KN" && ((ch(grid, 3) > '2' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "72")) || (slice(grid, 1, 2) == "LN" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "LM" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '5' && ch(grid, 4) < ':' && slice(grid, 3, 4) != "16")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TF") {
      if ((slice(grid, 1, 2) == "HP" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '2' && ch(grid, 4) < '7') || (slice(grid, 1, 2) == "IP" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '2' && ch(grid, 4) < '7')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TK") {
      if (slice(grid, 1, 3) == "JN4" && ch(grid, 4) > '0' && ch(grid, 4) < '4') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TR") {
      if (slice(callsign, 1, 5) == "TR8CA" || (slice(grid, 1, 2) == "JJ" && ((ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "52" || slice(grid, 3, 4) == "62")) || (slice(grid, 1, 2) == "JI" && ((ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "56"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TS") {
      if (slice(grid, 1, 2) == "JM" && ch(grid, 3) > '2' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '8') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TG" || slice(callsign, 1, 2) == "TD") {
      if (slice(grid, 1, 2) == "EK" && ch(grid, 3) > '2' && ch(grid, 3) < '6' && ch(grid, 4) > '2' && ch(grid, 4) < '8') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TI" || slice(callsign, 1, 2) == "TE") {
      if ((slice(grid, 1, 2) == "EK" && ch(grid, 3) > '6' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "EJ" && ch(grid, 3) > '6' && ch(grid, 3) < '9' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "EJ65") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "T6") {
      if ((slice(grid, 1, 2) == "MM" && ch(grid, 4) > '/' && ch(grid, 4) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "ML" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && slice(grid, 3, 4) == "9")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "T7" && slice(grid, 1, 4) == "JN63") {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "T8") {
      if (slice(grid, 1, 2) == "PJ" && ch(grid, 3) > '4' && ch(grid, 3) < '8' && ch(grid, 4) > '1' && ch(grid, 4) < '9') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "T4") {
      if ((slice(grid, 1, 2) == "EL" && ch(grid, 3) > '6' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "FL" && ch(grid, 3) > '/' && ch(grid, 3) < '3') || slice(grid, 1, 4) == "FK19") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TO") {
      if ((slice(grid, 1, 3) == "FK9" && (ch(grid, 4) == '6' || ch(grid, 4) == '5' || ch(grid, 4) == '4')) || (slice(grid, 1, 3) == "FK8" && (ch(grid, 4) == '8' || ch(grid, 4) == '7')) || (slice(grid, 1, 2) == "LG" && (slice(grid, 3, 4) == "79" || slice(grid, 3, 4) == "78"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "T2") {
      if ((slice(grid, 1, 2) == "RI" && (slice(grid, 3, 4) == "91" || slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "82" || slice(grid, 3, 4) == "83" || slice(grid, 3, 4) == "84")) || slice(grid, 1, 4) == "RH99") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "T3") {
      if (ch(grid, 1) == 'R') {
        if ((slice(grid, 1, 3) == "RJ6" && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (slice(grid, 1, 3) == "RI7" && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "RI49" || slice(grid, 1, 4) == "RI87" || slice(grid, 1, 4) == "RI88") {
          lgvalid = true;
        }
      } else if (ch(grid, 1) == 'A') {
        if ((slice(grid, 1, 2) == "AI" && ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '4' && ch(grid, 4) < '8') || slice(grid, 1, 4) == "AJ94") {
          lgvalid = true;
        }
      } else if (ch(grid, 1) == 'B') {
        if (slice(grid, 1, 4) == "BJ03" || slice(grid, 1, 4) == "BJ11" || slice(grid, 1, 4) == "BJ12" || (slice(grid, 1, 3) == "BI2" && ch(grid, 4) > '3' && ch(grid, 4) < '7') || slice(grid, 1, 4) == "BH48" || slice(grid, 1, 4) == "BH49" || slice(grid, 1, 4) == "BI40") {
          lgvalid = true;
        }
      }
    } else if (slice(callsign, 1, 2) == "TJ") {
      if ((slice(grid, 1, 2) == "JJ" && ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JK" && ch(grid, 3) > '5' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '4')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TL") {
      if ((slice(grid, 1, 2) == "JJ" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KJ" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KK" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) == '0')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TN") {
      if ((slice(grid, 1, 2) == "JI" && ch(grid, 3) > '4' && ch(grid, 3) < '9' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JJ" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '4')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TT") {
      if ((slice(grid, 1, 2) == "JK" && ch(grid, 3) > '6' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "JJ" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "KJ09" || (slice(grid, 1, 2) == "KK" && ch(grid, 3) > '/' && ch(grid, 3) < '2') || (slice(grid, 1, 2) == "JL" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (slice(grid, 1, 2) == "KL" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '2')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TU") {
      if ((slice(grid, 1, 2) == "IJ" && ch(grid, 3) > '4' && ch(grid, 3) < '9' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "IK" && (slice(grid, 3, 4) == "60" || slice(grid, 3, 4) == "70" || slice(grid, 3, 4) == "50"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TY") {
      if ((slice(grid, 1, 2) == "JJ" && ch(grid, 3) > '/' && ch(grid, 3) < '1' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JK" && ch(grid, 3) > '/' && ch(grid, 3) < '1' && ch(grid, 4) > '/' && ch(grid, 4) < '3')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TZ") {
      if ((slice(grid, 1, 2) == "IK" && ch(grid, 3) > '2' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "JK" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "IL" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "JL" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '2')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TX") {
      if ((slice(grid, 1, 2) == "QH" && (slice(grid, 3, 4) == "90" || slice(grid, 3, 4) == "91")) || slice(grid, 1, 4) == "QG98" || (slice(grid, 1, 2) == "BH" && ch(grid, 3) > '1' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (slice(grid, 1, 2) == "BG" && ch(grid, 3) > '1' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 3) == "BI9" && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "CI" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01")) || slice(grid, 1, 4) == "CH09" || slice(grid, 1, 4) == "DK50") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "TW") {
      if (slice(grid, 1, 2) == "AH" && (slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "16")) {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) == 'H' || ch(callsign, 2) == 'M' || ch(callsign, 2) == 'P' || ch(callsign, 2) == 'Q' || ch(callsign, 2) == 'V') {
      if ((slice(grid, 1, 2) == "IN" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 3) == "IN7" && (ch(grid, 4) == '7' || ch(grid, 4) == '8')) || (slice(grid, 1, 2) == "JN" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 3) == "JN4" && (ch(grid, 4) == '8' || ch(grid, 4) == '9')) || slice(grid, 1, 4) == "JN02" || slice(grid, 1, 4) == "JN12" || slice(grid, 1, 4) == "IN92" || (slice(grid, 1, 2) == "JO" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "10" || slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "11"))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "IN" && (slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "85")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "T1" || slice(callsign, 1, 2) == "T0") {
      lwrongcall = true;
      return;
    }
  } else if (ch(callsign, 1) == 'U') {
    if ((ch(callsign, 2) > '@' && ch(callsign, 2) < 'J') || (ch(callsign, 2) > '/' && ch(callsign, 2) < ':')) {
      if ((slice(grid, 1, 2) == "KO" && ((ch(grid, 3) > '2' && ch(grid, 3) < ':') || (ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '3' && ch(grid, 4) < '6'))) || slice(grid, 1, 2) == "LO" || slice(grid, 1, 2) == "MO" || slice(grid, 1, 2) == "NO" || (slice(grid, 1, 2) == "LN" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KN" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KP" && ((ch(grid, 3) > '3' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "30")) || slice(grid, 1, 2) == "NP" || slice(grid, 1, 2) == "OO" || slice(grid, 1, 2) == "PO" || slice(grid, 1, 2) == "LP" || slice(grid, 1, 2) == "MP" || (slice(grid, 1, 2) == "PN" && ((ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "39" || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "52" || slice(grid, 3, 4) == "62")) || slice(grid, 1, 4) == "MQ60" || (slice(grid, 1, 2) == "QN" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "28" || slice(grid, 3, 4) == "29")) || (slice(grid, 1, 2) == "QO" && (ch(grid, 3) == '0' || ch(grid, 3) == '1' || ch(grid, 3) == '8' || ch(grid, 3) == '9' || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "59")) || slice(grid, 1, 4) == "JO94" || slice(grid, 1, 2) == "OP" || slice(grid, 1, 2) == "PP" || slice(grid, 1, 4) == "JB59" || slice(grid, 1, 4) == "NQ03" || (slice(grid, 1, 2) == "RP" && (slice(grid, 3, 4) == "59" || slice(grid, 3, 4) == "84"))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "KO" && ((ch(grid, 3) > '2' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "35" || slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "60" || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "61")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "LO" && (slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "60")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "LN" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) == '1') || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "48")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "MO" && ((ch(grid, 3) > '0' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (ch(grid, 3) > '1' && ch(grid, 3) < '6' && ch(grid, 4) == '3'))) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "KN" && (ch(grid, 3) > '5' && ch(grid, 3) < '9' && ((ch(grid, 4) > '6' && ch(grid, 4) < ':') || ch(grid, 4) == '3'))) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "KP" && (slice(grid, 3, 4) == "42" || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "99")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OO" && slice(grid, 3, 4) == "00") {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PO" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) == '0') || slice(grid, 3, 4) == "11" || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "21")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "LP" && ch(grid, 4) == '9' && ch(grid, 3) > '/' && ch(grid, 3) < '9' && ch(grid, 3) != '4' && ch(grid, 3) != '5') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PN" && (slice(grid, 3, 4) == "56" || slice(grid, 3, 4) == "83" || slice(grid, 3, 4) == "93" || slice(grid, 3, 4) == "94" || slice(grid, 3, 4) == "95")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 2) > 'Q' && ch(callsign, 2) < '[') {
      if ((slice(grid, 1, 2) == "KO" && ((ch(grid, 3) > '0' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "52" || slice(grid, 3, 4) == "62")) || (slice(grid, 1, 2) == "KN" && ch(grid, 3) > '0' && ch(grid, 3) < ':' && ch(grid, 4) > '4' && ch(grid, 4) < ':')) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "KO" && (slice(grid, 3, 4) == "81" || slice(grid, 3, 4) == "91")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "KN" && ((ch(grid, 3) > '0' && ch(grid, 3) < '4' && ch(grid, 4) > '4' && ch(grid, 4) < '7') || slice(grid, 3, 4) == "95" || slice(grid, 3, 4) == "96")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 2) > 'M' && ch(callsign, 2) < 'R') {
      if ((slice(grid, 1, 2) == "MN" && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "MO" && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "LN" && ch(grid, 3) > '2' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "LO" && ch(grid, 3) > '2' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "NN" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "NO" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "10" || slice(grid, 3, 4) == "20")) || (slice(grid, 1, 2) == "MO" && (slice(grid, 3, 4) == "45" || slice(grid, 3, 4) == "55")) || (slice(grid, 1, 2) == "MN" && ((ch(grid, 4) == '1' && (ch(grid, 3) == '3' || ch(grid, 3) == '4' || ch(grid, 3) == '5')) || slice(grid, 3, 4) == "40")) || (slice(grid, 1, 3) == "NN0" && ch(grid, 4) > '1' && ch(grid, 4) < '5')) {
        lgvalid = true;
      }
    }
  } else if (ch(callsign, 1) == 'V') {
    if ((ch(callsign, 2) > '@' && ch(callsign, 2) < 'H') || slice(callsign, 1, 2) == "VO" || slice(callsign, 1, 2) == "VX" || slice(callsign, 1, 2) == "VY") {
      if ((slice(grid, 1, 2) == "CN" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FN" && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "EN" && ch(grid, 4) > '0' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "CO" || (slice(grid, 1, 2) == "GN" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "FO" || slice(grid, 1, 2) == "EO" || slice(grid, 1, 2) == "DO" || (slice(grid, 1, 2) == "GO" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "DN" && ch(grid, 4) > '8' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "CP" || slice(grid, 1, 2) == "DP" || slice(grid, 1, 2) == "EP" || (slice(grid, 1, 2) == "FP" && ((ch(grid, 3) > '/' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "96")) || (slice(grid, 1, 2) == "EQ" && (slice(grid, 3, 4) == "15" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "71" || slice(grid, 3, 4) == "73" || slice(grid, 3, 4) == "79" || slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "86" || slice(grid, 3, 4) == "96")) || (slice(grid, 1, 2) == "FR" && (slice(grid, 3, 4) == "71" || slice(grid, 3, 4) == "82")) || (slice(grid, 1, 2) == "FQ" && (slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "12")) || slice(grid, 1, 4) == "GN03" || slice(grid, 1, 4) == "FN93" || slice(grid, 1, 4) == "ER60" || slice(grid, 1, 4) == "DQ10" || slice(grid, 1, 4) == "CQ71") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "EO" && ((ch(grid, 3) > '3' && ch(grid, 3) < '9' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || (ch(grid, 4) == '7' && ch(grid, 3) > '4' && ch(grid, 3) < '9') || slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "86")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "CO" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "18" || slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "40")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "EP" && ((ch(grid, 4) == '0' && ch(grid, 3) > '2' && ch(grid, 3) < ':') || (ch(grid, 4) == '1' && ch(grid, 3) > '3' && ch(grid, 3) < '9') || (ch(grid, 4) == '2' && ch(grid, 3) > '4' && ch(grid, 3) < '8'))) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "CN" && (slice(grid, 3, 4) == "68" || slice(grid, 3, 4) == "98")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FP" && ((ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '4' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "04")) {
          lgvalid = false;
        }
      }
    } else if ((ch(callsign, 2) > 'G' && ch(callsign, 2) < 'O') || slice(callsign, 1, 2) == "VZ") {
      if ((slice(grid, 1, 2) == "QG" && ch(grid, 3) > '/' && ch(grid, 3) < '7') || (slice(grid, 1, 2) == "QF" && ch(grid, 3) > '/' && ch(grid, 3) < '7') || (slice(grid, 1, 2) == "PF" && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "OF" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "QE" && ((ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "19")) || (slice(grid, 1, 2) == "QH" && ch(grid, 3) > '/' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "OG" && ch(grid, 3) > '5' && ch(grid, 3) < ':') || slice(grid, 1, 2) == "PG" || slice(grid, 1, 2) == "PH" || (slice(callsign, 1, 3) == "VK0" && ((slice(grid, 1, 2) == "MD" && (slice(grid, 3, 4) == "66" || slice(grid, 3, 4) == "67")) || (slice(grid, 1, 2) == "QD" && (slice(grid, 3, 4) == "95" || slice(grid, 3, 4) == "94")))) || (slice(callsign, 1, 3) == "VK9" && ((slice(grid, 1, 2) == "NH" && (slice(grid, 3, 4) == "87" || slice(grid, 3, 4) == "88")) || slice(grid, 1, 4) == "QF98" || slice(grid, 1, 4) == "OH29" || (slice(grid, 1, 2) == "RG" && (slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31")) || (slice(grid, 1, 2) == "QH" && ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) > '1' && ch(grid, 4) < '5')))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "QF" && ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '7') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PF" && ((ch(grid, 3) > '/' && ch(grid, 3) < '8' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || (ch(grid, 4) == '5' && ch(grid, 3) > '1' && ch(grid, 3) < '7') || (ch(grid, 4) == '6' && ch(grid, 3) > '2' && ch(grid, 3) < '7') || slice(grid, 3, 4) == "57")) {
          lgvalid = false;
        } else if (slice(grid, 1, 4) == "OF74") {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "QH" && ((ch(grid, 3) == '3' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (ch(grid, 3) == '4' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "09")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OG" && (slice(grid, 3, 4) == "60" || slice(grid, 3, 4) == "69")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PH" && ((ch(grid, 3) == '0' && ch(grid, 4) > '1' && ch(grid, 4) < ':' && ch(grid, 4) != '5' && ch(grid, 4) != '6') || (ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (ch(grid, 3) == '9' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "59" || slice(grid, 3, 4) == "79" || slice(grid, 3, 4) == "89")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 2) > 'S' && ch(callsign, 2) < 'X') {
      if ((slice(grid, 1, 2) == "MJ" && (slice(grid, 3, 4) == "99" || slice(grid, 3, 4) == "89" || slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "68")) || (slice(grid, 1, 2) == "MK" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "52")) || (slice(grid, 1, 2) == "NK" && ((ch(grid, 3) == '0' && ch(grid, 4) > '1' && ch(grid, 4) < '6') || (ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "39")) || (slice(grid, 1, 2) == "ML" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "NL" && ch(grid, 3) > '/' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "MM" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 1, 4) == "NM00") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "MK" && ch(grid, 3) == '6' && ch(grid, 4) > '1' && ch(grid, 4) < '5') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "NK" && (slice(grid, 3, 4) == "26" || slice(grid, 3, 4) == "27")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "ML" && ((ch(grid, 3) > '4' && (ch(grid, 4) == '0' || ch(grid, 4) == '5' || ch(grid, 4) == '8' || ch(grid, 4) == '9')) || slice(grid, 3, 4) == "58" || slice(grid, 3, 4) == "59")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "NL" && ((ch(grid, 4) == '9' && ch(grid, 3) > '/' && ch(grid, 4) < '5') || (ch(grid, 4) == '8' && ch(grid, 3) > '0' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "40")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "MM" && (slice(grid, 3, 4) == "61" || slice(grid, 3, 4) == "62")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "VR" && (slice(grid, 1, 2) == "OL" && (slice(grid, 3, 4) == "72" || slice(grid, 3, 4) == "62"))) {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "V2") {
      if (slice(grid, 1, 2) == "FK" && (slice(grid, 3, 4) == "86" || slice(grid, 3, 4) == "97")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "V3") {
      if ((slice(grid, 1, 2) == "EK" && ch(grid, 3) == '5' && ch(grid, 4) > '4' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "EK" && ch(grid, 3) == '6' && ch(grid, 4) > '5' && ch(grid, 4) < '9')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "V4" && slice(grid, 1, 4) == "FK87") {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "V5") {
      if ((slice(grid, 1, 2) == "JG" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JH" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "KG" && (slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "09")) || (slice(grid, 1, 2) == "KH" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '3')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "V6") {
      if ((slice(grid, 1, 2) == "PJ" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "QJ" && ((ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "71" || slice(grid, 3, 4) == "73")) || (slice(grid, 1, 2) == "RJ" && (slice(grid, 3, 4) == "06" || slice(grid, 3, 4) == "15")) || slice(grid, 1, 4) == "PK90") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "V7") {
      if ((slice(grid, 1, 2) == "RJ" && ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "RK" && ((ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "39"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "V8") {
      if (slice(grid, 1, 2) == "OJ" && (slice(grid, 3, 4) == "74" || slice(grid, 3, 4) == "75")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 3) == "VP9" && slice(grid, 1, 4) == "FM72") {
      lgvalid = true;
    } else if (slice(callsign, 1, 3) == "VQ9") {
      if (slice(grid, 1, 2) == "MI" && ch(grid, 3) > '4' && ch(grid, 3) < '7' && ch(grid, 4) > '1' && ch(grid, 4) < '5') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "V9") {
      if ((slice(grid, 1, 2) == "KG" && ((ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (ch(grid, 3) == '6' && ch(grid, 4) > '0' && ch(grid, 4) < '4'))) || (slice(grid, 1, 2) == "KF" && ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JF" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '4' && ch(grid, 4) < ':' && slice(grid, 3, 4) != "85") || (slice(grid, 1, 2) == "JG" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 1, 4) == "KE83" || slice(grid, 1, 4) == "KE93") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "KF" && ((ch(grid, 4) == '5' && ch(grid, 3) > '2' && ch(grid, 3) < '6') || (ch(grid, 4) == '6' && ch(grid, 3) > '3' && ch(grid, 3) < '6') || slice(grid, 3, 4) == "57")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 3) == "VP2" && (slice(grid, 1, 2) == "FK" && (slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "86" || slice(grid, 3, 4) == "78"))) {
      lgvalid = true;
    } else if ((slice(callsign, 1, 3) == "VP5" || slice(callsign, 1, 3) == "VQ5") && (slice(grid, 1, 2) == "FL" && (slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "41"))) {
      lgvalid = true;
    } else if (slice(callsign, 1, 3) == "VP6" && (slice(grid, 1, 2) == "CG" && (slice(grid, 3, 4) == "55" || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "46" || slice(grid, 3, 4) == "75"))) {
      lgvalid = true;
    } else if (slice(callsign, 1, 3) == "VP8") {
      if ((slice(grid, 1, 2) == "FD" && ch(grid, 3) > '8' && ch(grid, 3) < ':' && ch(grid, 4) > '6' && ch(grid, 4) < '9') || slice(grid, 1, 4) == "GD86" || (slice(grid, 1, 2) == "HD" && ((slice(grid, 3, 4) == "15" || slice(grid, 3, 4) == "25" || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "16" || slice(grid, 3, 4) == "06") || (slice(grid, 1, 3) == "HD6" && (ch(grid, 4) > '/' && ch(grid, 4) < '4')) || slice(grid, 1, 4) == "HD53")) || slice(grid, 1, 4) == "GC69" || slice(grid, 1, 4) == "GC79") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "V1" || slice(callsign, 1, 2) == "V0") {
      lwrongcall = true;
      return;
    }
  } else if (ch(callsign, 1) == 'W') {
    if (ch(callsign, 2) > '/' && ch(callsign, 2) < ':' && ch(callsign, 3) > '/' && ch(callsign, 3) < ':') {
      return;
    }
    if ((slice(grid, 1, 2) == "FM" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "EL" && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "DL" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "EM" || slice(grid, 1, 2) == "DM" || slice(grid, 1, 2) == "EN" || (slice(grid, 1, 2) == "CM" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FN" && ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "DN" && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "CN" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "CO" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FK" && ch(grid, 3) > '5' && ch(grid, 3) < '8' && ch(grid, 4) > '6' && ch(grid, 4) < '9') || slice(grid, 1, 2) == "BP" || (slice(grid, 1, 2) == "BO" && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "AP" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || slice(grid, 1, 4) == "FK28" || (slice(grid, 1, 2) == "AO" && ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "BL" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "BK" && (slice(grid, 3, 4) == "19" || slice(grid, 3, 4) == "28" || slice(grid, 3, 4) == "29")) || (slice(grid, 1, 2) == "AL" && (slice(grid, 3, 4) == "91" || slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "18" || slice(grid, 3, 4) == "27" || slice(grid, 3, 4) == "36" || slice(grid, 3, 4) == "45" || slice(grid, 3, 4) == "64" || slice(grid, 3, 4) == "73" || slice(grid, 3, 4) == "93")) || (slice(grid, 1, 3) == "QK2" && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "AO" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '0' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "RO" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '0' && ch(grid, 4) < '3') || slice(grid, 3, 4) == "63")) || slice(grid, 1, 4) == "AP30" || slice(grid, 1, 4) == "QK36" || slice(grid, 1, 4) == "QL20" || slice(grid, 1, 4) == "RK39" || slice(grid, 1, 4) == "AK56" || slice(grid, 1, 4) == "AH48" || slice(grid, 1, 4) == "JA00" || slice(grid, 1, 4) == "RB32" || slice(grid, 1, 4) == "CP00") {
      lgvalid = true;
    }
    if (lgvalid) {
      if (slice(grid, 1, 2) == "FM" && (slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "23" || slice(grid, 3, 4) == "24")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "EL" && ((ch(grid, 3) > '1' && ch(grid, 3) < '8' && ch(grid, 4) > '3' && ch(grid, 4) < '9' && slice(grid, 3, 4) != "28" && slice(grid, 3, 4) != "58") || slice(grid, 3, 4) == "04" || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "14")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "DL" && (slice(grid, 3, 4) == "78" || slice(grid, 3, 4) == "88")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "DM" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "EN" && ((ch(grid, 3) == '9' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || ((ch(grid, 3) == '8' || ch(grid, 3) == '7') && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) == '9' && ch(grid, 3) != '2'))) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "CM" && (slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "85")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "FN" && ((ch(grid, 4) == '7' && ch(grid, 3) > '/' && ch(grid, 3) < '5') || (ch(grid, 4) == '6' && ch(grid, 3) > '/' && ch(grid, 3) < '4') || (ch(grid, 3) == '6' && ch(grid, 4) > '0' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "04" || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "15" || slice(grid, 3, 4) == "52")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "CO" && ((ch(grid, 3) == '0' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || (ch(grid, 3) == '1' && ch(grid, 4) > '3' && ch(grid, 4) < '7') || (ch(grid, 3) == '4' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "39")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "BO" && ((ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < '9') || (ch(grid, 3) == '4' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "14" || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "35")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "AP" && ((ch(grid, 3) == '4' && ((ch(grid, 4) > '/' && ch(grid, 4) < '3') || (ch(grid, 4) > '3' && ch(grid, 4) < ':'))) || (ch(grid, 3) == '5' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "63" || slice(grid, 3, 4) == "67" || slice(grid, 3, 4) == "69" || slice(grid, 3, 4) == "79")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "AO" && ((ch(grid, 3) == '4' && ((ch(grid, 4) > '2' && ch(grid, 4) < '7') || ch(grid, 4) == '8' || ch(grid, 4) == '9')) || (ch(grid, 3) == '5' && ch(grid, 4) > '3' && ch(grid, 4) < ':' && ch(grid, 4) != '6') || ((ch(grid, 3) == '6' || ch(grid, 3) == '7') && ch(grid, 4) > '4' && ch(grid, 4) < '9') || (ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "97" || slice(grid, 3, 4) == "86" || slice(grid, 3, 4) == "87" || slice(grid, 3, 4) == "31")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "BL" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "22")) {
        lgvalid = false;
      } else if (slice(grid, 1, 2) == "RO" && (slice(grid, 3, 4) == "61" || slice(grid, 3, 4) == "71")) {
        lgvalid = false;
      }
    }
  } else if (ch(callsign, 1) == 'X') {
    if (ch(callsign, 2) > '@' && ch(callsign, 2) < 'J') {
      if ((slice(grid, 1, 2) == "DL" && ((ch(grid, 3) > '1' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "09")) || (slice(grid, 1, 2) == "EL" && ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "EK" && ((ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "34")) || (slice(grid, 1, 2) == "DM" && ch(grid, 3) > '0' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "DK" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "DK" && (slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "28" || slice(grid, 3, 4) == "38"))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "DL" && ((ch(grid, 3) > '1' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "23" || slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "33" || slice(grid, 3, 4) == "60")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "EL" && ((ch(grid, 3) > '1' && ch(grid, 3) < '7' && ch(grid, 4) > '2' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "17" || slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "62")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "EK" && (slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "65" || slice(grid, 3, 4) == "66" || slice(grid, 3, 4) == "67" || slice(grid, 3, 4) == "39")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "DM" && ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) == '2') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "DK" && (slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "77" || slice(grid, 3, 4) == "86")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 1) == 'X' && ch(callsign, 2) > 'I' && ch(callsign, 2) < 'P') {
      if ((slice(grid, 1, 2) == "CN" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FN" && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "EN" && ch(grid, 4) > '0' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "CO" || (slice(grid, 1, 2) == "GN" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "FO" || slice(grid, 1, 2) == "EO" || slice(grid, 1, 2) == "DO" || (slice(grid, 1, 2) == "GO" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "DN" && ch(grid, 4) > '8' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "CP" || slice(grid, 1, 2) == "DP" || slice(grid, 1, 2) == "EP" || (slice(grid, 1, 2) == "FP" && ((ch(grid, 3) > '/' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "96")) || (slice(grid, 1, 2) == "EQ" && (slice(grid, 3, 4) == "15" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "71" || slice(grid, 3, 4) == "73" || slice(grid, 3, 4) == "79" || slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "86" || slice(grid, 3, 4) == "96")) || (slice(grid, 1, 2) == "FR" && (slice(grid, 3, 4) == "71" || slice(grid, 3, 4) == "82")) || (slice(grid, 1, 2) == "FQ" && (slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "12")) || slice(grid, 1, 4) == "GN03" || slice(grid, 1, 4) == "FN93" || slice(grid, 1, 4) == "ER60" || slice(grid, 1, 4) == "DQ10" || slice(grid, 1, 4) == "CQ71") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "EO" && ((ch(grid, 3) > '3' && ch(grid, 3) < '9' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || (ch(grid, 4) == '7' && ch(grid, 3) > '4' && ch(grid, 3) < '9') || slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "86")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "CO" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "18" || slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "40")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "EP" && ((ch(grid, 4) == '0' && ch(grid, 3) > '2' && ch(grid, 3) < ':') || (ch(grid, 4) == '1' && ch(grid, 3) > '3' && ch(grid, 3) < '9') || (ch(grid, 4) == '2' && ch(grid, 3) > '4' && ch(grid, 3) < '8'))) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "CN" && (slice(grid, 3, 4) == "68" || slice(grid, 3, 4) == "98")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FP" && ((ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '4' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "04")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "XT") {
      if ((slice(grid, 1, 2) == "IK" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "JK" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '0' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "IJ" && (slice(grid, 3, 4) == "79" || slice(grid, 3, 4) == "89"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "XU") {
      if (slice(grid, 1, 2) == "OK" && ch(grid, 3) > '0' && ch(grid, 3) < '4' && ch(grid, 4) > '/' && ch(grid, 4) < '5') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "XV") {
      if ((slice(grid, 1, 2) == "OL" && ch(grid, 3) > '0' && ch(grid, 3) < '4' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "OK" && ch(grid, 3) > '0' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "OJ" && ch(grid, 3) > '1' && ch(grid, 3) < '4' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "OJ19" || slice(grid, 1, 4) == "OL41" || slice(grid, 1, 4) == "OL23") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "XW") {
      if ((slice(grid, 1, 2) == "OK" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "OL" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '3')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 3) == "XX9" && slice(grid, 1, 4) == "OL62") {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "XQ" || slice(callsign, 1, 2) == "XR") {
      if ((slice(grid, 1, 2) == "FF" && ch(grid, 3) > '2' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "FG" && ((ch(grid, 3) > '3' && ch(grid, 3) < '6') || (ch(grid, 3) == '6' && ch(grid, 4) > '4' && ch(grid, 4) < '8'))) || (slice(grid, 1, 2) == "FE" && ch(grid, 3) > '1' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "FH" && ((ch(grid, 3) > '3' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "52")) || (slice(grid, 1, 2) == "FD" && ((ch(grid, 3) > '1' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "53" || slice(grid, 3, 4) == "85")) || slice(grid, 1, 4) == "EF96" || slice(grid, 1, 4) == "FF06" || slice(grid, 1, 4) == "EG93" || slice(grid, 1, 4) == "FG03" || slice(grid, 1, 4) == "DG52" || slice(grid, 1, 4) == "DG73") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "FF" && ch(grid, 3) == '3' && ch(grid, 4) > '5' && ch(grid, 4) < ':') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FE" && (slice(grid, 3, 4) == "40" || slice(grid, 3, 4) == "41" || slice(grid, 3, 4) == "29")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "XY" || slice(callsign, 1, 2) == "XZ") {
      if ((slice(grid, 1, 2) == "NK" && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "NL" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "OL" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "XP") {
      if ((slice(grid, 1, 2) == "GP" && ch(grid, 3) > '1' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "HP" && ch(grid, 3) > '/' && ch(grid, 3) < '9' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "HQ90" || (slice(grid, 1, 2) == "GQ" && (slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "41" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "14" || slice(grid, 3, 4) == "12")) || (slice(grid, 1, 2) == "FQ" && (slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "67" || slice(grid, 3, 4) == "57" || slice(grid, 3, 4) == "56" || slice(grid, 3, 4) == "38"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "XS") {
      if (slice(grid, 1, 2) == "OL" || (slice(grid, 1, 2) == "PL" && ch(grid, 3) > '/' && ch(grid, 3) < '2') || slice(grid, 1, 2) == "OM" || (slice(grid, 1, 2) == "PM" && ch(grid, 3) > '/' && ch(grid, 3) < '3') || (slice(grid, 1, 2) == "PN" && ch(grid, 3) > '/' && ch(grid, 3) < '8') || (slice(grid, 1, 2) == "PO" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (slice(grid, 1, 2) == "OK" && (slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "58" || slice(grid, 3, 4) == "59")) || slice(grid, 1, 2) == "ON" || slice(grid, 1, 4) == "OO90" || slice(grid, 1, 4) == "OO91" || slice(grid, 1, 2) == "NN" || slice(grid, 1, 2) == "NM" || (slice(grid, 1, 2) == "NL" && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "MN" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "MM" && ch(grid, 3) > '5' && ch(grid, 3) < ':')) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "NL" && ((ch(grid, 3) > '/' && ch(grid, 3) < '9' && ch(grid, 4) > '0' && ch(grid, 4) < '7' && slice(grid, 3, 4) != "83" && slice(grid, 3, 4) != "84" && slice(grid, 3, 4) != "85") || slice(grid, 3, 4) == "77" || slice(grid, 3, 4) == "87")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "ON" && ((ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (ch(grid, 3) > '4' && ch(grid, 3) < '7' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "79")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PN" && ((ch(grid, 3) == '7' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (ch(grid, 3) == '5' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "40" || slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "59" || slice(grid, 3, 4) == "69" || slice(grid, 3, 4) == "79")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "MM" && ((ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (ch(grid, 3) == '7' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (ch(grid, 3) == '8' && ch(grid, 4) > '/' && ch(grid, 4) < '5'))) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OL" && ((ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) == '0') || slice(grid, 3, 4) == "11" || slice(grid, 3, 4) == "21")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 2) > '/' && ch(callsign, 2) < ':') {
      lwrongcall = true;
      return;
    }
  } else if (ch(callsign, 1) == 'Y') {
    if (ch(callsign, 2) > 'N' && ch(callsign, 2) < 'S') {
      if (slice(grid, 1, 2) == "KN" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '2' && ch(grid, 4) < '9') {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) == 'U' || ch(callsign, 2) == 'T') {
      if ((slice(grid, 1, 3) == "JN9" && ch(grid, 4) > '2' && ch(grid, 4) < '7') || (slice(grid, 1, 3) == "KN0" && ch(grid, 4) > '1' && ch(grid, 4) < '7') || (slice(grid, 1, 3) == "KN1" && ch(grid, 4) > '1' && ch(grid, 4) < '5')) {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'A' && ch(callsign, 2) < 'I') {
      if ((slice(grid, 1, 2) == "NJ" && ch(grid, 3) > '5' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "NI" && ch(grid, 3) > '8' && ch(grid, 3) < ':' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "OI" || slice(grid, 1, 2) == "PI" || (slice(grid, 1, 2) == "OJ" && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "PJ" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "QI" && ch(grid, 3) > '/' && ch(grid, 3) < '1' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "PH" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) == '9')) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "PJ" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "44")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "NJ" && ((ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (ch(grid, 3) == '7' && ((ch(grid, 4) > '5' && ch(grid, 4) < ':') || ch(grid, 4) == '0' || ch(grid, 4) == '1' || ch(grid, 4) == '3')) || (ch(grid, 3) == '8' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "95")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OJ" && ((ch(grid, 3) > '4' && ch(grid, 3) < '7' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || (ch(grid, 3) == '1' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "04")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OI" && ((ch(grid, 4) == '1' && ch(grid, 3) > '1' && ch(grid, 3) < '5') || (ch(grid, 4) == '5' && ch(grid, 3) > '2' && ch(grid, 3) < '6') || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "39")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PI" && (slice(grid, 3, 4) == "61" || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "71" || slice(grid, 3, 4) == "83" || slice(grid, 3, 4) == "25" || slice(grid, 3, 4) == "35" || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "29")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 2) > 'U' && ch(callsign, 2) < 'Z') {
      if ((slice(grid, 1, 2) == "FJ" && ((ch(grid, 3) > '2' && ch(grid, 3) < ':' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "60" || slice(grid, 3, 4) == "70")) || (slice(grid, 1, 2) == "FK" && ch(grid, 3) > '2' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || slice(grid, 1, 4) == "FK85") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "FJ" && ((ch(grid, 3) > '2' && ch(grid, 3) < '6' && ch(grid, 4) > '0' && ch(grid, 4) < '7' && slice(grid, 3, 4) != "57") || slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "93")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FJ" && (slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "72" || slice(grid, 3, 4) == "82" || slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "91")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "YL") {
      if ((slice(grid, 1, 2) == "KO" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '5' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "KO" && (slice(grid, 3, 4) == "25" || slice(grid, 3, 4) == "35" || slice(grid, 3, 4) == "46" || slice(grid, 3, 4) == "28"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "YA") {
      if ((slice(grid, 1, 2) == "MM" && ch(grid, 3) > '/' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "ML" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && slice(grid, 3, 4) == "9")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "YI") {
      if ((slice(grid, 1, 2) == "LM" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "KM" && (slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "93")) || (slice(grid, 1, 2) == "LL" && ch(grid, 3) > '0' && ch(grid, 3) < '5' && ch(grid, 4) == '9')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "YJ") {
      if ((slice(grid, 1, 2) == "RH" && ch(grid, 3) > '2' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || slice(grid, 1, 4) == "RG49" || slice(grid, 1, 4) == "RH50" || slice(grid, 1, 4) == "RH36") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "YK") {
      if ((slice(grid, 1, 2) == "KM" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < '7') || (slice(grid, 1, 2) == "LM" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '3' && ch(grid, 4) < '8')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "YM") {
      if ((slice(grid, 1, 2) == "KM" && ((ch(grid, 3) > '2' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "85")) || (slice(grid, 1, 2) == "KN" && ((ch(grid, 3) > '2' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "72")) || (slice(grid, 1, 2) == "LN" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "LM" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '5' && ch(grid, 4) < ':' && slice(grid, 3, 4) != "16")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "YN") {
      if (slice(grid, 1, 2) == "EK" && ch(grid, 3) > '5' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '5') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "YS") {
      if (slice(grid, 1, 2) == "EK" && (slice(grid, 3, 4) == "53" || slice(grid, 3, 4) == "54" || slice(grid, 3, 4) == "63" || slice(grid, 3, 4) == "43")) {
        lgvalid = true;
      }
    } else if (ch(callsign, 1) == 'Y' && ch(callsign, 2) > '1' && ch(callsign, 2) < ':') {
      if ((slice(grid, 1, 2) == "JO" && ((ch(grid, 3) > '1' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "45")) || (slice(grid, 1, 2) == "JN" && ch(grid, 3) > '2' && ch(grid, 3) < '7' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "IB59") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "JO" && ((ch(grid, 3) < '2' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "35")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "Y1" || slice(callsign, 1, 2) == "Y0") {
      lwrongcall = true;
      return;
    }
  } else if (ch(callsign, 1) == 'Z') {
    if (slice(callsign, 1, 2) == "Z3") {
      if ((slice(grid, 1, 3) == "KN0" && ch(grid, 4) > '/' && ch(grid, 4) < '3') || slice(grid, 1, 4) == "KN11" || slice(grid, 1, 4) == "KN11") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "Z6" && (slice(grid, 1, 4) == "KN02" || slice(grid, 1, 4) == "KN03")) {
      lgvalid = true;
    } else if (ch(callsign, 2) > 'J' && ch(callsign, 2) < 'N') {
      if ((slice(grid, 1, 2) == "RF" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (slice(grid, 1, 2) == "RE" && ch(grid, 3) > '2' && ch(grid, 3) < '9' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "AE16" || slice(grid, 1, 4) == "AE15" || slice(grid, 1, 4) == "RE31" || slice(grid, 1, 4) == "RE90" || slice(grid, 1, 4) == "RE92" || slice(grid, 1, 4) == "AF10" || slice(grid, 1, 4) == "AG09" || slice(grid, 1, 4) == "AG08" || slice(grid, 1, 4) == "RD29" || slice(grid, 1, 4) == "RD39" || slice(grid, 1, 4) == "RD47") {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'Q' && ch(callsign, 2) < 'V') {
      if ((slice(grid, 1, 2) == "KG" && ((ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (ch(grid, 3) == '6' && ch(grid, 4) > '0' && ch(grid, 4) < '4'))) || (slice(grid, 1, 2) == "KF" && ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JF" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '4' && ch(grid, 4) < ':' && slice(grid, 3, 4) != "85") || (slice(grid, 1, 2) == "JG" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 1, 4) == "KE83" || slice(grid, 1, 4) == "KE93") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "KF" && ((ch(grid, 4) == '5' && ch(grid, 3) > '2' && ch(grid, 3) < '6') || (ch(grid, 4) == '6' && ch(grid, 3) > '3' && ch(grid, 3) < '6') || slice(grid, 3, 4) == "57")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "ZP") {
      if ((slice(grid, 1, 2) == "GG" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FG" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "FH90" || slice(grid, 1, 4) == "GH00" || slice(grid, 1, 4) == "FG95") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "ZA") {
      if ((slice(grid, 1, 3) == "JN9" && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 3) == "KN0" && ch(grid, 4) > '/' && ch(grid, 4) < '3') || slice(grid, 1, 4) == "JM99" || slice(grid, 1, 4) == "KM09") {
        lgvalid = true;
      }
    } else if ((slice(callsign, 1, 2) == "ZB" || slice(callsign, 1, 2) == "ZG") && slice(grid, 1, 4) == "IM76") {
      lgvalid = true;
    } else if (slice(callsign, 1, 3) == "ZC4" && slice(grid, 1, 4) == "KM64") {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "Z2") {
      if ((slice(grid, 1, 2) == "KH" && ch(grid, 3) > '1' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "KG" && ch(grid, 3) > '2' && ch(grid, 3) < '7' && ch(grid, 4) > '6' && ch(grid, 4) < ':')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "Z8") {
      if ((slice(grid, 1, 2) == "KJ" && ch(grid, 3) > '1' && ch(grid, 3) < '8' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KK" && ch(grid, 3) > '1' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '3')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 3) == "ZD7" && (slice(grid, 1, 4) == "IH74" || slice(grid, 1, 4) == "IH73")) {
      lgvalid = true;
    } else if (slice(callsign, 1, 3) == "ZD8" && slice(grid, 1, 4) == "II22") {
      lgvalid = true;
    } else if (slice(callsign, 1, 3) == "ZD9" && (slice(grid, 1, 4) == "IF32" || slice(grid, 1, 4) == "IE49" || slice(grid, 1, 4) == "IE59")) {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "ZF" && (slice(grid, 1, 4) == "EK99" || slice(grid, 1, 4) == "FK09")) {
      lgvalid = true;
    } else if (slice(callsign, 1, 3) == "ZK3" && (slice(grid, 1, 4) == "AI31" || slice(grid, 1, 4) == "AI40")) {
      lgvalid = true;
    } else if (ch(callsign, 2) > 'U' && ch(callsign, 2) < '[') {
      if ((slice(grid, 1, 2) == "GG" && ((ch(grid, 3) > '0' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "09")) || (slice(grid, 1, 2) == "HI" && ((ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '6' && slice(grid, 3, 4) != "25") || slice(grid, 3, 4) == "06" || slice(grid, 3, 4) == "07" || slice(grid, 3, 4) == "36")) || (slice(grid, 1, 2) == "HH" && ch(grid, 3) > '/' && ch(grid, 3) < '2') || (slice(grid, 1, 2) == "GH" && slice(grid, 3, 4) != "01") || slice(grid, 1, 2) == "GI" || (slice(grid, 1, 2) == "GF" && ((ch(grid, 3) > '0' && ch(grid, 3) < '5' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "36")) || (slice(grid, 1, 2) == "GJ" && ((ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "04" || slice(grid, 3, 4) == "44")) || (slice(grid, 1, 2) == "FJ" && ((ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "95")) || (slice(grid, 1, 2) == "FI" && ch(grid, 3) > '2' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "FH" && ((ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "93" || slice(grid, 3, 4) == "94" || slice(grid, 3, 4) == "95")) || slice(grid, 1, 4) == "HG59" || slice(grid, 1, 4) == "HJ50") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "GG" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '6' && slice(grid, 3, 4) != "64" && slice(grid, 3, 4) != "65") || (ch(grid, 3) == '1' && ch(grid, 4) > '1' && ch(grid, 4) < '7'))) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "HH" && (ch(grid, 3) == '1' && ch(grid, 4) > '/' && ch(grid, 4) < '7')) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "GI" && ((ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "79")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "GF" && (slice(grid, 3, 4) == "17" || slice(grid, 3, 4) == "27")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "GJ" && (slice(grid, 3, 4) == "13" || slice(grid, 3, 4) == "23" || slice(grid, 3, 4) == "52" || slice(grid, 3, 4) == "53")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FJ" && ch(grid, 3) > '4' && ch(grid, 3) < '7' && ch(grid, 4) > '1' && ch(grid, 4) < '5') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FI" && ch(grid, 3) > '2' && ch(grid, 3) < '5' && ch(grid, 4) > '5' && ch(grid, 4) < ':') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FH" && ((ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '5' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "68")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 2) == '1' || ch(callsign, 2) == '7' || ch(callsign, 2) == '9') {
      lwrongcall = true;
      return;
    }
  }
label_4:
  if (ch(callsign, 1) == '1') {
    if (slice(callsign, 1, 2) == "1A" && slice(grid, 1, 4) == "JN61") {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "1B") {
      if ((slice(grid, 1, 3) == "KM6" || slice(grid, 1, 3) == "KM7") && ch(grid, 4) > '3' && ch(grid, 4) < '6') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "1S") {
      if ((slice(grid, 1, 2) == "OK" && ch(grid, 3) > '5' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "OJ" && ch(grid, 3) > '3' && ch(grid, 3) < '9' && ch(grid, 4) > '6' && ch(grid, 4) < ':')) {
        lgvalid = true;
      }
    } else if ((ch(callsign, 2) > 'B' && ch(callsign, 2) < 'S') || (ch(callsign, 2) > 'S' && ch(callsign, 2) < '[')) {
      lwrongcall = true;
      return;
    }
  } else if (ch(callsign, 1) == '2') {
    if (ch(callsign, 2) == 'E' || ch(callsign, 2) == 'A' || ch(callsign, 2) == 'M' || ch(callsign, 2) == 'W' || ch(callsign, 2) == 'I' || ch(callsign, 2) == 'J' || ch(callsign, 2) == 'U' || ch(callsign, 2) == 'D') {
      if ((slice(grid, 1, 2) == "IO" && ch(grid, 3) > '4' && ch(grid, 3) < ':') || (slice(grid, 1, 3) == "JO0" && ch(grid, 4) > '/' && ch(grid, 4) < '4') || slice(grid, 1, 4) == "IP90" || slice(grid, 1, 4) == "IN69" || slice(grid, 1, 4) == "IN89" || slice(grid, 1, 4) == "IO37") {
        lgvalid = true;
      }
      if (lgvalid && slice(grid, 1, 2) == "IO") {
        if ((slice(grid, 1, 3) == "IO5" && ch(grid, 4) != '4' && ch(grid, 4) != '7') || (slice(grid, 1, 3) == "IO6" && ch(grid, 4) > '/' && ch(grid, 4) < '4') || slice(grid, 3, 4) == "96" || slice(grid, 3, 4) == "98") {
          lgvalid = false;
        }
      }
    } else {
      lwrongcall = true;
      return;
    }
  } else if (ch(callsign, 1) == '3') {
    if (slice(callsign, 1, 2) == "3Z") {
      if ((slice(grid, 1, 2) == "JO" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "KO" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "20")) || slice(grid, 1, 4) == "KN09" || slice(grid, 1, 4) == "KN19" || slice(grid, 1, 4) == "KO20" || slice(grid, 1, 4) == "JN99" || slice(grid, 1, 4) == "JN89") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "3A" && slice(grid, 1, 4) == "JN33") {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "3G") {
      if ((slice(grid, 1, 2) == "FF" && ch(grid, 3) > '2' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "FG" && ((ch(grid, 3) > '3' && ch(grid, 3) < '6') || (ch(grid, 3) == '6' && ch(grid, 4) > '4' && ch(grid, 4) < '8'))) || (slice(grid, 1, 2) == "FE" && ch(grid, 3) > '1' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "FH" && ((ch(grid, 3) > '3' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "52")) || (slice(grid, 1, 2) == "FD" && ((ch(grid, 3) > '1' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "53" || slice(grid, 3, 4) == "85")) || slice(grid, 1, 4) == "EF96" || slice(grid, 1, 4) == "FF06" || slice(grid, 1, 4) == "EG93" || slice(grid, 1, 4) == "FG03" || slice(grid, 1, 4) == "DG52" || slice(grid, 1, 4) == "DG73") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "FF" && ch(grid, 3) == '3' && ch(grid, 4) > '5' && ch(grid, 4) < ':') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FE" && (slice(grid, 3, 4) == "40" || slice(grid, 3, 4) == "41" || slice(grid, 3, 4) == "29")) {
          lgvalid = false;
        }
      }
    } else if ((slice(callsign, 1, 3) == "3B6" || slice(callsign, 1, 3) == "3B7") && (slice(grid, 1, 4) == "LH89" || slice(grid, 1, 4) == "LH93")) {
      lgvalid = true;
    } else if (slice(callsign, 1, 3) == "3B8" && (slice(grid, 1, 4) == "LG89" || slice(grid, 1, 4) == "LH80")) {
      lgvalid = true;
    } else if (slice(callsign, 1, 3) == "3B9" && slice(grid, 1, 4) == "MH10") {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "3C") {
      if ((slice(grid, 1, 3) == "JJ4" && ch(grid, 4) > '/' && ch(grid, 4) < '3') || slice(grid, 1, 4) == "JJ51" || slice(grid, 1, 4) == "JJ52" || slice(grid, 1, 4) == "JI28") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 3) == "3D2") {
      if ((slice(grid, 1, 2) == "RH" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (slice(grid, 1, 3) == "AH0" && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 1, 4) == "RG78" || slice(grid, 1, 4) == "AG08" || slice(grid, 1, 4) == "AG09") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 3) == "3DA") {
      if ((slice(grid, 1, 3) == "KG5" && ch(grid, 4) > '1' && ch(grid, 4) < '5') || slice(grid, 1, 4) == "KG63" || slice(grid, 1, 4) == "KG64") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "3V") {
      if (slice(grid, 1, 2) == "JM" && ch(grid, 3) > '2' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '8') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "3W") {
      if ((slice(grid, 1, 2) == "OL" && ch(grid, 3) > '0' && ch(grid, 3) < '4' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "OK" && ch(grid, 3) > '0' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "OJ" && ch(grid, 3) > '1' && ch(grid, 3) < '4' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "OJ19" || slice(grid, 1, 4) == "OL41" || slice(grid, 1, 4) == "OL23") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "3X") {
      if ((slice(grid, 1, 2) == "IJ" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "IK" && ch(grid, 3) > '1' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || slice(grid, 1, 4) == "IJ39") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 3) == "3Y" && (slice(grid, 1, 4) == "JD15" || slice(grid, 1, 4) == "EC41")) {
      lgvalid = true;
    } else if (ch(callsign, 2) == 'E' || ch(callsign, 2) == 'F') {
      if ((slice(grid, 1, 2) == "FJ" && (slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "09" || slice(grid, 3, 4) == "17" || slice(grid, 3, 4) == "18")) || (slice(grid, 1, 2) == "EJ" && (slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "89" || slice(grid, 3, 4) == "97" || slice(grid, 3, 4) == "98" || slice(grid, 3, 4) == "99"))) {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'G' && ch(callsign, 2) < 'V') {
      if (len_trim(callsign) > 5) {
        return;
      }
      if (slice(grid, 1, 2) == "OL" || (slice(grid, 1, 2) == "PL" && ch(grid, 3) > '/' && ch(grid, 3) < '2') || slice(grid, 1, 2) == "OM" || (slice(grid, 1, 2) == "PM" && ch(grid, 3) > '/' && ch(grid, 3) < '3') || (slice(grid, 1, 2) == "PN" && ch(grid, 3) > '/' && ch(grid, 3) < '8') || (slice(grid, 1, 2) == "PO" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (slice(grid, 1, 2) == "OK" && (slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "58" || slice(grid, 3, 4) == "59")) || slice(grid, 1, 2) == "ON" || slice(grid, 1, 4) == "OO90" || slice(grid, 1, 4) == "OO91" || slice(grid, 1, 2) == "NN" || slice(grid, 1, 2) == "NM" || (slice(grid, 1, 2) == "NL" && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "MN" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "MM" && ch(grid, 3) > '5' && ch(grid, 3) < ':')) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "NL" && ((ch(grid, 3) > '/' && ch(grid, 3) < '9' && ch(grid, 4) > '0' && ch(grid, 4) < '7' && slice(grid, 3, 4) != "83" && slice(grid, 3, 4) != "84" && slice(grid, 3, 4) != "85") || slice(grid, 3, 4) == "77" || slice(grid, 3, 4) == "87")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "ON" && ((ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (ch(grid, 3) > '4' && ch(grid, 3) < '7' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "79")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PN" && ((ch(grid, 3) == '7' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (ch(grid, 3) == '5' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "40" || slice(grid, 3, 4) == "50" || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "59" || slice(grid, 3, 4) == "69" || slice(grid, 3, 4) == "79")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "MM" && ((ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (ch(grid, 3) == '7' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (ch(grid, 3) == '8' && ch(grid, 4) > '/' && ch(grid, 4) < '5'))) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OL" && ((ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) == '0') || slice(grid, 3, 4) == "11" || slice(grid, 3, 4) == "21")) {
          lgvalid = false;
        }
      }
    }
  } else if (ch(callsign, 1) == '4') {
    if (slice(callsign, 1, 2) == "4M") {
      if ((slice(grid, 1, 2) == "FJ" && ((ch(grid, 3) > '2' && ch(grid, 3) < ':' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "60" || slice(grid, 3, 4) == "70")) || (slice(grid, 1, 2) == "FK" && ch(grid, 3) > '2' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || slice(grid, 1, 4) == "FK85") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "FJ" && ((ch(grid, 3) > '2' && ch(grid, 3) < '6' && ch(grid, 4) > '0' && ch(grid, 4) < '7' && slice(grid, 3, 4) != "57") || slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "93")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "FJ" && (slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "72" || slice(grid, 3, 4) == "82" || slice(grid, 3, 4) == "92" || slice(grid, 3, 4) == "91")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "4X" || slice(callsign, 1, 2) == "4Z") {
      if ((slice(grid, 1, 3) == "KM7" && ch(grid, 4) > '/' && ch(grid, 4) < '4') || slice(grid, 1, 4) == "KL79") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "4O") {
      if ((slice(grid, 1, 3) == "JN9" && ch(grid, 4) > '0' && ch(grid, 4) < '4') || slice(grid, 1, 4) == "KN02" || slice(grid, 1, 4) == "KN03") {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'O' && ch(callsign, 2) < 'T') {
      if ((slice(grid, 1, 3) == "MJ9" && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 3) == "NJ0" && ch(grid, 4) > '4' && ch(grid, 4) < ':')) {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'C' && ch(callsign, 2) < 'J') {
      if ((slice(grid, 1, 2) == "PK" && ch(grid, 3) > '/' && ch(grid, 3) < '4') || (slice(grid, 1, 2) == "PJ" && ((ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "04")) || (slice(grid, 1, 2) == "OJ" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "OK" && ch(grid, 3) == '9' && ch(grid, 4) > '/' && ch(grid, 4) < '7') || (slice(grid, 1, 2) == "PL" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "10")) || slice(grid, 1, 4) == "OK70" || slice(grid, 1, 4) == "OK71") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "PK" && ((ch(grid, 3) == '3' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (ch(grid, 3) == '2' && ch(grid, 4) > '4' && ch(grid, 4) < ':'))) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PJ" && (slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "15")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OJ" && (slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "85")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "4J" || slice(callsign, 1, 2) == "4J") {
      if ((slice(grid, 1, 2) == "LN" && ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "LM" && ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "LN50") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "4J" || slice(callsign, 1, 2) == "4J") {
      if (slice(grid, 1, 2) == "LN" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '/' && ch(grid, 4) < '4') {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) == 'U' && (slice(callsign, 1, 6) == "4U1ITU" || slice(callsign, 1, 5) == "4U1UN" || slice(callsign, 1, 6) == "4U1VIC" || slice(grid, 1, 4) == "JN88")) {
      lgvalid = true;
    } else if (ch(callsign, 2) > '@' && ch(callsign, 2) < 'D') {
      if ((slice(grid, 1, 2) == "DL" && ((ch(grid, 3) > '1' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "09")) || (slice(grid, 1, 2) == "EL" && ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "EK" && ((ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "34")) || (slice(grid, 1, 2) == "DM" && ch(grid, 3) > '0' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "DK" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "DK" && (slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "28" || slice(grid, 3, 4) == "38"))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "DL" && ((ch(grid, 3) > '1' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "23" || slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "33" || slice(grid, 3, 4) == "60")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "EL" && ((ch(grid, 3) > '1' && ch(grid, 3) < '7' && ch(grid, 4) > '2' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "17" || slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "62")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "EK" && (slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "65" || slice(grid, 3, 4) == "66" || slice(grid, 3, 4) == "67" || slice(grid, 3, 4) == "39")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "DM" && ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) == '2') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "DK" && (slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "77" || slice(grid, 3, 4) == "86")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "4V") {
      if ((slice(grid, 1, 2) == "FK" && (slice(grid, 3, 4) == "38" || slice(grid, 3, 4) == "39" || slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "49")) || slice(grid, 1, 4) == "FL30") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "4W") {
      if (slice(grid, 1, 2) == "PI" && (slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "31")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "4T") {
      if ((slice(grid, 1, 2) == "FH" && ch(grid, 3) > '0' && ch(grid, 3) < '6' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "FI" && ch(grid, 3) > '/' && ch(grid, 3) < '5') || (slice(grid, 1, 3) == "EI9" && ch(grid, 4) > '2' && ch(grid, 4) < '7')) {
        lgvalid = true;
      }
    }
  } else if (ch(callsign, 1) == '5') {
    if (slice(callsign, 1, 2) == "5P" || slice(callsign, 1, 2) == "5Q") {
      if (slice(grid, 1, 2) == "JO" && ((ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "75" || slice(grid, 3, 4) == "76")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5B") {
      if ((slice(grid, 1, 3) == "KM6" && (ch(grid, 4) == '4' || ch(grid, 4) == '5')) || (slice(grid, 1, 3) == "KM7" && (ch(grid, 4) == '4' || ch(grid, 4) == '5'))) {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'B' && ch(callsign, 2) < 'H') {
      if ((slice(grid, 1, 2) == "IM" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (slice(grid, 1, 2) == "IL" && ((ch(grid, 3) > '0' && ch(grid, 3) < '6' && ch(grid, 4) > '0' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "69" || slice(grid, 3, 4) == "79"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5A") {
      if ((slice(grid, 1, 2) == "JM" && ch(grid, 3) > '4' && ch(grid, 3) < '/' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "KM" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "JL" && ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KL" && ch(grid, 3) > '/' && ch(grid, 3) < '3') || (slice(grid, 1, 2) == "JM" && (slice(grid, 3, 4) == "40" || slice(grid, 3, 4) == "53")) || slice(grid, 1, 4) == "JL91" || slice(grid, 1, 4) == "KK19") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5H" || slice(callsign, 1, 2) == "5I") {
      if ((slice(grid, 1, 2) == "KI" && ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "KH" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5K" || slice(callsign, 1, 2) == "5J") {
      if ((slice(grid, 1, 2) == "FJ" && ((ch(grid, 3) > '0' && ch(grid, 3) < '7') || slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "02" || slice(grid, 3, 4) == "03")) || (slice(grid, 1, 2) == "FK" && ch(grid, 3) > '1' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '3' && slice(grid, 3, 4) != "22") || (slice(grid, 1, 2) == "FI" && ((ch(grid, 3) > '1' && ch(grid, 3) < '6' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "45" || slice(grid, 3, 4) == "55" || slice(grid, 3, 4) == "46" || slice(grid, 3, 4) == "56") && slice(grid, 3, 4) != "27") || (slice(grid, 1, 2) == "EJ" && (slice(grid, 3, 4) == "93" || slice(grid, 3, 4) == "94")) || slice(grid, 1, 4) == "EK92") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5L" || slice(callsign, 1, 2) == "5M") {
      if (slice(grid, 1, 2) == "IJ" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < '9' && slice(grid, 3, 4) != "67" && slice(grid, 3, 4) != "67") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5N" || slice(callsign, 1, 2) == "5O") {
      if ((slice(grid, 1, 2) == "JJ" && ch(grid, 3) > '0' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JK" && ch(grid, 3) > '0' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '4')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5R" || slice(callsign, 1, 2) == "5S") {
      if ((slice(grid, 1, 2) == "LH" && ch(grid, 3) > '1' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '7') || (slice(grid, 1, 2) == "LG" && ch(grid, 3) > '0' && ch(grid, 3) < '5' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "LH" && (slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "47" || slice(grid, 3, 4) == "48")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5T") {
      if ((slice(grid, 1, 2) == "IK" && ((ch(grid, 3) > '0' && ch(grid, 3) < '8' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "44")) || (slice(grid, 1, 2) == "IL" && ((ch(grid, 3) > '0' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || slice(grid, 3, 4) == "56" || slice(grid, 3, 4) == "66" || slice(grid, 3, 4) == "57" || slice(grid, 3, 4) == "75"))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "IK" && slice(grid, 3, 4) == "15" || slice(grid, 3, 4) == "25") {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "IL" && ((ch(grid, 3) > '0' && ch(grid, 3) < '3' && ch(grid, 4) > '1' && ch(grid, 4) < '6') || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "35")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "5U") {
      if ((slice(grid, 1, 2) == "JK" && ch(grid, 3) > '/' && ch(grid, 3) < '8' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JL" && ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || slice(grid, 1, 4) == "JK11" || slice(grid, 1, 2) == "JL" && (slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "53" || slice(grid, 3, 4) == "63")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5V") {
      if ((slice(grid, 1, 3) == "JJ0" && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JK" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01")) || (slice(grid, 1, 2) == "IK" && (slice(grid, 3, 4) == "90" || slice(grid, 3, 4) == "91"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5W") {
      if (slice(grid, 1, 2) == "AH" && (slice(grid, 3, 4) == "36" || slice(grid, 3, 4) == "46" || slice(grid, 3, 4) == "45")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5X") {
      if ((slice(grid, 1, 2) == "KJ" && ch(grid, 3) > '4' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (slice(grid, 1, 2) == "KI" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) == '9') || (slice(grid, 1, 2) == "KJ" && (slice(grid, 3, 4) == "40" || slice(grid, 3, 4) == "64" || slice(grid, 3, 4) == "74")) || (slice(grid, 1, 2) == "KI" && (slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "58"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "5Z" || slice(callsign, 1, 2) == "5Y") {
      if ((slice(grid, 1, 2) == "KI" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KJ" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "LI" && ch(grid, 3) == '0' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "LJ" && ch(grid, 3) == '0' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 1, 4) == "KJ60" || slice(grid, 1, 4) == "KI69") {
        lgvalid = true;
      }
    }
  } else if (ch(callsign, 1) == '6') {
    if (slice(callsign, 1, 2) == "6C") {
      if ((slice(grid, 1, 2) == "KM" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < '7') || (slice(grid, 1, 2) == "LM" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '3' && ch(grid, 4) < '8')) {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'C' && ch(callsign, 2) < 'K') {
      if ((slice(grid, 1, 2) == "DL" && ((ch(grid, 3) > '1' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "09")) || (slice(grid, 1, 2) == "EL" && ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "EK" && ((ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "34")) || (slice(grid, 1, 2) == "DM" && ch(grid, 3) > '0' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "DK" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "DK" && (slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "28" || slice(grid, 3, 4) == "38"))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "DL" && ((ch(grid, 3) > '1' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "23" || slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "33" || slice(grid, 3, 4) == "60")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "EL" && ((ch(grid, 3) > '1' && ch(grid, 3) < '7' && ch(grid, 4) > '2' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "17" || slice(grid, 3, 4) == "20" || slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "32" || slice(grid, 3, 4) == "62")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "EK" && (slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "65" || slice(grid, 3, 4) == "66" || slice(grid, 3, 4) == "67" || slice(grid, 3, 4) == "39")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "DM" && ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) == '2') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "DK" && (slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "77" || slice(grid, 3, 4) == "86")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 1) == '6' && ch(callsign, 2) > 'J' && ch(callsign, 2) < 'O') {
      if ((slice(grid, 1, 3) == "PM3" && ch(grid, 4) > '2' && ch(grid, 4) < '9') || (slice(grid, 1, 3) == "PM4" && ch(grid, 4) > '3' && ch(grid, 4) < '9') || slice(grid, 1, 4) == "PM24" || slice(grid, 1, 4) == "PM57" || slice(grid, 1, 4) == "GC07") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "6V" || slice(callsign, 1, 2) == "6W") {
      if (slice(grid, 1, 2) == "IK" && ((ch(grid, 3) > '0' && ch(grid, 3) < '4' && ch(grid, 4) > '1' && ch(grid, 4) < '7') || slice(grid, 3, 4) == "42" || slice(grid, 3, 4) == "43")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "6X") {
      if ((slice(grid, 1, 2) == "LH" && ch(grid, 3) > '1' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '7') || (slice(grid, 1, 2) == "LG" && ch(grid, 3) > '0' && ch(grid, 3) < '5' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 1, 2) == "LH" && (slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "47" || slice(grid, 3, 4) == "48")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "6Y") {
      if (slice(grid, 1, 2) == "FK" && (slice(grid, 3, 4) == "17" || slice(grid, 3, 4) == "18" || slice(grid, 3, 4) == "08" || slice(grid, 3, 4) == "06")) {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) == 'T' || ch(callsign, 2) == 'U') {
      if ((slice(grid, 1, 2) == "KK" && ch(grid, 3) > '0' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "KL" && ch(grid, 3) > '1' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 1, 4) == "KJ79") {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'O' && ch(callsign, 2) < 'T') {
      if ((slice(grid, 1, 2) == "MM" && ((ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) > '/' && ch(grid, 4) < '7') || slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "31" || slice(grid, 3, 4) == "84" || slice(grid, 3, 4) == "85")) || (slice(grid, 1, 2) == "ML" && ((ch(grid, 3) > '/' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "33"))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "MM" && ch(grid, 3) == '4' && ch(grid, 4) > '3' && ch(grid, 4) < '7') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "ML" && ((ch(grid, 3) == '6' && ch(grid, 4) > '3' && ch(grid, 4) < '8') || slice(grid, 3, 4) == "07")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "6Z") {
      if (slice(grid, 1, 2) == "IJ" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '3' && ch(grid, 4) < '9' && slice(grid, 3, 4) != "67" && slice(grid, 3, 4) != "67") {
        lgvalid = true;
      }
    }
  } else if (ch(callsign, 1) == '7') {
    if (ch(callsign, 1) == '7' && ch(callsign, 2) > '@' && ch(callsign, 2) < 'J') {
      if ((slice(grid, 1, 2) == "NJ" && ch(grid, 3) > '5' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "NI" && ch(grid, 3) > '8' && ch(grid, 3) < ':' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "OI" && !(ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '4') && !(ch(grid, 3) > '1' && ch(grid, 3) < '8' && ch(grid, 4) == '0')) || (slice(grid, 1, 2) == "PI" && !(ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) == '0')) || (slice(grid, 1, 2) == "OJ" && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "PJ" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "QI" && ch(grid, 3) > '/' && ch(grid, 3) < '1' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "PH" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) == '9')) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "PJ" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "44")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "NJ" && ((ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (ch(grid, 3) == '7' && ((ch(grid, 4) > '5' && ch(grid, 4) < ':') || ch(grid, 4) == '0' || ch(grid, 4) == '1' || ch(grid, 4) == '3')) || (ch(grid, 3) == '8' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "95")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OJ" && ((ch(grid, 3) > '4' && ch(grid, 3) < '7' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || (ch(grid, 3) == '1' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "04")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OI" && ((ch(grid, 4) == '1' && ch(grid, 3) > '1' && ch(grid, 3) < '5') || (ch(grid, 4) == '5' && ch(grid, 3) > '2' && ch(grid, 3) < '6') || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "39")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PI" && (slice(grid, 3, 4) == "61" || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "71" || slice(grid, 3, 4) == "83" || slice(grid, 3, 4) == "25" || slice(grid, 3, 4) == "35" || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "29")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 1) == '7' && ch(callsign, 2) > 'I' && ch(callsign, 2) < 'O') {
      if ((slice(grid, 1, 2) == "PM" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || (slice(grid, 1, 3) == "QM0" && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "QN" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (slice(grid, 1, 3) == "PN9" && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "PL" && ch(grid, 3) > '0' && ch(grid, 3) < '6' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "QM19" || slice(grid, 1, 4) == "KC90") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "PM" && ((ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (ch(grid, 3) > '5' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '3' && slice(grid, 3, 4) != "62") || slice(grid, 3, 4) == "45" || slice(grid, 3, 4) == "46" || slice(grid, 3, 4) == "55" || slice(grid, 3, 4) == "56" || slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "89")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PL" && ((ch(grid, 3) > '0' && ch(grid, 3) < '3' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "38" || slice(grid, 3, 4) == "39" || slice(grid, 3, 4) == "57" || slice(grid, 3, 4) == "59")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "QN" && ((ch(grid, 3) == '2' && (ch(grid, 4) == '0' || ch(grid, 4) == '1' || ch(grid, 4) == '5')) || slice(grid, 3, 4) == "10")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "7O") {
      if (slice(grid, 1, 2) == "LK" && ((ch(grid, 3) > '0' && ch(grid, 3) < '7' && ch(grid, 4) > '2' && ch(grid, 4) < '9') || slice(grid, 3, 4) == "05" || slice(grid, 3, 4) == "06" || slice(grid, 3, 4) == "12" || slice(grid, 3, 4) == "22")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "7P") {
      if ((slice(grid, 1, 2) == "KG" && ch(grid, 3) > '2' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "KF" && (slice(grid, 3, 4) == "39" || slice(grid, 3, 4) == "49"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "7Q") {
      if ((slice(grid, 1, 2) == "KH" && ch(grid, 3) > '5' && ch(grid, 3) < '8' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "KI60" || (slice(grid, 1, 3) == "KH7" && ch(grid, 4) > '1' && ch(grid, 4) < '5')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "7S") {
      if ((slice(grid, 1, 2) == "JO" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JP" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "KP" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "25"))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "JO" && (slice(grid, 3, 4) == "85" || slice(grid, 3, 4) == "95")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "JP" && ((ch(grid, 3) == '6' && ch(grid, 4) > '4' && ch(grid, 4) < '9') || (ch(grid, 3) == '7' && ch(grid, 4) > '6' && ch(grid, 4) < '9') || slice(grid, 3, 4) == "89" || slice(grid, 3, 4) == "99")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "KP" && (slice(grid, 3, 4) == "13" || slice(grid, 3, 4) == "14")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "7R" || ch(callsign, 2) > 'S' && ch(callsign, 2) < 'Z') {
      if ((slice(grid, 1, 2) == "JM" && ((ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '7') || slice(grid, 3, 4) == "37") && slice(grid, 3, 4) != "43") || (slice(grid, 1, 2) == "IM" && ((ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || slice(grid, 3, 4) == "70")) || (slice(grid, 1, 2) == "JL" && ((ch(grid, 3) > '/' && ch(grid, 3) < '5') || ch(grid, 3) == '5' && ch(grid, 3) < '1' && ch(grid, 4) == '5')) || (slice(grid, 1, 2) == "IL" && ((ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "92")) || (slice(grid, 1, 2) == "JK" && ch(grid, 3) > '0' && ch(grid, 3) < '4' && ch(grid, 4) == '9')) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "IL" && ((ch(grid, 3) == '5' && ch(grid, 3) < '2' && ch(grid, 4) == '6') || slice(grid, 3, 4) == "63" || slice(grid, 3, 4) == "64" || slice(grid, 3, 4) == "73")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "7Z") {
      if ((slice(grid, 1, 2) == "LL" && ch(grid, 3) > '/' && ch(grid, 3) < '8') || (slice(grid, 1, 2) == "KL" && ch(grid, 3) > '6' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "LK" && ch(grid, 3) > '/' && ch(grid, 3) < '8' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "LM" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "10")) || (slice(grid, 1, 2) == "KM" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 1, 4) == "KM92") {
        lgvalid = true;
      }
    }
  } else if (ch(callsign, 1) == '8') {
    if (ch(callsign, 1) == '8' && ch(callsign, 2) > '@' && ch(callsign, 2) < 'J') {
      if ((slice(grid, 1, 2) == "NJ" && ch(grid, 3) > '5' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "NI" && ch(grid, 3) > '8' && ch(grid, 3) < ':' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "OI" && !(ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '4') && !(ch(grid, 3) > '1' && ch(grid, 3) < '8' && ch(grid, 4) == '0')) || (slice(grid, 1, 2) == "PI" && !(ch(grid, 3) > '3' && ch(grid, 3) < ':' && ch(grid, 4) == '0')) || (slice(grid, 1, 2) == "OJ" && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "PJ" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "QI" && ch(grid, 3) > '/' && ch(grid, 3) < '1' && ch(grid, 4) > '/' && ch(grid, 4) < '8') || (slice(grid, 1, 2) == "PH" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) == '9')) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "PJ" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "44")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "NJ" && ((ch(grid, 3) == '6' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (ch(grid, 3) == '7' && ((ch(grid, 4) > '5' && ch(grid, 4) < ':') || ch(grid, 4) == '0' || ch(grid, 4) == '1' || ch(grid, 4) == '3')) || (ch(grid, 3) == '8' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "43" || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "95")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OJ" && ((ch(grid, 3) > '4' && ch(grid, 3) < '7' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || (ch(grid, 3) == '1' && ch(grid, 4) > '1' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "24" || slice(grid, 3, 4) == "04")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "OI" && ((ch(grid, 4) == '1' && ch(grid, 3) > '1' && ch(grid, 3) < '5') || (ch(grid, 4) == '5' && ch(grid, 3) > '2' && ch(grid, 3) < '6') || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "39")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PI" && (slice(grid, 3, 4) == "61" || slice(grid, 3, 4) == "62" || slice(grid, 3, 4) == "71" || slice(grid, 3, 4) == "83" || slice(grid, 3, 4) == "25" || slice(grid, 3, 4) == "35" || slice(grid, 3, 4) == "34" || slice(grid, 3, 4) == "44" || slice(grid, 3, 4) == "29")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 1) == '8' && ch(callsign, 2) > 'I' && ch(callsign, 2) < 'O') {
      if ((slice(grid, 1, 2) == "PM" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || (slice(grid, 1, 3) == "QM0" && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "QN" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '6') || (slice(grid, 1, 3) == "PN9" && ch(grid, 4) > '/' && ch(grid, 4) < '3') || (slice(grid, 1, 2) == "PL" && ch(grid, 3) > '0' && ch(grid, 3) < '6' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "QM19" || slice(grid, 1, 4) == "KC90") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "PM" && ((ch(grid, 3) > '3' && ch(grid, 3) < '8' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || (ch(grid, 3) > '5' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '3' && slice(grid, 3, 4) != "62") || slice(grid, 3, 4) == "45" || slice(grid, 3, 4) == "46" || slice(grid, 3, 4) == "55" || slice(grid, 3, 4) == "56" || slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "89")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "PL" && ((ch(grid, 3) > '0' && ch(grid, 3) < '3' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "38" || slice(grid, 3, 4) == "39" || slice(grid, 3, 4) == "57" || slice(grid, 3, 4) == "59")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "QN" && ((ch(grid, 3) == '2' && (ch(grid, 4) == '0' || ch(grid, 4) == '1' || ch(grid, 4) == '5')) || slice(grid, 3, 4) == "10")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "8O") {
      if ((slice(grid, 1, 2) == "KG" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KH" && ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "KH" && (slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "30")) || (slice(grid, 1, 2) == "KG" && (slice(grid, 3, 4) == "48" || slice(grid, 3, 4) == "47"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "8P" && slice(grid, 1, 4) == "GK03") {
      lgvalid = true;
    } else if (slice(callsign, 1, 2) == "8Q") {
      if ((slice(grid, 1, 3) == "MJ6" && ch(grid, 4) > '/' && ch(grid, 4) < '8') || slice(grid, 1, 4) == "MI69") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "8R") {
      if ((slice(grid, 1, 2) == "GJ" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '0' && ch(grid, 4) < '9') || (slice(grid, 1, 3) == "FJ9" && ch(grid, 4) > '3' && ch(grid, 4) < '8')) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "8S") {
      if ((slice(grid, 1, 2) == "JO" && ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "JP" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '9') || (slice(grid, 1, 2) == "KP" && ((ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "25"))) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "JO" && (slice(grid, 3, 4) == "85" || slice(grid, 3, 4) == "95")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "JP" && ((ch(grid, 3) == '6' && ch(grid, 4) > '4' && ch(grid, 4) < '9') || (ch(grid, 3) == '7' && ch(grid, 4) > '6' && ch(grid, 4) < '9') || slice(grid, 3, 4) == "89" || slice(grid, 3, 4) == "99")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "KP" && (slice(grid, 3, 4) == "13" || slice(grid, 3, 4) == "14")) {
          lgvalid = false;
        }
      }
    } else if (ch(callsign, 2) > 'S' && ch(callsign, 2) < 'Z') {
      if ((slice(grid, 1, 2) == "MJ" && (slice(grid, 3, 4) == "99" || slice(grid, 3, 4) == "89" || slice(grid, 3, 4) == "88" || slice(grid, 3, 4) == "68")) || (slice(grid, 1, 2) == "MK" && ((ch(grid, 3) > '5' && ch(grid, 3) < ':') || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "52")) || (slice(grid, 1, 2) == "NK" && ((ch(grid, 3) == '0' && ch(grid, 4) > '1' && ch(grid, 4) < '6') || (ch(grid, 3) > '/' && ch(grid, 3) < '3' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "39")) || (slice(grid, 1, 2) == "ML" && ch(grid, 3) > '3' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "NL" && ch(grid, 3) > '/' && ch(grid, 3) < '5') || (slice(grid, 1, 2) == "MM" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 1, 4) == "NM00") {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "MK" && ch(grid, 3) == '6' && ch(grid, 4) > '1' && ch(grid, 4) < '5') {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "NK" && (slice(grid, 3, 4) == "26" || slice(grid, 3, 4) == "27")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "ML" && ((ch(grid, 3) > '4' && (ch(grid, 4) == '0' || ch(grid, 4) == '5' || ch(grid, 4) == '8' || ch(grid, 4) == '9')) || slice(grid, 3, 4) == "58" || slice(grid, 3, 4) == "59")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "NL" && ((ch(grid, 4) == '9' && ch(grid, 3) > '/' && ch(grid, 4) < '5') || (ch(grid, 4) == '8' && ch(grid, 3) > '0' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "40")) {
          lgvalid = false;
        } else if (slice(grid, 1, 2) == "MM" && (slice(grid, 3, 4) == "61" || slice(grid, 3, 4) == "62")) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "8Z") {
      if ((slice(grid, 1, 2) == "LL" && ch(grid, 3) > '/' && ch(grid, 3) < '8') || (slice(grid, 1, 2) == "KL" && ch(grid, 3) > '6' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "LK" && ch(grid, 3) > '/' && ch(grid, 3) < '8' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "LM" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "01" || slice(grid, 3, 4) == "10")) || (slice(grid, 1, 2) == "KM" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 1, 4) == "KM92") {
        lgvalid = true;
      }
    }
  } else if (ch(callsign, 1) == '9') {
    if (slice(callsign, 1, 2) == "9A") {
      if (slice(grid, 1, 2) == "JN" && ((ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < '6') || slice(grid, 3, 4) == "64" || slice(grid, 3, 4) == "65" || slice(grid, 3, 4) == "76" || slice(grid, 3, 4) == "86") && slice(grid, 3, 4) != "93") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9H") {
      if (slice(grid, 1, 2) == "JM" && (slice(grid, 3, 4) == "75" || slice(grid, 3, 4) == "76")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9M" || slice(callsign, 1, 2) == "9W") {
      if (slice(grid, 1, 4) == "NJ96" || (slice(grid, 1, 2) == "OJ" && (((ch(grid, 3) > '/' && ch(grid, 3) < '2') || (ch(grid, 3) > '4' && ch(grid, 3) < ':')) && ch(grid, 4) > '0' && ch(grid, 4) < '7') || slice(grid, 3, 4) == "21" || slice(grid, 3, 4) == "22" || slice(grid, 3, 4) == "41" || slice(grid, 3, 4) == "87")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9K") {
      if ((slice(grid, 1, 2) == "LL" && (slice(grid, 3, 4) == "39" || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "38" || slice(grid, 3, 4) == "48")) || (slice(grid, 1, 2) == "LM" && (slice(grid, 3, 4) == "30" || slice(grid, 3, 4) == "40"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9V") {
      if (slice(grid, 1, 2) == "OJ" && (slice(grid, 3, 4) == "11" || slice(grid, 3, 4) == "21")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9Y" || slice(callsign, 1, 2) == "9Z") {
      if (slice(grid, 1, 2) == "FK" && (slice(grid, 3, 4) == "90" || slice(grid, 3, 4) == "91")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9N") {
      if ((slice(grid, 1, 2) == "NL" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "NM" && (slice(grid, 3, 4) == "00" || slice(grid, 3, 4) == "10"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9G") {
      if ((slice(grid, 1, 2) == "IJ" && ch(grid, 3) > '/' && ch(grid, 3) < '5' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || (slice(grid, 1, 3) == "JJ0" && ch(grid, 4) > '4' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "IK" && ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) == '0') || (slice(grid, 1, 4) == "JK00" || slice(grid, 1, 4) == "JJ84" || slice(grid, 1, 4) == "JJ94")) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9J" || slice(callsign, 1, 2) == "9I") {
      if ((slice(grid, 1, 2) == "KH" && ch(grid, 3) > '0' && ch(grid, 3) < '7' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KI" && ch(grid, 3) > '3' && ch(grid, 3) < '7' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || slice(grid, 1, 4) == "KH31") {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9L") {
      if ((slice(grid, 1, 2) == "IJ" && ch(grid, 3) > '2' && ch(grid, 3) < '5' && ch(grid, 4) > '6' && ch(grid, 4) < ':') || slice(grid, 1, 4) == "IJ46") {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'N' && ch(callsign, 2) < 'U') {
      if ((slice(grid, 1, 2) == "JI" && ((ch(grid, 3) > '5' && ch(grid, 3) < '8' && ch(grid, 4) > '3' && ch(grid, 4) < '6') || ch(grid, 3) > '7' && ch(grid, 3) < ':' && ch(grid, 4) > '1' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "81") || (slice(grid, 1, 2) == "KI" && ((ch(grid, 3) > '/' && ch(grid, 3) < '5') || slice(grid, 3, 4) == "51" || slice(grid, 3, 4) == "52")) || (slice(grid, 1, 2) == "KJ" && ((ch(grid, 3) > '/' && ch(grid, 3) < '6' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "25" || slice(grid, 3, 4) == "35")) || (slice(grid, 1, 2) == "JJ" && ((ch(grid, 3) > '8' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || slice(grid, 3, 4) == "80" || slice(grid, 3, 4) == "81" || slice(grid, 3, 4) == "95")) || (slice(grid, 1, 2) == "KH" && ((ch(grid, 3) > '0' && ch(grid, 3) < '5' && ch(grid, 4) > '7' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "37" || slice(grid, 3, 4) == "47" || slice(grid, 3, 4) == "46"))) {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9U") {
      if (slice(grid, 1, 2) == "KI" && ch(grid, 3) > '3' && ch(grid, 3) < '6' && ch(grid, 4) > '4' && ch(grid, 4) < '8') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 2) == "9X") {
      if (slice(grid, 1, 2) == "KI" && ch(grid, 3) > '3' && ch(grid, 3) < '6' && ch(grid, 4) > '6' && ch(grid, 4) < '9') {
        lgvalid = true;
      }
    } else if (slice(callsign, 1, 3) == "9M0") {
      if ((slice(grid, 1, 2) == "OK" && ch(grid, 3) > '5' && ch(grid, 3) < '9' && ch(grid, 4) > '/' && ch(grid, 4) < '2') || (slice(grid, 1, 2) == "OJ" && ch(grid, 3) > '3' && ch(grid, 3) < '9' && ch(grid, 4) > '6' && ch(grid, 4) < ':')) {
        lgvalid = true;
      }
    } else if (ch(callsign, 2) > 'A' && ch(callsign, 2) < 'E') {
      if ((slice(grid, 1, 2) == "LM" && ch(grid, 3) > '1' && ch(grid, 3) < ':') || (slice(grid, 1, 2) == "LL" && ((ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '5' && ch(grid, 4) < ':') || slice(grid, 3, 4) == "49" || slice(grid, 3, 4) == "75" || slice(grid, 3, 4) == "85" || slice(grid, 3, 4) == "95")) || (slice(grid, 1, 2) == "ML" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '4' && ch(grid, 4) < ':' && slice(grid, 3, 4) != "15" && slice(grid, 3, 4) != "19") || (slice(grid, 1, 2) == "MM" && ch(grid, 3) == '0' && ch(grid, 4) > '/' && ch(grid, 4) < '7')) {
        lgvalid = true;
      }
      if (lgvalid) {
        if (slice(grid, 1, 2) == "LM" && ((ch(grid, 3) == '2' && ch(grid, 4) > '/' && ch(grid, 4) < '4') || (ch(grid, 3) > '4' && ch(grid, 3) < ':' && ch(grid, 4) > '7' && ch(grid, 4) < ':' && slice(grid, 3, 4) != "78" && slice(grid, 3, 4) != "88"))) {
          lgvalid = false;
        }
      }
    } else if (slice(callsign, 1, 2) == "9E" || slice(callsign, 1, 2) == "9F") {
      if ((slice(grid, 1, 2) == "LJ" && ch(grid, 3) > '/' && ch(grid, 3) < '4' && ch(grid, 4) > '3' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "LK" && ch(grid, 3) > '/' && ch(grid, 3) < '2' && ch(grid, 4) > '/' && ch(grid, 4) < '5') || (slice(grid, 1, 2) == "KJ" && ch(grid, 3) > '5' && ch(grid, 3) < ':' && ch(grid, 4) > '2' && ch(grid, 4) < ':') || (slice(grid, 1, 2) == "KK" && ch(grid, 3) > '6' && ch(grid, 3) < ':' && ch(grid, 4) > '/' && ch(grid, 4) < '5')) {
        lgvalid = true;
      }
    }
  }
}
return;
}

} // namespace

extern "C" void ftx_ft8var_chkgrid_c (char const callsign[12], char const grid[12], int* lchkcall_out, int* lgvalid_out, int* lwrongcall_out)
{
  bool lchkcall = false;
  bool lgvalid = false;
  bool lwrongcall = false;
  ft8var_chkgrid_impl ({callsign, 12}, {grid, 12}, lchkcall, lgvalid, lwrongcall);
  if (lchkcall_out) *lchkcall_out = lchkcall ? 1 : 0;
  if (lgvalid_out) *lgvalid_out = lgvalid ? 1 : 0;
  if (lwrongcall_out) *lwrongcall_out = lwrongcall ? 1 : 0;
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
