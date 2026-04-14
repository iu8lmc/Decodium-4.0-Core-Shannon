#ifndef JT9_FAST_DECODER_HPP
#define JT9_FAST_DECODER_HPP

#include <QByteArray>
#include <QStringList>

#include <array>
#include <cstdint>
#include <vector>

namespace decodium
{
namespace jt9fast
{

struct DecodeRequest;

struct FastDecodeState
{
  int nsubmode0 {-1};
  int ntot {0};
  std::vector<float> s1;
};

QByteArray decode_soft_symbols (std::array<std::int8_t, 207> const& soft, int limit, int* nlim);
QStringList decode_fast9 (DecodeRequest const& request, FastDecodeState* state);

}
}

#endif
