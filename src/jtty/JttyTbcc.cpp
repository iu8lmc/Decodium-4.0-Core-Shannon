// Port di tbcc.f90 (codifica, CRC) e jtty_tbcc_list_decoder.f90 (percorso
// "optimized": sopravvissuti impacchettati in un intero a 64 bit, ordinati per
// metrica e poi per chiave, cosi' che a parita' di metrica l'ordine sia
// quello lessicale dei bit — lo stesso del Fortran).

#include "JttyTbcc.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

namespace decodium
{
namespace jtty
{

int const kSync13[kSyncSymbols] = {0, 2, 2, 3, 0, 0, 3, 2, 1, 3, 1, 2, 0};

namespace
{

// JTTY_TBCC_PROFILE_1167_1545_80F
constexpr int kMemoryNu = 9;
constexpr int kStateCount = 512;
constexpr int kStateMask = 0x1FF;
constexpr int kRegisterMask = 0x3FF;
constexpr int kGenerator0 = 0x277;    // octal 1167
constexpr int kGenerator1 = 0x365;    // octal 1545
constexpr int kOuterPolynomial = 0x80F;
constexpr int kOuterTopBit = 0x800;
constexpr int kOuterRegisterMask = 0xFFF;
constexpr int kPayloadBits = 34;
constexpr int kReservedBit = 33;

constexpr int kPackedOriginBits = 11;
constexpr int kPackedValidBit = kInformationBits + kPackedOriginBits;   // 57
constexpr int kPerStateWidth = 4;
constexpr int kWraps = 2;
constexpr int kHypotheses = 4;

constexpr double kNegativeMetric = -std::numeric_limits<double>::max ();

int parity (int v)
{
  return __builtin_popcount (static_cast<unsigned> (v)) & 1;
}

void transition (int state, int input_bit, int& next_state, int& tone)
{
  int const output_register = ((state << 1) | input_bit) & kRegisterMask;
  int const b0 = parity (output_register & kGenerator0);
  int const b1 = parity (output_register & kGenerator1);
  tone = 2 * b0 + (b0 ^ b1);
  next_state = output_register & kStateMask;
}

bool block_contains_reserved (int block_start, int block_length)
{
  return block_start <= kReservedBit && block_start + block_length > kReservedBit;
}

struct PackedSurvivor
{
  double metric {kNegativeMetric};
  std::int64_t key {0};
};

inline bool valid (PackedSurvivor const& s)
{
  return (s.key >> kPackedValidBit) & 1;
}

inline bool packed_precedes (PackedSurvivor const& l, PackedSurvivor const& r)
{
  if (l.metric > r.metric) return true;
  if (l.metric < r.metric) return false;
  return l.key < r.key;
}

struct PoolCandidate
{
  int bits[kInformationBits] {};
  int start_state {-1};
  std::int64_t identity {0};
  double clean_metric {kNegativeMetric};
  double wava_metric {kNegativeMetric};
  bool crc_valid {false};
};

bool candidate_precedes (PoolCandidate const& l, PoolCandidate const& r)
{
  if (l.clean_metric > r.clean_metric) return true;
  if (l.clean_metric < r.clean_metric) return false;
  if (l.identity != r.identity) return l.identity < r.identity;
  if (l.start_state != r.start_state) return l.start_state < r.start_state;
  return l.wava_metric > r.wava_metric;
}

struct Plan
{
  int coherent_length {0};
  int block_count {0};
  int max_branch_count {0};
  int energy_count {0};
  int next_state[kStateCount][2];
  int tone[kStateCount][2];
  std::vector<int> block_starts, block_lengths, branch_counts, energy_offsets;
  // [block][word]
  std::vector<std::int64_t> branch_identities;
  // [block][word][state]
  std::vector<int> branch_end_states;
  std::vector<int> branch_sequence_indices;

  int idx (int block, int word, int state) const
  {
    return (block * max_branch_count + word) * kStateCount + state;
  }

