#ifndef JT9_WIDE_DECODER_HPP
#define JT9_WIDE_DECODER_HPP

#include <QStringList>
#include <QVector>

#include <array>
#include <cstdint>
#include <vector>

namespace decodium
{
namespace legacyjt
{
struct DecodeRequest;
}

namespace jt9wide
{

namespace detail
{

struct SyncPassResult
{
  int lagpk {0};
  std::vector<float> ccfred;
  std::array<float, 28> ccfblue {};
};

struct DetectionEstimate
{
  float sync {0.0f};
  float xdt0 {0.0f};
  float f0 {0.0f};
  float width {0.0f};
  float base {0.0f};
  float rms {1.0f};
  int lagpk {0};
  std::vector<float> ccfred;
  std::array<float, 28> ccfblue {};
  bool ok {false};
};

struct SoftsymResult
{
  float xdt1 {0.0f};
  int nsnr {0};
  std::array<std::int8_t, 207> soft {};
  bool ok {false};
};

SyncPassResult run_sync_pass (QVector<float> const& ss, int nzhsym, int lag1, int lag2, int ia, int ib, int nadd);
DetectionEstimate estimate_wide_jt9 (legacyjt::DecodeRequest const& request);
SoftsymResult compute_softsym (QVector<short> const& audio, float xdt0, float f0, float width,
                               int nsubmode);

}

QStringList decode_wide_jt9 (legacyjt::DecodeRequest const& request);

}
}

#endif
