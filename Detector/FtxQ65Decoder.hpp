#ifndef FTX_Q65_DECODER_HPP
#define FTX_Q65_DECODER_HPP

#include <complex>
#include <vector>

namespace decodium
{
namespace q65
{

bool ana64_transform (short const* iwave, int npts, std::vector<std::complex<float>>& c0);

}
}

#endif // FTX_Q65_DECODER_HPP
