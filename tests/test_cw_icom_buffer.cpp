#include "CwCatText.h"
#include "CwController.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <hamlib/rig.h>

#include <chrono>
#include <deque>
#include <mutex>

using namespace decodium::cw;

namespace {

// Real Hamlib FIFO, dummy backend only.  The model rejects commands when a
// finite, 60-character keyer buffer is full, draining at 20 WPM.  This is a
// simulated failure condition, not a claim about every Icom's buffer size.
class BufferedKeyer {
public:
    static constexpr int wpm = 20;

    BufferedKeyer()
    {
        rig_set_debug(RIG_DEBUG_NONE);
        m_rig = rig_init(RIG_MODEL_DUMMY);
        if (!m_rig)
            return;
        m_caps = *m_rig->caps;
        m_caps.send_morse = &send;
        m_caps.stop_morse = &stop;
        m_caps.morse_qsize = 0; // Hamlib default, as used by the IC-7100.
        m_rig->caps = &m_caps;
        s_instance = this;
        m_open = rig_open(m_rig) == RIG_OK;
    }

    ~BufferedKeyer()
    {
        if (m_open) {
            rig_stop_morse(m_rig, RIG_VFO_CURR);
            rig_close(m_rig); // Join Hamlib's worker before destroying state.
        }
        if (m_rig)
            rig_cleanup(m_rig);
        s_instance = nullptr;
    }

    bool ready() const { return m_open; }
    RIG* rig() const { return m_rig; }

    QString accepted()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_accepted;
    }

    int rejects()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_rejects;
    }

private:
    using Clock = std::chrono::steady_clock;
    inline static BufferedKeyer* s_instance = nullptr;
    rig_caps m_caps {};
    RIG* m_rig = nullptr;
    bool m_open = false;
    std::mutex m_mutex;
    std::deque<Clock::time_point> m_pending;
    QString m_accepted;
    int m_rejects = 0;

    static int send(RIG*, vfo_t, const char* text)
    {
        auto& self = *s_instance;
        std::lock_guard<std::mutex> lock(self.m_mutex);
        const auto now = Clock::now();
        while (!self.m_pending.empty() && self.m_pending.front() <= now)
            self.m_pending.pop_front();
        const QString chunk = QString::fromLatin1(text);
        if (self.m_pending.size() + size_t(chunk.size()) > 60) {
            ++self.m_rejects;
            return -RIG_ERJCTED;
        }
        auto end = self.m_pending.empty() ? now : self.m_pending.back();
        for (QChar c : chunk) {
            // Each letter includes a 3-dot gap; a space extends it to 7.
            const int duration = c == QLatin1Char(' ') ? 4 * (1200 / wpm)
                : CwKeyer::millisFor(QString(c), wpm) + 3 * (1200 / wpm);
            end += std::chrono::milliseconds(duration);
            self.m_pending.push_back(end);
        }
        self.m_accepted += chunk;
        return RIG_OK;
    }

    static int stop(RIG*, vfo_t)
    {
        auto& self = *s_instance;
        std::lock_guard<std::mutex> lock(self.m_mutex);
        self.m_pending.clear();
        return RIG_OK;
    }
};

} // namespace

class TestCwIcomBuffer : public QObject {
    Q_OBJECT
private slots:
    void issue109KeepsTheWholeMessageThroughTheRealHamlibFifo()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QSettings settings(dir.filePath("cw.ini"), QSettings::IniFormat);
        BufferedKeyer keyer;
        QVERIFY(keyer.ready());
        QCOMPARE(rig_set_mode(keyer.rig(), RIG_VFO_CURR, RIG_MODE_CW, 500), RIG_OK);
        QTest::qWait(150); // Allow Hamlib's asynchronous FIFO to initialise.

        CwController controller;
        QStringList submissions;
        QVector<bool> ptt;
        CwController::Hooks hooks;
        hooks.rigKeyerAvailable = [] { return true; };
        hooks.rigMorseTextLimit = [] { return catMorseTextLimit("Icom", false); };
        hooks.rigCwRequiresExplicitPtt = [] { return false; };
        hooks.rigPtt = [&](bool on) { ptt.append(on); };
        hooks.sendRigMorse = [&](const QString& text, int wpm) {
            if (wpm != BufferedKeyer::wpm)
                return false;
            submissions.append(text);
            const QByteArray bytes = text.toLatin1();
            return rig_send_morse(keyer.rig(), RIG_VFO_CURR, bytes.constData()) == RIG_OK;
        };
        hooks.stopRigMorse = [&] { rig_stop_morse(keyer.rig(), RIG_VFO_CURR); };
        controller.setHooks(hooks);
        controller.start(&settings);
        controller.setWpm(BufferedKeyer::wpm);
        QSignalSpy messages(&controller, &CwController::message);
        const QString text = QStringLiteral(
            "GE NAME YOUSIF YOUSIF GTH BAGHDAD/IRAQ BAGHDAD/IRAQ RST RST 589 589");
        QCOMPARE(text.size(), 67);
        controller.sendText(text, {});
        QCOMPARE(submissions.size(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.sending(), 65000);
        QCOMPARE(submissions.size(), 3);
        QCOMPARE(submissions.at(0).size(), 30);
        QCOMPARE(submissions.at(1).size(), 30);
        QCOMPARE(submissions.at(2), QStringLiteral("589 589"));
        QCOMPARE(submissions.join(QString()), text);
        QTRY_COMPARE_WITH_TIMEOUT(keyer.accepted(), text, 1000);
        QCOMPARE(keyer.rejects(), 0);
        QVERIFY(messages.isEmpty());
        QVERIFY(ptt.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestCwIcomBuffer)
#include "test_cw_icom_buffer.moc"
