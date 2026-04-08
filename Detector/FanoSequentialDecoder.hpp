#ifndef FANO_SEQUENTIAL_DECODER_HPP
#define FANO_SEQUENTIAL_DECODER_HPP

namespace decodium
{
namespace fano
{

int sequential_decode (unsigned int* metric, unsigned int* cycles, unsigned int* maxnp,
                       unsigned char* data, unsigned char const* symbols,
                       unsigned int nbits, int const mettab[2][256],
                       int delta, unsigned int maxcycles);

}
}

#endif
