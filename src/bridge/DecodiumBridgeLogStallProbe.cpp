#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <algorithm>
#include <memory>
#include <utility>

void decodiumBridgeAudioLevelLog(const QString&);

// Sonda delle pause dell'interfaccia attorno alla messa a log. Segnalato
// (15/9/2026): in FT2 "un leggero blocco, uno stacco tra il 73 e la messa a
// log". La parte sincrona di logQsoNow costa ~11 ms (misurato); le pause da
// 120-200 ms viste da MAINWATCH arrivano DOPO, solo dopo un log e mai agli
// altri avvii di TX. Per 4 s un filtro d'eventi sull'applicazione annota
// l'evento che precede ogni vuoto lungo del ciclo degli eventi (oggetto,
// classe, tipo): con il timer da 20 ms che tiene il ciclo sveglio, un vuoto
// oltre i 60 ms e' lavoro dentro quell'evento, non attesa.
namespace {
class LogStallEventSpy final : public QObject
{
public:
    explicit LogStallEventSpy(QString call) : m_call(std::move(call)) { m_clock.start(); }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        qint64 const now = m_clock.elapsed();
        if (m_lastType != QEvent::None && now - m_lastStart > 60) {
            Culprit const c {now - m_lastStart, m_lastStart, m_lastDesc, m_lastType};
            m_culprits.append(c);
            std::sort(m_culprits.begin(), m_culprits.end(),
                      [](Culprit const& a, Culprit const& b) { return a.ms > b.ms; });
            if (m_culprits.size() > 4) {
                m_culprits.resize(4);
            }
        }
        m_lastStart = now;
        m_lastType = event ? event->type() : QEvent::None;
        m_lastDesc = describe(watched, event);
        return false;
    }

    QString report() const
    {
        QStringList parts;
        for (Culprit const& c : m_culprits) {
            parts << QStringLiteral("%1 ms a +%2 ms dopo [%3 tipo %4]")
                         .arg(c.ms).arg(c.at).arg(c.desc).arg(static_cast<int>(c.type));
        }
        return parts.isEmpty() ? QStringLiteral("nessun evento oltre 60 ms") : parts.join(QStringLiteral("; "));
    }

private:
    struct Culprit
    {
        qint64 ms;
        qint64 at;
        QString desc;
        QEvent::Type type;
    };

    static QString describe(QObject* obj, QEvent* event)
    {
        if (!obj) {
            return QStringLiteral("?");
        }
        QString desc = QString::fromLatin1(obj->metaObject()->className());
        if (!obj->objectName().isEmpty()) {
            desc += QLatin1Char('/') + obj->objectName();
        }
        // I QTimer e gli oggetti anonimi si riconoscono dal genitore.
        if (QObject* parent = obj->parent()) {
            desc += QStringLiteral(" in ") + QString::fromLatin1(parent->metaObject()->className());
        }
        if (event && event->type() == QEvent::Timer) {
            desc += QStringLiteral(" timer");
        }
        return desc;
    }

    QString m_call;
    QElapsedTimer m_clock;
    qint64 m_lastStart {0};
    QEvent::Type m_lastType {QEvent::None};
    QString m_lastDesc;
    QVector<Culprit> m_culprits;
};
}  // namespace

void decodiumBridgeStartLogUiStallProbe(QObject* context, const QString& call)
{
    auto* spy = new LogStallEventSpy(call);
    QCoreApplication::instance()->installEventFilter(spy);
    auto* timer = new QTimer(context);
    timer->setTimerType(Qt::PreciseTimer);
    timer->setInterval(20);
    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();
    auto last = std::make_shared<qint64>(0);
    auto worst = std::make_shared<qint64>(0);
    auto worstAt = std::make_shared<qint64>(0);
    auto over100 = std::make_shared<int>(0);
    QObject::connect(timer, &QTimer::timeout, context, [timer, spy, clock, last, worst, worstAt, over100, call]() {
        qint64 const now = clock->elapsed();
        qint64 const gap = now - *last;
        *last = now;
        if (gap > *worst) {
            *worst = gap;
            *worstAt = now;
        }
        if (gap > 100) {
            ++*over100;
        }
        if (now >= 4000) {
            timer->stop();
            QCoreApplication::instance()->removeEventFilter(spy);
            decodiumBridgeAudioLevelLog(QStringLiteral("[LOGSTALL] %1: pausa massima dell'interfaccia %2 ms a +%3 ms dalla messa a log, pause >100 ms: %4")
                          .arg(call)
                          .arg(*worst)
                          .arg(*worstAt)
                          .arg(*over100));
            decodiumBridgeAudioLevelLog(QStringLiteral("[LOGSTALL] %1: eventi piu' lunghi: %2").arg(call, spy->report()));
            spy->deleteLater();
            timer->deleteLater();
        }
    });
    timer->start();
}

