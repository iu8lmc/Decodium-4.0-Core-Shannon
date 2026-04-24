#ifndef AUDIODEVICE_HPP__
#define AUDIODEVICE_HPP__

#include <QIODevice>
#include <QtGlobal>

class QDataStream;

//
// abstract base class for audio devices
//
class AudioDevice : public QIODevice
{
public:
  enum Channel {Mono, Left, Right, Both}; // these are mapped to combobox index so don't change

  static char const * toString (Channel c)
  {
    return Mono == c ? "Mono" : Left == c ? "Left" : Right == c ? "Right" : "Both";
  }

  static Channel fromString (QString const& str)
  {
    QString s (str.toCaseFolded ().trimmed ().toLatin1 ());
    return "both" == s ? Both : "right" == s ? Right : "left" == s ? Left : Mono;
  }

  bool initialize (OpenMode mode, Channel channel, int sourceChannelCount = -1);

  bool isSequential () const override {return true;}

  size_t bytesPerFrame () const {return sizeof (qint16) * m_sourceChannelCount;}

  Channel channel () const {return m_channel;}
  void setInputGainLinear (float gain) {m_inputGainLinear = qMax (0.0f, gain);}
  float inputGainLinear () const {return m_inputGainLinear;}

protected:
  AudioDevice (QObject * parent = nullptr)
    : QIODevice {parent}
  {
  }

  void store (char const * source, size_t numFrames, qint16 * dest)
  {
    auto apply_input_gain = [this] (qint16 sample) {
      if (m_inputGainLinear == 1.0f)
        {
          return sample;
        }
      return static_cast<qint16> (qBound (-32768,
                                          qRound (static_cast<float> (sample) * m_inputGainLinear),
                                          32767));
    };
    int const sourceChannels = qMax (1, m_sourceChannelCount);
    qint16 const * begin (reinterpret_cast<qint16 const *> (source));
    for (qint16 const * i = begin; i != begin + numFrames * sourceChannels; i += sourceChannels)
      {
        qint16 const left = *i;
        qint16 const right = sourceChannels > 1 ? *(i + 1) : left;
        switch (m_channel)
          {
          case Mono:
            // Match Decodium3/Qt5 mono behavior: avoid OS/Qt downmixing but
            // keep the first hardware channel. Users can explicitly select
            // Right when an interface carries RX audio only on that side.
            *dest++ = apply_input_gain (left);
            break;

          case Right:
            *dest++ = apply_input_gain (right);
            break;

          case Both:
            *dest++ = apply_input_gain (static_cast<qint16> ((static_cast<int> (left) + static_cast<int> (right)) / 2));
            break;

          case Left:
            *dest++ = apply_input_gain (left);
            break;
          }
      }
  }

  qint16 * load (qint16 const sample, qint16 * dest)
  {
    switch (m_channel)
      {
      case Mono:
        *dest++ = sample;
        break;

      case Left:
        *dest++ = sample;
        *dest++ = 0;
        break;

      case Right:
        *dest++ = 0;
        *dest++ = sample;
        break;

      case Both:
        *dest++ = sample;
        *dest++ = sample;
        break;
      }
    return dest;
  }

private:
  Channel m_channel;
  int m_sourceChannelCount {1};
  float m_inputGainLinear {1.0f};
};

Q_DECLARE_METATYPE (AudioDevice::Channel);

#endif