  void init (int L)
  {
    coherent_length = L;
    block_count = (kInformationBits + L - 1) / L;
    max_branch_count = 1 << L;
    for (int b = 0; b < 2; ++b)
      for (int s = 0; s < kStateCount; ++s)
        transition (s, b, next_state[s][b], tone[s][b]);

    block_starts.assign (block_count, 0);
    block_lengths.assign (block_count, 0);
    branch_counts.assign (block_count, 0);
    energy_offsets.assign (block_count, 0);
    branch_identities.assign (static_cast<std::size_t> (block_count * max_branch_count), 0);
    branch_end_states.assign (static_cast<std::size_t> (block_count * max_branch_count * kStateCount), 0);
    branch_sequence_indices.assign (branch_end_states.size (), 0);
    energy_count = 0;
    for (int bi = 0; bi < block_count; ++bi)
      {
        int const block_start = 1 + bi * L;
        int const block_length = std::min (L, kInformationBits - block_start + 1);
        block_starts[bi] = block_start;
        block_lengths[bi] = block_length;
        branch_counts[bi] = 1 << block_length;
        energy_offsets[bi] = energy_count;
        energy_count += 1 << (2 * block_length);
        for (int word = 0; word < branch_counts[bi]; ++word)
          {
            int input_bits[4] = {0, 0, 0, 0};
            for (int off = 1; off <= block_length; ++off)
              input_bits[off - 1] = (word >> (block_length - off)) & 1;
            std::int64_t identity = 0;
            for (int off = 1; off <= block_length; ++off)
              if (input_bits[off - 1])
                identity |= std::int64_t {1} << (kInformationBits - block_start - off + 1);
            branch_identities[static_cast<std::size_t> (bi * max_branch_count + word)] = identity;
            for (int s = 0; s < kStateCount; ++s)
              {
                int state = s;
                int tone_word = 0;
                for (int off = 1; off <= block_length; ++off)
                  {
                    tone_word = 4 * tone_word + tone[state][input_bits[off - 1]];
                    state = next_state[state][input_bits[off - 1]];
                  }
                branch_end_states[static_cast<std::size_t> (idx (bi, word, s))] = state;
                branch_sequence_indices[static_cast<std::size_t> (idx (bi, word, s))] =
                    energy_offsets[bi] + tone_word;
              }
          }
      }
  }
};

struct ListDecoder
{
  Plan plan;
  std::vector<PackedSurvivor> previous, current;   // [state*width + rank]
  std::vector<double> energies;

  explicit ListDecoder (int L)
  {
    plan.init (L);
    previous.resize (kStateCount * kPerStateWidth);
    current.resize (kStateCount * kPerStateWidth);
    energies.resize (static_cast<std::size_t> (plan.energy_count));
  }

  void precompute (SymbolCorrelations const& c)
  {
    for (int bi = 0; bi < plan.block_count; ++bi)
      {
        int const start = plan.block_starts[bi];
        int const L = plan.block_lengths[bi];
        int const count = 1 << (2 * L);
        for (int word = 0; word < count; ++word)
          {
            std::complex<float> sum {0.f, 0.f};
            for (int off = 1; off <= L; ++off)
              {
                int const t = (word >> (2 * (L - off))) & 3;
                sum += c[static_cast<std::size_t> (start + off - 2)][static_cast<std::size_t> (t)];
              }
            float const p = sum.real () * sum.real () + sum.imag () * sum.imag ();
            energies[static_cast<std::size_t> (plan.energy_offsets[bi] + word)] =
                static_cast<double> (p) / static_cast<double> (L);
          }
      }
  }

  static void insert (PackedSurvivor sel[4], PackedSurvivor const& cand, bool dedupe)
  {
    int const width = kPerStateWidth;
    if (dedupe)
      {
        for (int slot = 1; slot <= width; ++slot)
          {
            if (!valid (sel[slot - 1])) continue;
            if (sel[slot - 1].key == cand.key)
              {
                if (cand.metric <= sel[slot - 1].metric) return;
                for (int k = 1; k <= 3; ++k)
                  if (k >= slot && k < width) sel[k - 1] = sel[k];
                sel[width - 1].key = 0;
                break;
              }
          }
      }
    int ins = width + 1;
    for (int slot = 1; slot <= width; ++slot)
      if (!valid (sel[slot - 1]) || packed_precedes (cand, sel[slot - 1]))
        {
          ins = slot;
          break;
        }
    if (ins > width) return;
    for (int slot = 4; slot >= 2; --slot)
      if (slot <= width && slot > ins) sel[slot - 1] = sel[slot - 2];
    sel[ins - 1] = cand;
  }

