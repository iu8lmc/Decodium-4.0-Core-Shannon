// decodium_rx.cpp -- Decodium RX: ricevitore FT8/FT4/FT2 da terminale, solo RX.
//
// Porta su Windows (e su qualunque piattaforma di Decodium) il "decodium-rx"
// di DecodiumOS (mods/54-decodium-rx): la' e' uno script Python che cattura
// l'audio con parec/arecord e lancia, uno slot alla volta, un core a riga di
// comando ricavato da utils/jt9.cpp. Qui e' un solo eseguibile:
//   - la cattura e' Qt Multimedia (QAudioSource), a 48 kHz decimati a 12 kHz
//     con lo stesso filtro FIR della ricezione di Decodium (Fil4Filter);
//   - la decodifica usa i worker FT8/FT4/FT2 di Decodium NELLO STESSO
//     processo, su un thread dedicato: i worker restano vivi fra uno slot e
//     l'altro, quindi conservano la memoria dei nominativi gia' sentiti come
//     nell'applicazione, e non si paga l'avvio di un processo per slot.
// Non trasmette mai e non tocca CAT o PTT.
//
// Righe, colori, domande iniziali, ALL.TXT e opzioni seguono l'originale,
// cosi' chi passa da DecodiumOS a Windows ritrova lo stesso strumento.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimeZone>
#include <QTimer>
#include <QtEndian>

#include <memory>

#include <boost/log/core.hpp>

#include "Detector/FT2DecodeWorker.hpp"
#include "Detector/FT4DecodeWorker.hpp"
#include "Detector/FT8DecodeWorker.hpp"
#include "Detector/Fil4Filter.hpp"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace
{

constexpr int kRate {12000};

struct ModeInfo
{
  char const* name;
  double period;   // durata dello slot, s
  int samples;     // campioni che il decoder legge dall'inizio dello slot
};

ModeInfo const kModes[] {
  {"ft8", 15.0, 50 * 3456},
  {"ft4", 7.5, 21 * 3456},
  {"ft2", 3.75, 45000},
};

ModeInfo const* findMode (QString const& name)
{
  for (auto const& m : kModes)
    {
      if (name.compare (QLatin1String (m.name), Qt::CaseInsensitive) == 0)
        {
          return &m;
        }
    }
  return nullptr;
}

std::atomic<bool> g_stop {false};

#ifdef Q_OS_WIN
BOOL WINAPI consoleHandler (DWORD type)
{
  if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT)
    {
      g_stop = true;
      return TRUE;
    }
  return FALSE;
}
#else
void signalHandler (int)
{
  g_stop = true;
}
#endif

bool stdoutIsTerminal ()
{
#ifdef Q_OS_WIN
  return _isatty (_fileno (stdout)) != 0;
#else
  return isatty (fileno (stdout)) != 0;
#endif
}

int terminalWidth ()
{
#ifdef Q_OS_WIN
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (GetConsoleScreenBufferInfo (GetStdHandle (STD_OUTPUT_HANDLE), &info))
    {
      return info.srWindow.Right - info.srWindow.Left + 1;
    }
#else
  winsize ws {};
  if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
    {
      return ws.ws_col;
    }
#endif
  return 100;
}

