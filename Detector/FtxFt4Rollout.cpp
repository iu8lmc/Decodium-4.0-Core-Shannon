#include <algorithm>
#include <cstdlib>
#ifdef _WIN32
#  include <stdlib.h>
// UCRT/MinGW compatibility: setenv/unsetenv not available on Windows
static inline int setenv(const char* name, const char* value, int /*overwrite*/)
{
  return _putenv_s(name, value);
}
static inline int unsetenv(const char* name)
{
  return _putenv_s(name, "");
}
#endif

namespace
{

char constexpr kFt4DspStageEnv[] {"DECODIUM_FT4_CPP_DSP_STAGE"};
constexpr int kFt4StableDspStage {4};

}

extern "C" void ftx_ft4_cpp_dsp_rollout_stage_reset_c ()
{
  unsetenv (kFt4DspStageEnv);
}

extern "C" void ftx_ft4_cpp_dsp_rollout_stage_override_c (int stage)
{
  int const clamped = std::max (0, std::min (kFt4StableDspStage, stage));
  char value[2] = {static_cast<char> ('0' + clamped), '\0'};
  setenv (kFt4DspStageEnv, value, 1);
}