  void advance (int bi, bool prune, bool ordered)
  {
    int const width = kPerStateWidth;
    int const branch_count = plan.branch_counts[bi];
    int const stride = kStateCount / branch_count;
    bool const prune_block = prune && block_contains_reserved (plan.block_starts[bi], plan.block_lengths[bi]);
    PackedSurvivor sel[4];
    for (int end_state = 0; end_state < kStateCount; ++end_state)
      {
        // Il Fortran azzera solo le chiavi: le metriche restano quelle del
        // giro prima, ma senza il bit di validita' non vengono mai lette.
        for (auto& s : sel) s.key = 0;
        int const word = end_state & (branch_count - 1);
        std::int64_t const branch_identity =
            plan.branch_identities[static_cast<std::size_t> (bi * plan.max_branch_count + word)];
        if (!(prune_block && ((branch_identity >> (kInformationBits - kReservedBit)) & 1)))
          {
            for (int state = end_state / branch_count; state <= kStateCount - 1; state += stride)
              {
                double const branch_metric = energies[static_cast<std::size_t> (
                    plan.branch_sequence_indices[static_cast<std::size_t> (plan.idx (bi, word, state))])];
                for (int rank = 0; rank < width; ++rank)
                  {
                    PackedSurvivor const& p = previous[static_cast<std::size_t> (state * width + rank)];
                    if (!valid (p)) continue;
                    double const metric = p.metric + branch_metric;
                    if (valid (sel[width - 1]) && metric < sel[width - 1].metric)
                      {
                        if (ordered) break;
                        continue;
                      }
                    PackedSurvivor ext = p;
                    ext.metric = metric;
                    ext.key |= branch_identity;
                    insert (sel, ext, bi == 0);
                  }
              }
          }
        for (int rank = 0; rank < width; ++rank)
          current[static_cast<std::size_t> (end_state * width + rank)] = sel[rank];
      }
  }

  static std::int64_t reverse_identity (std::int64_t key)
  {
    std::int64_t id = 0;
    for (int b = 0; b < kInformationBits; ++b)
      if ((key >> b) & 1) id |= std::int64_t {1} << (kInformationBits - b - 1);
    return id;
  }

  void score_identity (std::int64_t identity, double& metric, bool& closed) const
  {
    int start_state = 0;
    for (int b = kInformationBits - kMemoryNu + 1; b <= kInformationBits; ++b)
      start_state = ((start_state << 1) | static_cast<int> ((identity >> (b - 1)) & 1)) & (kStateCount - 1);
    int state = start_state;
    metric = 0.0;
    for (int bi = 0; bi < plan.block_count; ++bi)
      {
        int word = 0;
        for (int off = 0; off < plan.block_lengths[bi]; ++off)
          word = 2 * word + static_cast<int> ((identity >> (plan.block_starts[bi] + off - 1)) & 1);
        metric += energies[static_cast<std::size_t> (
            plan.branch_sequence_indices[static_cast<std::size_t> (plan.idx (bi, word, state))])];
        state = plan.branch_end_states[static_cast<std::size_t> (plan.idx (bi, word, state))];
      }
    closed = state == start_state;
  }

  // jtty_tbcc_list_wava_optimized con prune_reserved_zero=.true.
  int run (SymbolCorrelations const& c, PoolCandidate out[kHypotheses])
  {
    precompute (c);
    bool ordered = true;
    for (double e : energies)
      if (!std::isfinite (e))
        {
          ordered = false;
          break;
        }
    for (auto& s : previous)
      {
        s.key = 0;
        s.metric = kNegativeMetric;
      }
    for (int state = 0; state < kStateCount; ++state)
      {
        auto& s = previous[static_cast<std::size_t> (state * kPerStateWidth)];
        s.key = (std::int64_t {1} << kPackedValidBit) | (static_cast<std::int64_t> (state) << kInformationBits);
        s.metric = 0.0;
      }
    for (int wrap = 0; wrap < kWraps; ++wrap)
      {
        for (int state = 0; state < kStateCount; ++state)
          for (int rank = 0; rank < kPerStateWidth; ++rank)
            {
              auto& s = previous[static_cast<std::size_t> (state * kPerStateWidth + rank)];
              if (!valid (s)) continue;
              s.key = (std::int64_t {1} << kPackedValidBit) | (static_cast<std::int64_t> (state) << kInformationBits);
            }
        for (int bi = 0; bi < plan.block_count; ++bi)
          {
            advance (bi, true, ordered);
            std::swap (previous, current);
          }
      }

    std::vector<PoolCandidate> pool;
    pool.reserve (kStateCount * kPerStateWidth);
    std::unordered_map<std::int64_t, std::size_t> seen;
    for (int state = 0; state < kStateCount; ++state)
      for (int rank = 0; rank < kPerStateWidth; ++rank)
        {
          PackedSurvivor const& s = previous[static_cast<std::size_t> (state * kPerStateWidth + rank)];
          if (!valid (s)) continue;
          if (static_cast<int> ((s.key >> kInformationBits) & ((1 << kPackedOriginBits) - 1)) != state) continue;
          std::int64_t const identity = reverse_identity (s.key);
          auto const it = seen.find (identity);
          if (it != seen.end ())
            {
              PoolCandidate& p = pool[it->second];
              if (s.metric > p.wava_metric || (!(s.metric < p.wava_metric) && state < p.start_state))
                {
                  p.wava_metric = s.metric;
                  p.start_state = state;
                }
              continue;
            }
          PoolCandidate p;
          p.identity = identity;
          p.start_state = state;
          p.wava_metric = s.metric;
          for (int b = 0; b < kInformationBits; ++b) p.bits[b] = static_cast<int> ((identity >> b) & 1);
          bool closed;
          score_identity (identity, p.clean_metric, closed);
          p.crc_valid = tbcc_crc_valid (p.bits);
          seen.emplace (identity, pool.size ());
          pool.push_back (p);
        }
    std::sort (pool.begin (), pool.end (), candidate_precedes);
    int const count = std::min (kHypotheses, static_cast<int> (pool.size ()));
    for (int i = 0; i < count; ++i) out[i] = pool[static_cast<std::size_t> (i)];
    return count;
  }