// Su Windows i colori ANSI e l'UTF-8 vanno chiesti alla console.
bool prepareConsole ()
{
#ifdef Q_OS_WIN
  SetConsoleOutputCP (CP_UTF8);
  SetConsoleCP (CP_UTF8);
  HANDLE const out = GetStdHandle (STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (!GetConsoleMode (out, &mode))
    {
      return false;
    }
  return SetConsoleMode (out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
  return true;
#endif
}

void writeOut (QString const& text)
{
  QByteArray const utf8 = text.toUtf8 ();
  std::fwrite (utf8.constData (), 1, static_cast<size_t> (utf8.size ()), stdout);
  std::fflush (stdout);
}

struct Style
{
  bool enabled {false};
  QString operator() (QString const& text, char const* code) const
  {
    return enabled ? QStringLiteral ("\033[%1m%2\033[0m").arg (QLatin1String (code), text) : text;
  }
};

// ── cattura audio ──────────────────────────────────────────────────────────

// Legge il flusso della scheda audio e fa corrispondere ogni indice di
// campione a un istante dell'orologio. Ogni lettura stima quando e' stato
// preso il campione 0: arrivo meno durata del flusso finora. Latenze di buffer
// e di scheduling possono solo far arrivare tardi, quindi si prende il minimo
// delle ultime ~200 stime: la finestra segue la deriva del clock della scheda.
class Capture final : public QObject
{
public:
  bool open (QAudioDevice const& device, QString* error)
  {
    QAudioFormat format;
    format.setSampleFormat (QAudioFormat::Int16);
    // Prima scelta: 48 kHz mono, come la ricezione di Decodium. Poi stereo
    // (si tiene il canale sinistro) e infine 12 kHz diretti.
    struct Candidate { int rate; int channels; };
    for (Candidate const c : {Candidate {48000, 1}, Candidate {48000, 2}, Candidate {12000, 1}, Candidate {12000, 2}})
      {
        format.setSampleRate (c.rate);
        format.setChannelCount (c.channels);
        if (device.isFormatSupported (format))
          {
            factor_ = c.rate / kRate;
            channels_ = c.channels;
            source_ = new QAudioSource (device, format, this);
            source_->setBufferSize (c.rate * c.channels * 2 / 5);   // ~200 ms
            io_ = source_->start ();
            if (!io_)
              {
                *error = QStringLiteral ("the audio input could not be opened");
                return false;
              }
            connect (io_, &QIODevice::readyRead, this, [this] { read (); });
            label_ = QStringLiteral ("%1 (%2 Hz, %3 ch)").arg (device.description ()).arg (c.rate).arg (c.channels);
            return true;
          }
      }
    *error = QStringLiteral ("the input supports neither 48000 nor 12000 Hz, 16 bit");
    return false;
  }

  QString label () const { return label_; }
  double level () const { return level_; }
  bool failed () const { return source_ && source_->error () != QAudio::NoError; }

  void stop ()
  {
    if (source_)
      {
        source_->stop ();
      }
  }

  // Campioni dello slot che comincia a startTime, o vuoto se non ancora tutti.
  QVector<short> slot (double startTime, int count) const
  {
    if (!haveT0_)
      {
        return {};
      }
    long long const i0 = std::llround ((startTime - t0_) * kRate);
    if (i0 < offset_ || i0 + count > offset_ + static_cast<long long> (buf_.size ()))
      {
        return {};
      }
    auto const first = buf_.begin () + static_cast<std::ptrdiff_t> (i0 - offset_);
    return QVector<short> (first, first + count);
  }

private:
  void read ()
  {
    QByteArray const bytes = io_->readAll ();
    if (bytes.isEmpty ())
      {
        return;
      }
    double const now = QDateTime::currentMSecsSinceEpoch () / 1000.0;
    int const frames = bytes.size () / (2 * channels_);
    auto const* raw = reinterpret_cast<qint16 const*> (bytes.constData ());
    for (int i = 0; i < frames; ++i)
      {
        pending_.push_back (qFromLittleEndian<qint16> (raw + i * channels_));
      }

    std::vector<short> out;
    if (factor_ == 1)
      {
        out.swap (pending_);
      }
    else
      {
        int const usable = static_cast<int> (pending_.size ()) / factor_ * factor_;
        if (usable > 0)
          {
            out.resize (static_cast<size_t> (usable / factor_));
            int32_t n2 = 0;
            fil4_cpp (reinterpret_cast<int16_t const*> (pending_.data ()), usable,
                      reinterpret_cast<int16_t*> (out.data ()), n2);
            out.resize (static_cast<size_t> (n2));
            pending_.erase (pending_.begin (), pending_.begin () + usable);
          }
      }
    if (out.empty ())
      {
        return;
      }

    buf_.insert (buf_.end (), out.begin (), out.end ());
    total_ += static_cast<long long> (out.size ());
    estimates_.push_back (now - static_cast<double> (total_) / kRate);
    if (estimates_.size () > 200)
      {
        estimates_.pop_front ();
      }
    t0_ = *std::min_element (estimates_.begin (), estimates_.end ());
    haveT0_ = true;

    constexpr size_t kKeep = 90 * kRate;
    if (buf_.size () > kKeep + 10 * kRate)
      {
        size_t const drop = buf_.size () - kKeep;
        buf_.erase (buf_.begin (), buf_.begin () + static_cast<std::ptrdiff_t> (drop));
        offset_ += static_cast<long long> (drop);
      }

    double power = 0.0;
    int n = 0;
    for (size_t i = 0; i < out.size (); i += 8, ++n)
      {
        power += static_cast<double> (out[i]) * out[i];
      }
    power /= std::max (1, n);
    level_ = power > 0.0 ? 10.0 * std::log10 (power / (32768.0 * 32768.0)) : -120.0;
  }

  QAudioSource* source_ {nullptr};
  QIODevice* io_ {nullptr};
  int factor_ {4};
  int channels_ {1};
  QString label_;
  std::vector<short> pending_;
  std::vector<short> buf_;
  long long offset_ {0};
  long long total_ {0};
  std::deque<double> estimates_;
  double t0_ {0.0};
  bool haveT0_ {false};
  double level_ {-120.0};
};

// ── decodifica ─────────────────────────────────────────────────────────────

struct Options
{
  ModeInfo const* mode {nullptr};
  double dial {0.0};
  QString call;
  int depth {3};
  int low {200};
  int high {4000};
  int rxFreq {1500};
  bool cqOnly {false};
  QRegularExpression grep;
  QString logPath;
  QString saveSlots;
  bool color {true};
};

struct Row
{
  int snr {0};
  double dt {0.0};
  int df {0};
  QString msg;
};

QByteArray fortranField (QString const& text, int width)
{
  QByteArray value = text.toLatin1 ().left (width);
  value.append (QByteArray (width - value.size (), ' '));
  return value;
}

// Il core lascia vuota la colonna UTC per lo slot 00:00:00.
QRegularExpression const kRow {
    QStringLiteral (R"(^\s*(?:(\d{4,6})\s+)?(-?\d+)\s+(-?\d+\.\d+)\s+(\d+)\s+\S\s+(.*?)\s*$)")};

std::vector<Row> parseRows (QStringList const& lines)
{
  std::vector<Row> rows;
  for (QString const& line : lines)
    {
      auto const m = kRow.match (line);
      if (m.hasMatch ())
        {
          rows.push_back ({m.captured (2).toInt (), m.captured (3).toDouble (), m.captured (4).toInt (),
                           m.captured (5)});
        }
    }
  return rows;
}

// I worker vivono nel thread che li usa: ne esiste uno solo, quello del modo.
class Engine
{
public:
  explicit Engine (Options const& options) : options_ (options) {}

  std::vector<Row> decode (QVector<short> audio, double slotTime)
  {
    QDateTime const stamp = QDateTime::fromMSecsSinceEpoch (static_cast<qint64> (slotTime * 1000.0), QTimeZone::UTC);
    int const nutc = stamp.time ().hour () * 10000 + stamp.time ().minute () * 100 + stamp.time ().second ();
    QByteArray const mycall = fortranField (options_.call, 12);
    QByteArray const hiscall = fortranField (QString {}, 12);
    int const threads = std::max (1, std::min (8, static_cast<int> (std::thread::hardware_concurrency ()) / 2));
    ++serial_;
    QStringList rows;
    QString const mode = QLatin1String (options_.mode->name);
    if (mode == QLatin1String ("ft8"))
      {
        if (!ft8_) ft8_.reset (new decodium::ft8::FT8DecodeWorker);
        decodium::ft8::DecodeRequest r;
        r.serial = serial_;
        r.audio = std::move (audio);
        r.nfqso = options_.rxFreq;
        r.nftx = options_.rxFreq;
        r.nutc = nutc;
        r.nfa = options_.low;
        r.nfb = std::max (options_.low + 50, options_.high);
        r.nzhsym = 50;
        r.ndepth = std::clamp (options_.depth, 1, 4);
        r.threadCount = threads;
        r.lft8apon = 1;
        r.apMyCallEnabled = !options_.call.isEmpty ();
        r.napwid = 75;
        r.ldiskdat = 1;   // lo slot e' completo, come un WAV letto da disco
        r.mycall = mycall;
        r.hiscall = hiscall;
        r.hisgrid = QByteArray (6, ' ');
        rows = run (*ft8_, &decodium::ft8::FT8DecodeWorker::decodeReady, r);
      }
    else if (mode == QLatin1String ("ft4"))
      {
        if (!ft4_) ft4_.reset (new decodium::ft4::FT4DecodeWorker);
        decodium::ft4::DecodeRequest r;
        r.serial = serial_;
        r.audio = std::move (audio);
        r.nutc = nutc;
        r.nfqso = options_.rxFreq;
        r.nfa = options_.low;
        r.nfb = std::max (options_.low + 50, options_.high);
        r.ndepth = std::clamp (options_.depth, 1, 4);
        r.threadCount = threads;
        r.mycall = mycall;
        r.hiscall = hiscall;
        rows = run (*ft4_, &decodium::ft4::FT4DecodeWorker::decodeReady, r);
      }
    else
      {
        if (!ft2_) ft2_.reset (new decodium::ft2::FT2DecodeWorker);
        decodium::ft2::DecodeRequest r;
        r.serial = serial_;
        r.audio = std::move (audio);
        r.nutc = nutc;
        r.nfqso = options_.rxFreq;
        r.nfa = options_.low;
        r.nfb = std::max (options_.low + 50, options_.high);
        r.ndepth = std::max (1, options_.depth);
        r.threadCount = threads;
        r.mycall = mycall;
        r.hiscall = hiscall;
        ft2_->markLatestDecodeSerial (r.serial);
        rows = run (*ft2_, &decodium::ft2::FT2DecodeWorker::decodeReady, r);
      }
    return parseRows (rows);
  }

private:
  template <typename Worker, typename Signal, typename Request>
  QStringList run (Worker& worker, Signal signal, Request const& request)
  {
    QStringList rows;
    quint64 const serial = request.serial;
    auto const connection = QObject::connect (&worker, signal, &worker,
                                              [&rows, serial] (quint64 ready, QStringList const& lines) {
                                                if (ready == serial) rows = lines;
                                              });
    worker.decode (request);
    QObject::disconnect (connection);
    return rows;
  }

  Options const& options_;
  quint64 serial_ {0};
  std::unique_ptr<decodium::ft8::FT8DecodeWorker> ft8_;
  std::unique_ptr<decodium::ft4::FT4DecodeWorker> ft4_;
  std::unique_ptr<decodium::ft2::FT2DecodeWorker> ft2_;
};

struct Job
{
  double slotTime {0.0};
  QVector<short> samples;
};

struct Result
{
  double slotTime {0.0};
  std::vector<Row> rows;
  int ms {0};
};

// Thread di decodifica: una coda di al massimo due slot, mai piu' indietro.
class Decoder
{
public:
  explicit Decoder (Options const& options) : options_ (options), thread_ ([this] { loop (); }) {}

  ~Decoder ()
  {
    {
      std::lock_guard<std::mutex> lock (mutex_);
      quit_ = true;
    }
    cv_.notify_all ();
    thread_.join ();
  }

  int submit (Job job)
  {
    int dropped = 0;
    {
      std::lock_guard<std::mutex> lock (mutex_);
      while (jobs_.size () >= 2)
        {
          jobs_.pop_front ();
          ++dropped;
        }
      jobs_.push_back (std::move (job));
    }
    cv_.notify_one ();
    return dropped;
  }

  std::optional<Result> takeResult ()
  {
    std::lock_guard<std::mutex> lock (mutex_);
    if (results_.empty ())
      {
        return std::nullopt;
      }
    Result r = std::move (results_.front ());
    results_.pop_front ();
    return r;
  }

  bool busy () const { return busy_; }

private:
  void loop ()
  {
    Engine engine {options_};
    for (;;)
      {
        Job job;
        {
          std::unique_lock<std::mutex> lock (mutex_);
          cv_.wait (lock, [this] { return quit_ || !jobs_.empty (); });
          if (quit_)
            {
              return;
            }
          job = std::move (jobs_.front ());
          jobs_.pop_front ();
        }
        busy_ = true;
        QElapsedTimer timer;
        timer.start ();
        if (!options_.saveSlots.isEmpty ())
          {
            saveWav (job);
          }
        std::vector<Row> rows = engine.decode (job.samples, job.slotTime);
        Result result {job.slotTime, std::move (rows), static_cast<int> (timer.elapsed ())};
        {
          std::lock_guard<std::mutex> lock (mutex_);
          results_.push_back (std::move (result));
        }
        busy_ = false;
      }
  }

  void saveWav (Job const& job) const
  {
    QDir ().mkpath (options_.saveSlots);
    QDateTime const stamp = QDateTime::fromMSecsSinceEpoch (static_cast<qint64> (job.slotTime * 1000.0), QTimeZone::UTC);
    QFile file {QDir {options_.saveSlots}.filePath (stamp.toString (QStringLiteral ("yyMMdd_HHmmss")) + QStringLiteral (".wav"))};
    if (!file.open (QIODevice::WriteOnly))
      {
        return;
      }
    quint32 const dataBytes = static_cast<quint32> (job.samples.size ()) * 2u;
    auto le32 = [&file] (quint32 v) { char b[4]; qToLittleEndian (v, b); file.write (b, 4); };
    auto le16 = [&file] (quint16 v) { char b[2]; qToLittleEndian (v, b); file.write (b, 2); };
    file.write ("RIFF", 4); le32 (36 + dataBytes); file.write ("WAVE", 4);
    file.write ("fmt ", 4); le32 (16); le16 (1); le16 (1); le32 (kRate); le32 (kRate * 2); le16 (2); le16 (16);
    file.write ("data", 4); le32 (dataBytes);
    for (short s : job.samples) le16 (static_cast<quint16> (s));
  }

  Options const& options_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<Job> jobs_;
  std::deque<Result> results_;
  std::atomic<bool> busy_ {false};
  bool quit_ {false};
  std::thread thread_;
};

// ── presentazione ──────────────────────────────────────────────────────────

class Screen
{
public:
  Screen (Options const& options, Style style) : options_ (options), style_ (style)
  {
    if (!options_.logPath.isEmpty ())
      {
        QDir ().mkpath (QFileInfo {options_.logPath}.absolutePath ());
        log_.setFileName (options_.logPath);
        if (!log_.open (QIODevice::Append | QIODevice::Text))
          {
            writeOut (QStringLiteral ("decodium-rx: cannot write the decode log %1\n")
                          .arg (QDir::toNativeSeparators (options_.logPath)));
          }
      }
  }

  void message (QString const& text)
  {
    clearStatus ();
    writeOut (text + QLatin1Char ('\n'));
  }

  void status (QString const& text)
  {
    if (!stdoutIsTerminal ())
      {
        return;
      }
    int const width = std::max (10, terminalWidth () - 1);
    writeOut (QStringLiteral ("\r\033[K") + style_ (text.left (width), "2;36"));
    statusShown_ = true;
  }

  void clearStatus ()
  {
    if (statusShown_)
      {
        writeOut (QStringLiteral ("\r\033[K"));
        statusShown_ = false;
      }
  }

  void header (QString const& source)
  {
    QString const mode = QString::fromLatin1 (options_.mode->name).toUpper ();
    QString const dial = options_.dial > 0.0 ? QStringLiteral ("%1 MHz").arg (options_.dial, 0, 'f', 3)
                                             : QStringLiteral ("dial not set");
    message (style_ (QStringLiteral ("Decodium RX · %1 · %2 · %3").arg (mode, dial, source), "1;96"));
    message (style_ (QStringLiteral ("  UTC        dB   DT   Freq   Message"), "2"));
  }

  void decodes (double slotTime, std::vector<Row> const& rows)
  {
    QString const mode = QString::fromLatin1 (options_.mode->name).toUpper ();
    QDateTime const stamp = QDateTime::fromMSecsSinceEpoch (static_cast<qint64> (slotTime * 1000.0), QTimeZone::UTC);
    QString clock = stamp.toString (QStringLiteral ("HH:mm:ss"));
    if (QLatin1String (options_.mode->name) == QLatin1String ("ft2"))
      {
        int const hundredths = static_cast<int> (std::lround (std::fmod (slotTime, 1.0) * 100.0)) % 100;
        clock += QStringLiteral (".%1").arg (hundredths, 2, 10, QLatin1Char ('0'));
      }
    else
      {
        clock += QStringLiteral ("   ");
      }
    QString const call = options_.call.toUpper ();
    for (Row const& row : rows)
      {
        if (log_.isOpen ())
          {
            QString const line = QStringLiteral ("%1 %2 Rx %3 %4 %5 %6 %7\n")
                                     .arg (stamp.toString (QStringLiteral ("yyMMdd_HHmmss")))
                                     .arg (options_.dial, 10, 'f', 3)
                                     .arg (mode, -4)
                                     .arg (row.snr, 6)
                                     .arg (row.dt, 4, 'f', 1)
                                     .arg (row.df, 4)
                                     .arg (row.msg);
            log_.write (line.toUtf8 ());
          }
        if (options_.cqOnly && !row.msg.startsWith (QStringLiteral ("CQ ")))
          {
            continue;
          }
        if (options_.grep.pattern ().size () && !options_.grep.match (row.msg).hasMatch ())
          {
            continue;
          }
        QString const freq = options_.dial > 0.0
                                 ? QStringLiteral ("%1").arg (options_.dial + row.df / 1e6, 10, 'f', 6)
                                 : QStringLiteral ("%1").arg (row.df, 5);
        QString line = QStringLiteral ("  %1 %2 %3 %4  %5")
                           .arg (clock)
                           .arg (row.snr, 4)
                           .arg (row.dt, 4, 'f', 1)
                           .arg (freq, row.msg);
        QStringList const words = row.msg.split (QLatin1Char (' '), Qt::SkipEmptyParts);
        if (!call.isEmpty () && words.contains (call))
          {
            line = style_ (line, "1;91");
          }
        else if (row.msg.startsWith (QStringLiteral ("CQ ")))
          {
            line = style_ (line, "92");
          }
        message (line);
      }
    if (log_.isOpen ())
      {
        log_.flush ();
      }
  }

private:
  Options const& options_;
  Style style_;
  QFile log_;
  bool statusShown_ {false};
};

// ── configurazione ─────────────────────────────────────────────────────────

QString configPath ()
{
  return QDir {QStandardPaths::writableLocation (QStandardPaths::AppConfigLocation)}.filePath (
      QStringLiteral ("decodium-rx.json"));
}

QJsonObject loadConfig ()
{
  QFile file {configPath ()};
  if (!file.open (QIODevice::ReadOnly))
    {
      return {};
    }
  return QJsonDocument::fromJson (file.readAll ()).object ();
}

QString ask (QString const& prompt, QString const& fallback)
{
  writeOut (QStringLiteral ("%1 [%2]: ").arg (prompt, fallback));
  std::string line;
  if (!std::getline (std::cin, line))
    {
      return fallback;
    }
  QString const answer = QString::fromLocal8Bit (line.c_str ()).trimmed ();
  return answer.isEmpty () ? fallback : answer;
}

void listDevices ()
{
  QAudioDevice const def = QMediaDevices::defaultAudioInput ();
  writeOut (QStringLiteral ("Audio inputs (use -d NUMBER or part of the name):\n"));
  int i = 0;
  for (QAudioDevice const& d : QMediaDevices::audioInputs ())
    {
      writeOut (QStringLiteral ("  %1  %2%3\n").arg (++i, 2).arg (d.description (),
                                                             d.id () == def.id () ? QStringLiteral ("  (default)") : QString {}));
    }
  if (i == 0)
    {
      writeOut (QStringLiteral ("  no audio input found\n"));
    }
}

std::optional<QAudioDevice> pickDevice (QString const& wanted)
{
  QList<QAudioDevice> const inputs = QMediaDevices::audioInputs ();
  if (wanted.isEmpty () || wanted == QStringLiteral ("default"))
    {
      QAudioDevice const def = QMediaDevices::defaultAudioInput ();
      if (def.isNull ())
        {
          return std::nullopt;
        }
      return def;
    }
  bool numeric = false;
  int const index = wanted.toInt (&numeric);
  if (numeric)
    {
      if (index >= 1 && index <= inputs.size ())
        {
          return inputs.at (index - 1);
        }
      return std::nullopt;
    }
  for (QAudioDevice const& d : inputs)
    {
      if (d.description ().contains (wanted, Qt::CaseInsensitive))
        {
          return d;
        }
    }
  return std::nullopt;
}

// Domande del primo avvio quando manca il modo (lanciato dal menu Start).
void interactive (Options& options, QString& device)
{
  QJsonObject const saved = loadConfig ();
  writeOut (QStringLiteral ("Decodium RX: digital-mode receiver (RX only). Enter keeps the value in brackets.\n\n"));
  ModeInfo const* mode = nullptr;
  QString modeText = saved.value (QStringLiteral ("mode")).toString (QStringLiteral ("ft8"));
  while (!(mode = findMode (modeText = ask (QStringLiteral ("Mode ft8 / ft4 / ft2"), modeText))))
    {
      modeText = QStringLiteral ("ft8");
    }
  QString const dial = ask (QStringLiteral ("Dial frequency in MHz (0 = none)"),
                            QString::number (saved.value (QStringLiteral ("dial")).toDouble (0.0)));
  QString const call = ask (QStringLiteral ("Your callsign, highlighted and used for AP decoding (- = none)"),
                            saved.value (QStringLiteral ("call")).toString (QStringLiteral ("-")));
  writeOut (QStringLiteral ("\n"));
  listDevices ();
  device = ask (QStringLiteral ("\nAudio input (default = system default)"),
                saved.value (QStringLiteral ("device")).toString (QStringLiteral ("default")));
  // Si ricorda il nome, non il numero: l'ordine degli ingressi cambia quando
  // si collega o scollega un'interfaccia USB.
  bool numeric = false;
  device.toInt (&numeric);
  if (numeric)
    {
      if (auto const chosen = pickDevice (device))
        {
          device = chosen->description ();
        }
    }
  options.mode = mode;
  options.dial = QString {dial}.replace (QLatin1Char (','), QLatin1Char ('.')).toDouble ();
  options.call = (call == QStringLiteral ("-") || call.isEmpty ()) ? QString {} : call.toUpper ();
  QDir ().mkpath (QFileInfo {configPath ()}.absolutePath ());
  QFile file {configPath ()};
  if (file.open (QIODevice::WriteOnly | QIODevice::Truncate))
    {
      QJsonObject obj;
      obj.insert (QStringLiteral ("mode"), QString::fromLatin1 (mode->name));
      obj.insert (QStringLiteral ("dial"), options.dial);
      obj.insert (QStringLiteral ("call"), options.call.isEmpty () ? QStringLiteral ("-") : options.call);
      obj.insert (QStringLiteral ("device"), device);
      file.write (QJsonDocument {obj}.toJson (QJsonDocument::Compact));
    }
  writeOut (QStringLiteral ("\n"));
}

// WAV PCM 16 bit mono a 12 kHz, come quelli di --save-slots e di Decodium.
QVector<short> readWav (QString const& path, QString* error)
{
  QFile file {path};
  if (!file.open (QIODevice::ReadOnly))
    {
      *error = file.errorString ();
      return {};
    }
  QByteArray const blob = file.readAll ();
  if (blob.size () < 44 || blob.mid (0, 4) != "RIFF" || blob.mid (8, 4) != "WAVE")
    {
      *error = QStringLiteral ("not a RIFF/WAVE file");
      return {};
    }
  int pos = 12;
  int channels = 0, rate = 0, bits = 0, format = 0;
  while (pos + 8 <= blob.size ())
    {
      QByteArray const id = blob.mid (pos, 4);
      quint32 const size = qFromLittleEndian<quint32> (blob.constData () + pos + 4);
      pos += 8;
      if (id == "fmt " && size >= 16)
        {
          format = qFromLittleEndian<quint16> (blob.constData () + pos);
          channels = qFromLittleEndian<quint16> (blob.constData () + pos + 2);
          rate = static_cast<int> (qFromLittleEndian<quint32> (blob.constData () + pos + 4));
          bits = qFromLittleEndian<quint16> (blob.constData () + pos + 14);
        }
      else if (id == "data")
        {
          if (format != 1 || channels != 1 || bits != 16 || rate != kRate)
            {
              *error = QStringLiteral ("must be PCM 16-bit mono at 12000 Hz");
              return {};
            }
          int const count = static_cast<int> (std::min<qint64> (size, blob.size () - pos)) / 2;
          QVector<short> samples (count);
          for (int i = 0; i < count; ++i)
            {
              samples[i] = qFromLittleEndian<qint16> (blob.constData () + pos + 2 * i);
            }
          return samples;
        }
      pos += static_cast<int> ((size + 1u) & ~1u);
    }
  *error = QStringLiteral ("no data chunk");
  return {};
}

}  // namespace

int main (int argc, char* argv[])
{
  QCoreApplication app {argc, argv};
  QCoreApplication::setOrganizationName (QStringLiteral ("Decodium"));
  QCoreApplication::setApplicationName (QStringLiteral ("decodium-rx"));

  QCommandLineParser parser;
  parser.setApplicationDescription (QStringLiteral (
      "Decodium RX: FT8, FT4 and FT2 receiver for the terminal (RX only).\n"
      "Captures audio from the radio's sound card, cuts it into UTC-aligned slots and decodes\n"
      "every slot with the Decodium 4 decoders. Nothing is ever transmitted.\n\n"
      "  decodium-rx                         ask mode, dial frequency and audio input\n"
      "  decodium-rx -m ft8 -f 14.074 -c IU8LMC\n"
      "  decodium-rx -m ft2 -d 2             audio input number 2 of --list-devices\n"
      "  decodium-rx -m ft8 --wav 250913_120000.wav     decode recordings and exit"));
  parser.addHelpOption ();
  QCommandLineOption const modeOpt ({QStringLiteral ("m"), QStringLiteral ("mode")}, QStringLiteral ("Digital mode: ft8, ft4 or ft2 (asked if omitted)."), QStringLiteral ("mode"));
  QCommandLineOption const dialOpt ({QStringLiteral ("f"), QStringLiteral ("dial")}, QStringLiteral ("Dial frequency in MHz, shown and logged."), QStringLiteral ("MHz"));
  QCommandLineOption const callOpt ({QStringLiteral ("c"), QStringLiteral ("call")}, QStringLiteral ("Your callsign: highlighted, used for AP decoding."), QStringLiteral ("call"));
  QCommandLineOption const devOpt ({QStringLiteral ("d"), QStringLiteral ("device")}, QStringLiteral ("Audio input: number or part of the name, see --list-devices."), QStringLiteral ("device"));
  QCommandLineOption const listOpt (QStringLiteral ("list-devices"), QStringLiteral ("List audio inputs and exit."));
  QCommandLineOption const depthOpt (QStringLiteral ("depth"), QStringLiteral ("Decode depth 1-3 (default 3)."), QStringLiteral ("n"), QStringLiteral ("3"));
  QCommandLineOption const lowOpt (QStringLiteral ("low"), QStringLiteral ("Lowest audio frequency, Hz (default 200)."), QStringLiteral ("Hz"), QStringLiteral ("200"));
  QCommandLineOption const highOpt (QStringLiteral ("high"), QStringLiteral ("Highest audio frequency, Hz (default 4000)."), QStringLiteral ("Hz"), QStringLiteral ("4000"));
  QCommandLineOption const rxOpt (QStringLiteral ("rx-freq"), QStringLiteral ("Audio frequency of interest, Hz (default 1500)."), QStringLiteral ("Hz"), QStringLiteral ("1500"));
  QCommandLineOption const cqOpt (QStringLiteral ("cq-only"), QStringLiteral ("Show only CQ calls."));
  QCommandLineOption const grepOpt (QStringLiteral ("grep"), QStringLiteral ("Show only messages matching REGEX."), QStringLiteral ("REGEX"));
  QString const defaultLog = QDir {QStandardPaths::writableLocation (QStandardPaths::AppDataLocation)}.filePath (QStringLiteral ("ALL.TXT"));
  QCommandLineOption const logOpt (QStringLiteral ("log"), QStringLiteral ("Decode log in WSJT-X ALL.TXT format (default: %1).").arg (QDir::toNativeSeparators (defaultLog)), QStringLiteral ("file"), defaultLog);
  QCommandLineOption const noLogOpt (QStringLiteral ("no-log"), QStringLiteral ("Do not write the decode log."));
  QCommandLineOption const saveOpt (QStringLiteral ("save-slots"), QStringLiteral ("Keep every slot as a WAV file in DIR."), QStringLiteral ("DIR"));
  QCommandLineOption const noColorOpt (QStringLiteral ("no-color"), QStringLiteral ("Plain output."));
  QCommandLineOption const wavOpt (QStringLiteral ("wav"), QStringLiteral ("Decode 12 kHz 16-bit mono WAV files (names end in _HHMMSS.wav) given as arguments, then exit."));
  QCommandLineOption const verboseOpt (QStringLiteral ("verbose"), QStringLiteral ("Show the decoders' diagnostic messages instead of writing them to decodium-rx-debug.log."));
  for (auto const* o : {&modeOpt, &dialOpt, &callOpt, &devOpt, &listOpt, &depthOpt, &lowOpt, &highOpt, &rxOpt,
                        &cqOpt, &grepOpt, &logOpt, &noLogOpt, &saveOpt, &noColorOpt, &wavOpt, &verboseOpt})
    {
      parser.addOption (*o);
    }
  parser.addPositionalArgument (QStringLiteral ("files"), QStringLiteral ("WAV files, with --wav."), QStringLiteral ("[files...]"));
  parser.process (app);

  bool const terminal = stdoutIsTerminal ();
  bool const vt = prepareConsole ();
  Style const style {terminal && vt && !parser.isSet (noColorOpt) && qEnvironmentVariableIsEmpty ("NO_COLOR")};

  if (parser.isSet (listOpt))
    {
      listDevices ();
      return 0;
    }

  Options options;
  options.dial = parser.value (dialOpt).replace (QLatin1Char (','), QLatin1Char ('.')).toDouble ();
  options.call = parser.value (callOpt).trimmed ().toUpper ();
  options.depth = std::clamp (parser.value (depthOpt).toInt (), 1, 3);
  options.low = parser.value (lowOpt).toInt ();
  options.high = parser.value (highOpt).toInt ();
  options.rxFreq = parser.value (rxOpt).toInt ();
  options.cqOnly = parser.isSet (cqOpt);
  if (parser.isSet (grepOpt))
    {
      options.grep = QRegularExpression {parser.value (grepOpt), QRegularExpression::CaseInsensitiveOption};
    }
  options.logPath = parser.isSet (noLogOpt) ? QString {} : parser.value (logOpt);
  options.saveSlots = parser.value (saveOpt);
  QString device = parser.value (devOpt);

  if (parser.isSet (modeOpt))
    {
      options.mode = findMode (parser.value (modeOpt));
      if (!options.mode)
        {
          std::fprintf (stderr, "decodium-rx: unknown mode \"%s\" (ft8, ft4, ft2)\n", qPrintable (parser.value (modeOpt)));
          return 2;
        }
    }
  else
    {
      if (parser.isSet (wavOpt))
        {
          std::fprintf (stderr, "decodium-rx: --mode is required with --wav\n");
          return 2;
        }
      interactive (options, device);
    }

  Screen screen {options, style};

  // I decoder scrivono la loro diagnostica (Boost.Log, fastldpc) su stderr:
  // in un terminale coprirebbe le righe decodificate. Va in un file accanto
  // all'ALL.TXT, salvo --verbose. Gli errori di questo programma vanno su stdout.
  if (!parser.isSet (verboseOpt))
    {
      // Boost.Log scrive sulla console per conto suo: si spegne del tutto.
      boost::log::core::get ()->set_logging_enabled (false);
      QString const dir = QStandardPaths::writableLocation (QStandardPaths::AppDataLocation);
      QDir ().mkpath (dir);
      QString const debugLog = QDir {dir}.filePath (QStringLiteral ("decodium-rx-debug.log"));
      if (!std::freopen (QFile::encodeName (debugLog).constData (), "a", stderr))
        {
          screen.message (style (QStringLiteral ("  ! cannot redirect diagnostics to %1").arg (debugLog), "93"));
        }
    }

  if (parser.isSet (wavOpt))
    {
      Engine engine {options};
      for (QString const& path : parser.positionalArguments ())
        {
          double slotTime = 0.0;
          auto const m = QRegularExpression {QStringLiteral (R"((\d{6})_(\d{6})\.wav$)"),
                                             QRegularExpression::CaseInsensitiveOption}.match (QFileInfo {path}.fileName ());
          if (m.hasMatch ())
            {
              QDateTime const when = QDateTime::fromString (m.captured (1) + m.captured (2), QStringLiteral ("yyMMddHHmmss"));
              if (when.isValid ())
                {
                  QDateTime utc = when;
                  utc.setTimeZone (QTimeZone::UTC);
                  slotTime = utc.addYears (when.date ().year () < 2000 ? 100 : 0).toMSecsSinceEpoch () / 1000.0;
                }
            }
          QString error;
          QVector<short> samples = readWav (path, &error);
          if (samples.isEmpty ())
            {
              screen.message (style (QStringLiteral ("  %1: %2").arg (path, error), "93"));
              continue;
            }
          samples.resize (options.mode->samples);   // completa con silenzio o tronca, come jt9
          screen.decodes (slotTime, engine.decode (samples, slotTime));
        }
      return 0;
    }

  std::optional<QAudioDevice> const input = pickDevice (device);
  if (!input)
    {
      screen.message (style (QStringLiteral ("decodium-rx: audio input \"%1\" not found, see --list-devices")
                                 .arg (device.isEmpty () ? QStringLiteral ("default") : device), "91"));
      return 1;
    }
  Capture capture;
  QString error;
  if (!capture.open (*input, &error))
    {
      screen.message (style (QStringLiteral ("decodium-rx: %1").arg (error), "91"));
      return 1;
    }

#ifdef Q_OS_WIN
  SetConsoleCtrlHandler (consoleHandler, TRUE);
#else
  std::signal (SIGINT, signalHandler);
  std::signal (SIGTERM, signalHandler);
#endif

  screen.header (capture.label ());
  Decoder decoder {options};
  double const period = options.mode->period;
  int const count = options.mode->samples;
  double nextSlot = std::ceil (QDateTime::currentMSecsSinceEpoch () / 1000.0 / period) * period;
  int lastRows = 0;
  int lastMs = 0;
  QString const modeUpper = QString::fromLatin1 (options.mode->name).toUpper ();

  QTimer tick;
  QObject::connect (&tick, &QTimer::timeout, &app, [&] {
    if (g_stop)
      {
        app.quit ();
        return;
      }
    double const now = QDateTime::currentMSecsSinceEpoch () / 1000.0;
    if (now >= nextSlot + count / static_cast<double> (kRate) + 0.1)
      {
        QVector<short> samples = capture.slot (nextSlot, count);
        if (!samples.isEmpty ())
          {
            int const dropped = decoder.submit ({nextSlot, std::move (samples)});
            if (dropped)
              {
                screen.message (style (QStringLiteral ("  ! decoder too slow, skipped %1 slot(s)").arg (dropped), "93"));
              }
          }
        nextSlot += period;
        if (nextSlot < now - period)   // fermo (sospensione, flusso interrotto)
          {
            nextSlot = std::ceil (now / period) * period;
          }
      }
    while (auto result = decoder.takeResult ())
      {
        screen.decodes (result->slotTime, result->rows);
        lastRows = static_cast<int> (result->rows.size ());
        lastMs = result->ms;
      }
    if (capture.failed ())
      {
        screen.message (style (QStringLiteral ("  ! audio input stopped"), "91"));
        app.quit ();
        return;
      }
    double const level = capture.level ();
    int const bars = std::clamp (static_cast<int> ((level + 60.0) / 5.0), 0, 10);
    double const wait = std::max (0.0, nextSlot + count / static_cast<double> (kRate) + 0.1 - now);
    screen.status (QStringLiteral ("  %1  input %2 dBFS %3%4  next decode %5s  last %6 decodes in %7 ms%8  Ctrl+C quits")
                       .arg (modeUpper)
                       .arg (level, 5, 'f', 1)
                       .arg (QString (bars, QChar (0x25AE)), QString (10 - bars, QChar (0x25AF)))
                       .arg (wait, 4, 'f', 1)
                       .arg (lastRows)
                       .arg (lastMs)
                       .arg (decoder.busy () ? QStringLiteral ("  decoding…") : QString {}));
  });
  tick.start (100);
  int const rc = app.exec ();
  capture.stop ();
  screen.clearStatus ();
  return rc;
}
