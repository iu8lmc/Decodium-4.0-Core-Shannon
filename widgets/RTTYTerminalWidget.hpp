#ifndef RTTYTERMINALWIDGET_HPP
#define RTTYTERMINALWIDGET_HPP

#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QString>

//
// RTTYTerminalWidget
// A custom widget for Decodium3 that provides a classic split-screen 
// RTTY terminal interface for receiving and transmitting text.
//
class RTTYTerminalWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RTTYTerminalWidget(QWidget *parent = nullptr);
    ~RTTYTerminalWidget() override;

    // Output decoded text (RX)
    void appendRxText(const QString &text);
    void setSummaryText(const QString &text);
    void setMacroTexts(const QString &cqText, const QString &seventyThreeText);
    
    // Clear terminals
    void clearRx();
    void clearTx();

signals:
    // Emitted when user types to transmit or clicks a macro
    void transmitText(const QString &text);

private slots:
    void onTxButtonClicked();
    void onMacroClicked();

private:
    QLabel *summaryLabel_;
    QPlainTextEdit *rxTerminal_;
    QPlainTextEdit *txTerminal_;
    QPushButton *txButton_;
    QPushButton *macroCQ_;
    QPushButton *macro73_;
    QString macroCQText_;
    QString macro73Text_;
    
    // UI Layout helpers
    void setupUI();
};

#endif // RTTYTERMINALWIDGET_HPP
