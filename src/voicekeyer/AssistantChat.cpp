#include "AssistantChat.h"

#include "PackManager.h"

#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>

namespace decodium::voicekeyer {

namespace {

constexpr int kMaxTurns = 6;        // quanti scambi recenti tenere nel prompt
constexpr int kMaxReplyTokens = 200;
constexpr int kTimeoutMs = 240000;

const QString kSystemPrompt = QStringLiteral(
    "Sei l'assistente di Decodium, un programma per radioamatori. "
    "Rispondi nella lingua della domanda (italiano o inglese), in al massimo tre frasi. "
    "Non inventare nominativi, frequenze, orari o dati tecnici: se non sai, dillo. "
    "Non dare istruzioni per trasmettere in violazione delle regole della licenza.");

}  // namespace

QString buildChatPrompt(const QVector<ChatTurn>& history, const QString& question)
{
    QString p = QStringLiteral("<|im_start|>system\n") + kSystemPrompt + QStringLiteral("<|im_end|>\n");
    const int from = qMax(0, history.size() - kMaxTurns * 2);
    for (int i = from; i < history.size(); ++i)
        p += QStringLiteral("<|im_start|>") + history.at(i).role + QLatin1Char('\n') + history.at(i).text
             + QStringLiteral("<|im_end|>\n");
    p += QStringLiteral("<|im_start|>user\n") + question + QStringLiteral("<|im_end|>\n<|im_start|>assistant\n");
    return p;
}

QString cleanReply(const QString& raw)
{
    QString s = raw;
    static const QRegularExpression ansi(QStringLiteral("\\x1b\\[[0-9;]*[A-Za-z]"));
    s.remove(ansi);
    s.remove(QStringLiteral("[end of text]"));
    return s.trimmed();
}

AssistantChat::AssistantChat(PackManager* packs, QObject* parent)
    : QObject(parent)
    , m_packs(packs)
{
    m_status = available() ? tr("Ask a question") : tr("Assistant not installed or not selected: see Optional packs");
}

AssistantChat::~AssistantChat()
{
    cancel();
}

bool AssistantChat::available() const
{
    return m_packs->active(Pack::Assistant);
}

QString AssistantChat::transcript() const
{
    QString out;
    for (const ChatTurn& t : m_history) {
        out += (t.role == QLatin1String("user") ? tr("You: ") : tr("Decodium: ")) + t.text + QLatin1Char('\n');
    }
    return out;
}

void AssistantChat::setStatus(const QString& text)
{
    m_status = text;
    emit changed();
}

void AssistantChat::ask(const QString& question)
{
    const QString q = question.trimmed();
    if (q.isEmpty() || busy()) return;
    if (!available()) {
        setStatus(tr("Assistant not installed or not selected: see Optional packs"));
        return;
    }
    m_pendingQuestion = q;
    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, &AssistantChat::finish);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (!m_process) return;
        setStatus(tr("The assistant could not start"));
        m_process->deleteLater();
        m_process = nullptr;
        emit changed();
    });
    const QString prompt = buildChatPrompt(m_history, q);
    m_process->start(m_packs->llamaExecutable(), {QStringLiteral("-m"), m_packs->assistantModel(), QStringLiteral("-p"), prompt,
                             QStringLiteral("-n"), QString::number(kMaxReplyTokens),
                             QStringLiteral("--temp"), QStringLiteral("0.3"),
                             QStringLiteral("-c"), QStringLiteral("2048"),
                             QStringLiteral("-no-cnv"), QStringLiteral("--no-display-prompt"),
                             // Attenzione: --no-warmup insieme a --log-disable azzera l'output
                             // di llama-completion (provato): per questo qui non ci sono.
                             QStringLiteral("--simple-io")});
    setStatus(tr("Thinking…"));
    // Il timeout guarda solo questo processo: una domanda successiva non viene uccisa.
    QPointer<QProcess> started = m_process;
    QTimer::singleShot(kTimeoutMs, this, [this, started] {
        if (started && started->state() != QProcess::NotRunning) {
            started->kill();
            setStatus(tr("The assistant took too long and was stopped"));
        }
    });
}

void AssistantChat::finish(int code, QProcess::ExitStatus st)
{
    if (!m_process) return;
    const QString raw = QString::fromUtf8(m_process->readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
    m_process->deleteLater();
    m_process = nullptr;
    if (st != QProcess::NormalExit || code != 0) {
        setStatus(tr("The assistant stopped with an error: %1").arg(err.left(160)));
        return;
    }
    const QString reply = cleanReply(raw);
    if (reply.isEmpty()) {
        setStatus(tr("No answer: try a shorter question"));
        return;
    }
    m_history.append({QStringLiteral("user"), m_pendingQuestion});
    m_history.append({QStringLiteral("assistant"), reply});
    m_lastReply = reply;
    setStatus(tr("Answered"));
}

void AssistantChat::cancel()
{
    if (!m_process) return;
    m_process->kill();
    m_process->deleteLater();
    m_process = nullptr;
    setStatus(tr("Stopped"));
}

void AssistantChat::clear()
{
    if (busy()) return;
    m_history.clear();
    m_lastReply.clear();
    setStatus(tr("Conversation cleared"));
}

}  // namespace decodium::voicekeyer
