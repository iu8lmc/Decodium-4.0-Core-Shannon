#include "RTTYTerminalWidget.hpp"
#include <QFontDatabase>
#include <QScrollBar>

RTTYTerminalWidget::RTTYTerminalWidget(QWidget *parent)
    : QWidget(parent),
      summaryLabel_(nullptr),
      rxTerminal_(nullptr),
      txTerminal_(nullptr),
      txButton_(nullptr),
      macroCQ_(nullptr),
      macro73_(nullptr),
      macroCQText_(QStringLiteral("CQ CQ CQ DE <MYCALL> <MYCALL> K")),
      macro73Text_(QStringLiteral("TU 73 SK"))
{
    setupUI();
}

RTTYTerminalWidget::~RTTYTerminalWidget()
{
}

void RTTYTerminalWidget::setupUI()
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    auto monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(11);

    summaryLabel_ = new QLabel(tr("45.45 baud Baudot terminal. Received text appears above; type the text to send below."), this);
    summaryLabel_->setWordWrap(true);
    mainLayout->addWidget(summaryLabel_);

    // RX Section
    auto *rxLabel = new QLabel(tr("RTTY Received Text"), this);
    auto *clearRxButton = new QPushButton(tr("Clear RX"), this);
    auto *rxHeaderLayout = new QHBoxLayout();
    rxHeaderLayout->addWidget(rxLabel);
    rxHeaderLayout->addStretch();
    rxHeaderLayout->addWidget(clearRxButton);

    rxTerminal_ = new QPlainTextEdit(this);
    rxTerminal_->setReadOnly(true);
    rxTerminal_->setFont(monoFont);
    rxTerminal_->setLineWrapMode(QPlainTextEdit::NoWrap);
    rxTerminal_->setPlaceholderText(tr("Waiting for RTTY traffic..."));
    rxTerminal_->setStyleSheet("background-color: black; color: #00ff00;");

    mainLayout->addLayout(rxHeaderLayout);
    mainLayout->addWidget(rxTerminal_, 2);

    // TX Section
    auto *txLabel = new QLabel(tr("RTTY Transmit Buffer"), this);
    auto *clearTxButton = new QPushButton(tr("Clear TX"), this);
    auto *txHeaderLayout = new QHBoxLayout();
    txHeaderLayout->addWidget(txLabel);
    txHeaderLayout->addStretch();
    txHeaderLayout->addWidget(clearTxButton);

    txTerminal_ = new QPlainTextEdit(this);
    txTerminal_->setFont(monoFont);
    txTerminal_->setLineWrapMode(QPlainTextEdit::NoWrap);
    txTerminal_->setPlaceholderText(tr("Type your RTTY message here, then press Send RTTY."));

    // TX Controls
    auto *txCtrlLayout = new QHBoxLayout();
    txButton_ = new QPushButton(tr("Send RTTY"), this);
    txCtrlLayout->addWidget(txButton_);
    txCtrlLayout->addStretch();
    // Example macro buttons can be added here
    macroCQ_ = new QPushButton("CQ", this);
    macro73_ = new QPushButton("73", this);
    txCtrlLayout->addWidget(macroCQ_);
    txCtrlLayout->addWidget(macro73_);

    mainLayout->addLayout(txHeaderLayout);
    mainLayout->addWidget(txTerminal_, 1);
    mainLayout->addLayout(txCtrlLayout);

    // Connect signals
    connect(txButton_, &QPushButton::clicked, this, &RTTYTerminalWidget::onTxButtonClicked);
    connect(macroCQ_, &QPushButton::clicked, this, &RTTYTerminalWidget::onMacroClicked);
    connect(macro73_, &QPushButton::clicked, this, &RTTYTerminalWidget::onMacroClicked);
    connect(clearRxButton, &QPushButton::clicked, this, &RTTYTerminalWidget::clearRx);
    connect(clearTxButton, &QPushButton::clicked, this, &RTTYTerminalWidget::clearTx);

    setMacroTexts(macroCQText_, macro73Text_);
}

void RTTYTerminalWidget::setSummaryText(const QString &text)
{
    if (summaryLabel_) {
        summaryLabel_->setText(text);
    }
}

void RTTYTerminalWidget::setMacroTexts(const QString &cqText, const QString &seventyThreeText)
{
    macroCQText_ = cqText;
    macro73Text_ = seventyThreeText;

    if (macroCQ_) {
        macroCQ_->setToolTip(cqText);
    }
    if (macro73_) {
        macro73_->setToolTip(seventyThreeText);
    }
}

void RTTYTerminalWidget::appendRxText(const QString &text)
{
    // Move cursor to end and insert
    rxTerminal_->moveCursor(QTextCursor::End);
    rxTerminal_->insertPlainText(text);
    rxTerminal_->verticalScrollBar()->setValue(rxTerminal_->verticalScrollBar()->maximum());
}

void RTTYTerminalWidget::clearRx()
{
    rxTerminal_->clear();
}

void RTTYTerminalWidget::clearTx()
{
    txTerminal_->clear();
}

void RTTYTerminalWidget::onTxButtonClicked()
{
    QString txt = txTerminal_->toPlainText();
    if (!txt.isEmpty()) {
        emit transmitText(txt);
        // Optionally clear after sending or keep it
    }
}

void RTTYTerminalWidget::onMacroClicked()
{
    auto *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        if (btn == macroCQ_) {
            emit transmitText(macroCQText_);
        } else if (btn == macro73_) {
            emit transmitText(macro73Text_);
        }
    }
}
