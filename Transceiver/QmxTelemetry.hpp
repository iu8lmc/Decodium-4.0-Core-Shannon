#ifndef QMX_TELEMETRY_HPP_
#define QMX_TELEMETRY_HPP_

#include <algorithm>
#include <array>
#include <limits>

#include <QByteArray>

namespace decodium
{
namespace qmx_telemetry
{
  inline constexpr std::array<int, 5> swr_poll_delays_ms {{120, 350, 525, 700, 1100}};

  inline bool ignore_scheduled_swr_sample (int delay_ms)
  {
    return delay_ms == swr_poll_delays_ms.front ();
  }

  inline bool parse_unsigned_reply (QByteArray const& reply, QByteArray const& prefix,
                                    unsigned int multiplier, unsigned int * value)
  {
    if (!value || multiplier == 0)
      {
        return false;
      }

    QByteArray const frame = reply.trimmed ();
    if (!frame.startsWith (prefix) || !frame.endsWith (';'))
      {
        return false;
      }

    QByteArray const digits = frame.mid (prefix.size (), frame.size () - prefix.size () - 1);
    if (digits.isEmpty ())
      {
        return false;
      }

    unsigned int parsed = 0;
    unsigned int const maximumBeforeMultiply = std::numeric_limits<unsigned int>::max () / multiplier;
    for (char const digit : digits)
      {
        if (digit < '0' || digit > '9')
          {
            return false;
          }

        unsigned int const number = static_cast<unsigned int> (digit - '0');
        if (parsed > (maximumBeforeMultiply - number) / 10)
          {
            return false;
          }
        parsed = parsed * 10 + number;
      }

    *value = parsed * multiplier;
    return true;
  }

  // QMX PC reports tenths of a watt. Decodium stores power in milliwatts.
  inline bool parse_power_milliwatts (QByteArray const& reply, unsigned int * milliwatts)
  {
    return parse_unsigned_reply (reply, QByteArrayLiteral ("PC"), 100, milliwatts);
  }

  // QMX SW reports hundredths of an SWR unit, matching TransceiverState.
  inline bool parse_swr_hundredths (QByteArray const& reply, unsigned int * hundredths)
  {
    return parse_unsigned_reply (reply, QByteArrayLiteral ("SW"), 1, hundredths);
  }

  enum class SwrFilterDecision
  {
    SettlingIgnored,
    InvalidSample,
    Collecting,
    Safe,
    HighPending,
    HighConfirmed
  };

  inline char const * swr_filter_decision_name (SwrFilterDecision decision)
  {
    switch (decision)
      {
      case SwrFilterDecision::SettlingIgnored: return "settling-first-sample-ignored";
      case SwrFilterDecision::InvalidSample: return "invalid-sample";
      case SwrFilterDecision::Collecting: return "collecting-three-sample-window";
      case SwrFilterDecision::Safe: return "median-at-or-below-threshold";
      case SwrFilterDecision::HighPending: return "high-not-yet-persistent";
      case SwrFilterDecision::HighConfirmed: return "persistent-high-swr";
      }
    return "unknown";
  }

  struct SwrFilterResult
  {
    unsigned int raw_hundredths {0};
    unsigned int filtered_hundredths {0};
    unsigned int published_hundredths {0};
    unsigned int samples {0};
    unsigned int consecutive_high {0};
    bool stop_eligible {false};
    SwrFilterDecision decision {SwrFilterDecision::Collecting};
  };

  // The QMX raw CAT fallback can report the preceding TX value immediately
  // after PTT changes.  This filter is deliberately kept here, beside the
  // fallback parser, so native Hamlib meter paths for every other radio remain
  // untouched.
  class SwrSafetyFilter
  {
  public:
    void reset ()
    {
      samples_.fill (0);
      sample_count_ = 0;
      next_sample_ = 0;
      consecutive_high_ = 0;
    }

    SwrFilterResult process (unsigned int raw_hundredths,
                             unsigned int threshold_hundredths,
                             bool settling_sample = false)
    {
      SwrFilterResult result;
      result.raw_hundredths = raw_hundredths;

      if (settling_sample)
        {
          consecutive_high_ = 0;
          result.samples = static_cast<unsigned int> (sample_count_);
          result.consecutive_high = consecutive_high_;
          result.decision = SwrFilterDecision::SettlingIgnored;
          return result;
        }

      if (raw_hundredths < 100 || threshold_hundredths < 100)
        {
          consecutive_high_ = 0;
          result.samples = static_cast<unsigned int> (sample_count_);
          result.consecutive_high = consecutive_high_;
          result.decision = SwrFilterDecision::InvalidSample;
          return result;
        }

      samples_[next_sample_] = raw_hundredths;
      next_sample_ = (next_sample_ + 1) % samples_.size ();
      sample_count_ = std::min (sample_count_ + 1, samples_.size ());
      if (raw_hundredths > threshold_hundredths)
        {
          ++consecutive_high_;
        }
      else
        {
          consecutive_high_ = 0;
        }

      result.samples = static_cast<unsigned int> (sample_count_);
      result.consecutive_high = consecutive_high_;
      if (sample_count_ < samples_.size ())
        {
          result.decision = SwrFilterDecision::Collecting;
          return result;
        }

      std::array<unsigned int, 3> ordered = samples_;
      std::sort (ordered.begin (), ordered.end ());
      result.filtered_hundredths = ordered[1];
      if (result.filtered_hundredths <= threshold_hundredths)
        {
          result.published_hundredths = result.filtered_hundredths;
          result.decision = SwrFilterDecision::Safe;
          return result;
        }

      if (consecutive_high_ >= 2)
        {
          result.published_hundredths = result.filtered_hundredths;
          result.stop_eligible = true;
          result.decision = SwrFilterDecision::HighConfirmed;
          return result;
        }

      // Keep the value at the configured limit until persistence is proven.
      // The bridge's existing protection trips only on values strictly above
      // the limit, so a lone spike cannot terminate TX.
      result.published_hundredths = threshold_hundredths;
      result.decision = SwrFilterDecision::HighPending;
      return result;
    }

  private:
    std::array<unsigned int, 3> samples_ {{0, 0, 0}};
    std::size_t sample_count_ {0};
    std::size_t next_sample_ {0};
    unsigned int consecutive_high_ {0};
  };
}
}

#endif
