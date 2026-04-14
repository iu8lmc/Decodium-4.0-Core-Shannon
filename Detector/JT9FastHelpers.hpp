#ifndef JT9_FAST_HELPERS_HPP
#define JT9_FAST_HELPERS_HPP

#include <array>
#include <cstdint>
#include <vector>

namespace decodium
{
namespace jt9fast
{

struct Sync9Result
{
  int lagpk {0};
  int ipk {0};
  float ccfbest {0.0f};
  std::array<float, 9 * 85> ss2 {};
  std::array<float, 8 * 69> ss3 {};
};

void spec9f_compute (short const* id2, int npts, int nsps, std::vector<float>& s1, int* jz_out,
                     int* nq_out);
void foldspec9f_compute (float const* s1, int nq, int jz, int ja, int jb, std::array<float, 240 * 340>& s2);
Sync9Result sync9f_compute (std::array<float, 240 * 340> const& s2, int nq, int nfa, int nfb);
void softsym9f_compute (std::array<float, 9 * 85> const& ss2, std::array<float, 8 * 69> const& ss3,
                        std::array<std::int8_t, 207>& soft_symbols);

}
}

#endif
