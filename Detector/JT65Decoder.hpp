#ifndef JT65DECODER_HPP
#define JT65DECODER_HPP

#include <QStringList>

#include <vector>

namespace decodium
{
namespace legacyjt
{
struct DecodeRequest;
}

namespace jt65
{

struct AverageState
{
  int avg_last_utc {-999};
  int avg_last_freq {-999};
  int avg_ring_index {0};
  int clear_avg65 {1};
  std::vector<int> avg_iutc;
  std::vector<int> avg_nfsave;
  std::vector<int> avg_nflipsave;
  std::vector<float> avg_s1save;
  std::vector<float> avg_s3save;
  std::vector<float> avg_dtsave;
  std::vector<float> avg_syncsave;
};

QStringList decode_async_jt65 (legacyjt::DecodeRequest const& request, AverageState* state);

}
}

#endif
