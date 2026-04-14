#include "Detector/FanoSequentialDecoder.hpp"

#include <vector>

namespace decodium
{
namespace fano
{
namespace
{

constexpr unsigned long kPoly1 = 0xf2d05351UL;
constexpr unsigned long kPoly2 = 0xe4613c47UL;

struct Node
{
  unsigned long encstate {0};
  long gamma {0};
  int metrics[4] {};
  int tm[2] {};
  int i {0};
};

unsigned char parity8 (unsigned int value)
{
  value ^= value >> 4;
  value &= 0x0fu;
  return static_cast<unsigned char> ((0x6996u >> value) & 0x1u);
}

unsigned int encode_symbol (unsigned long encstate)
{
  unsigned long tmp = encstate & kPoly1;
  tmp ^= tmp >> 16;
  unsigned int sym =
      static_cast<unsigned int> (parity8 (static_cast<unsigned int> ((tmp ^ (tmp >> 8)) & 0xffu))) << 1;

  tmp = encstate & kPoly2;
  tmp ^= tmp >> 16;
  sym |= parity8 (static_cast<unsigned int> ((tmp ^ (tmp >> 8)) & 0xffu));
  return sym;
}

}

int sequential_decode (unsigned int* metric, unsigned int* cycles, unsigned int* maxnp,
                       unsigned char* data, unsigned char const* symbols,
                       unsigned int nbits, int const mettab[2][256],
                       int delta, unsigned int maxcycles)
{
  if (!metric || !cycles || !maxnp || !data || !symbols || nbits < 31)
    {
      return -1;
    }

  std::vector<Node> nodes (static_cast<std::size_t> (nbits + 1));
  Node* const first = nodes.data ();
  Node* const lastnode = first + (nbits - 1);
  Node* const tail = first + (nbits - 31);
  *maxnp = 0;

  unsigned char const* input_symbols = symbols;
  for (Node* np = first; np <= lastnode; ++np)
    {
      np->metrics[0] = mettab[0][input_symbols[0]] + mettab[0][input_symbols[1]];
      np->metrics[1] = mettab[0][input_symbols[0]] + mettab[1][input_symbols[1]];
      np->metrics[2] = mettab[1][input_symbols[0]] + mettab[0][input_symbols[1]];
      np->metrics[3] = mettab[1][input_symbols[0]] + mettab[1][input_symbols[1]];
      input_symbols += 2;
    }

  Node* np = first;
  np->encstate = 0;

  unsigned int lsym = encode_symbol (np->encstate);
  int m0 = np->metrics[lsym];
  int m1 = np->metrics[3u ^ lsym];
  if (m0 > m1)
    {
      np->tm[0] = m0;
      np->tm[1] = m1;
    }
  else
    {
      np->tm[0] = m1;
      np->tm[1] = m0;
      ++np->encstate;
    }
  np->i = 0;

  unsigned int const total_cycles = maxcycles * nbits;
  int threshold = 0;
  np->gamma = 0;

  unsigned int i = 0;
  for (i = 1; i <= total_cycles; ++i)
    {
      unsigned int const progress = static_cast<unsigned int> (np - first);
      if (progress > *maxnp)
        {
          *maxnp = progress;
        }

      int const next_gamma = static_cast<int> (np->gamma + np->tm[np->i]);
      if (next_gamma >= threshold)
        {
          if (np->gamma < threshold + delta)
            {
              while (next_gamma >= threshold + delta)
                {
                  threshold += delta;
                }
            }

          np[1].gamma = next_gamma;
          np[1].encstate = np->encstate << 1;
          if (++np == (lastnode + 1))
            {
              break;
            }

          lsym = encode_symbol (np->encstate);
          if (np >= tail)
            {
              np->tm[0] = np->metrics[lsym];
            }
          else
            {
              m0 = np->metrics[lsym];
              m1 = np->metrics[3u ^ lsym];
              if (m0 > m1)
                {
                  np->tm[0] = m0;
                  np->tm[1] = m1;
                }
              else
                {
                  np->tm[0] = m1;
                  np->tm[1] = m0;
                  ++np->encstate;
                }
            }
          np->i = 0;
          continue;
        }

      for (;;)
        {
          if (np == first || np[-1].gamma < threshold)
            {
              threshold -= delta;
              if (np->i != 0)
                {
                  np->i = 0;
                  np->encstate ^= 1u;
                }
              break;
            }

          if (--np < tail && np->i != 1)
            {
              ++np->i;
              np->encstate ^= 1u;
              break;
            }
        }
    }

  *metric = static_cast<unsigned int> (np->gamma);
  unsigned int const nbytes = nbits >> 3;
  np = first + 7;
  for (unsigned int byte = 0; byte < nbytes; ++byte)
    {
      *data++ = static_cast<unsigned char> (np->encstate);
      np += 8;
    }
  *cycles = i + 1;

  if (i >= total_cycles)
    {
      return -1;
    }
  return 0;
}

}
}
