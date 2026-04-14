#ifndef DECODIUM_FTX_FST4_LDPC_HPP
#define DECODIUM_FTX_FST4_LDPC_HPP

#include <array>
#include <vector>

namespace decodium
{
namespace fst4
{

bool decode240_101_native (std::array<float, 240> const& llr,
                           int keff, int maxosd, int norder,
                           std::array<signed char, 240> const& apmask,
                           std::array<signed char, 101>& message101_out,
                           std::array<signed char, 240>& cw_out,
                           int& ntype_out, int& nharderror_out, float& dmin_out);

bool decode240_74_native (std::array<float, 240> const& llr,
                          int keff, int maxosd, int norder,
                          std::array<signed char, 240> const& apmask,
                          std::array<signed char, 74>& message74_out,
                          std::array<signed char, 240>& cw_out,
                          int& ntype_out, int& nharderror_out, float& dmin_out);

void dopspread_native (std::array<int, 160> const& itone,
                       std::vector<short> const& iwave,
                       int nsps, int nmax, int ndown, int hmod,
                       int i0, float fc, float& fmid, float& w50);

}
}

#endif