  // decode_rung
  bool rung (SymbolCorrelations const& c, int payload[kPayloadBits])
  {
    PoolCandidate cand[kHypotheses];
    int const count = run (c, cand);
    for (int rank = 0; rank < count; ++rank)
      {
        if (!cand[rank].crc_valid) continue;
        bool all_zero = true;
        for (int b = 0; b < kPayloadBits; ++b)
          if (cand[rank].bits[b] != 0)
            {
              all_zero = false;
              break;
            }
        if (all_zero) return false;
        for (int b = 0; b < kPayloadBits; ++b) payload[b] = cand[rank].bits[b];
        return true;
      }
    return false;
  }
};

}

void tbcc_encode (int const payload[34], int tones[kInformationBits])
{
  int info[kInformationBits];
  for (int i = 0; i < kPayloadBits; ++i) info[i] = payload[i];
  int crc = 0;
  for (int i = 0; i < kPayloadBits; ++i)
    {
      crc ^= payload[i] << (kCrcBits - 1);
      if (crc & kOuterTopBit) crc = (crc << 1) ^ kOuterPolynomial;
      else crc <<= 1;
      crc &= kOuterRegisterMask;
    }
  for (int i = 1; i <= kCrcBits; ++i) info[kPayloadBits + i - 1] = (crc >> (kCrcBits - i)) & 1;

  int state = 0;
  for (int t = 0; t < kMemoryNu; ++t)
    {
      int const bit = info[kInformationBits - (kMemoryNu - 1) + t - 1];
      state = ((state << 1) | bit) & (kStateCount - 1);
    }
  for (int t = 0; t < kInformationBits; ++t)
    {
      int const bit = info[t];
      int const g = ((state << 1) | bit) & kRegisterMask;
      int const b0 = parity (g & kGenerator0);
      int const b1 = parity (g & kGenerator1);
      state = ((state << 1) | bit) & (kStateCount - 1);
      int tone = 0;
      if (b0 == 0 && b1 == 0) tone = 0;
      if (b0 == 0 && b1 == 1) tone = 1;
      if (b0 == 1 && b1 == 1) tone = 2;
      if (b0 == 1 && b1 == 0) tone = 3;
      tones[t] = tone;
    }
}

bool tbcc_crc_valid (int const bits[kInformationBits])
{
  int crc = 0;
  for (int i = 0; i < kInformationBits; ++i)
    {
      crc ^= bits[i] << (kCrcBits - 1);
      if (crc & kOuterTopBit) crc = (crc << 1) ^ kOuterPolynomial;
      else crc <<= 1;
      crc &= kOuterRegisterMask;
    }
  return crc == 0;
}

struct TbccDecoder::Impl
{
  ListDecoder l1 {1};
  ListDecoder l2 {2};
  ListDecoder l4 {4};
};

TbccDecoder::TbccDecoder () : m_ {new Impl} {}
TbccDecoder::~TbccDecoder () = default;

bool TbccDecoder::decode (SymbolCorrelations const& correlations,
                          SymbolCorrelations const& half_correlations, int payload[34])
{
  for (int i = 0; i < kPayloadBits; ++i) payload[i] = 0;
  if (m_->l1.rung (correlations, payload)) return true;
  if (m_->l2.rung (correlations, payload)) return true;
  if (m_->l4.rung (correlations, payload)) return true;
  // Le energie dei mezzi simboli non hanno fase: ha senso solo L=1.
  if (m_->l1.rung (half_correlations, payload)) return true;
  for (int i = 0; i < kPayloadBits; ++i) payload[i] = 0;
  return false;
}

}
}
